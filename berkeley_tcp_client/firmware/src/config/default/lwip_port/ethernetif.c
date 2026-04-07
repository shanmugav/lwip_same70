/*
 * GMAC (Gigabit MAC) network interface driver for lwIP.
 * Adapted from Atmel ASF lwip-port-1.4.1 same70_gmac.c
 * Rewritten to use Harmony CMSIS-style register access (GMAC_REGS->).
 */

#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "netif/etharp.h"

#include <string.h>
#include "device.h"
#include "device_cache.h"
#include "ethernetif.h"
#include "debug_log.h"

/* ---- Configuration ---- */
#define IFNAME0               'e'
#define IFNAME1               'n'
#define NET_MTU               1500
#define NET_LINK_SPEED        100000000

/* PHY address (LAN8740 on SAM E70 Xplained Ultra) */
#define PHY_ADDRESS           0

/* PHY register addresses */
#define PHY_REG_BCR           0   /* Basic Control Register */
#define PHY_REG_BSR           1   /* Basic Status Register */
#define PHY_REG_ID1           2   /* PHY Identifier 1 */
#define PHY_REG_ID2           3   /* PHY Identifier 2 */
#define PHY_REG_ANLPAR        5   /* Auto-Negotiate Link Partner Ability */

/* PHY BCR bits */
#define PHY_BCR_RESET         (1u << 15)
#define PHY_BCR_AUTONEG       (1u << 12)
#define PHY_BCR_RESTART_AN    (1u <<  9)

/* PHY BSR bits */
#define PHY_BSR_LINK_UP       (1u << 2)
#define PHY_BSR_AUTONEG_DONE  (1u << 5)

/* PHY ANLPAR bits */
#define PHY_ANLPAR_100TX_FD   (1u << 8)
#define PHY_ANLPAR_100TX_HD   (1u << 7)
#define PHY_ANLPAR_10T_FD     (1u << 6)
#define PHY_ANLPAR_10T_HD     (1u << 5)

/* GMAC interrupt group to enable */
#define GMAC_INT_GROUP  (GMAC_ISR_RCOMP_Msk | GMAC_ISR_ROVR_Msk | \
                         GMAC_ISR_HRESP_Msk | GMAC_ISR_TCOMP_Msk | \
                         GMAC_ISR_TUR_Msk   | GMAC_ISR_TFC_Msk)

#define GMAC_TX_ERRORS  (GMAC_TSR_TFC_Msk | GMAC_TSR_HRESP_Msk)
#define GMAC_RX_ERRORS  (GMAC_RSR_RXOVR_Msk | GMAC_RSR_HNO_Msk)

/* Number of GMAC queues on SAME70 */
#define GMAC_NUM_QUEUES 6

/* ---- DMA Descriptor Definitions ---- */
/* RX descriptor address word bits */
#define GMAC_RXD_OWNERSHIP   (1u << 0)
#define GMAC_RXD_WRAP        (1u << 1)
#define GMAC_RXD_ADDR_MASK   0xFFFFFFFCu

/* RX descriptor status word bits */
#define GMAC_RXD_LEN_MASK    0x1FFFu

/* TX descriptor status word bits */
#define GMAC_TXD_USED         (1u << 31)
#define GMAC_TXD_WRAP         (1u << 30)
#define GMAC_TXD_LAST         (1u << 15)
#define GMAC_TXD_LEN_MASK     0x3FFFu

typedef struct {
    volatile uint32_t addr;
    volatile uint32_t status;
} gmac_rx_descriptor_t;

typedef struct {
    volatile uint32_t addr;
    volatile uint32_t status;
} gmac_tx_descriptor_t;

/* ---- GMAC driver instance ---- */
struct gmac_device {
    gmac_rx_descriptor_t rx_desc[GMAC_RX_BUFFERS] __attribute__((aligned(8)));
    gmac_tx_descriptor_t tx_desc[GMAC_TX_BUFFERS] __attribute__((aligned(8)));
    struct pbuf *rx_pbuf[GMAC_RX_BUFFERS];
    uint8_t tx_buf[GMAC_TX_BUFFERS][GMAC_TX_UNITSIZE];
    uint32_t us_rx_idx;
    uint32_t us_tx_idx;
    struct netif *netif;
    sys_sem_t rx_sem;
};

static struct gmac_device gs_gmac_dev __attribute__((aligned(8)));

/* Null descriptors for unused priority queues */
static gmac_tx_descriptor_t gs_tx_desc_null __attribute__((aligned(8)));
static gmac_rx_descriptor_t gs_rx_desc_null __attribute__((aligned(8)));

/* Global netif instance */
struct netif g_netif;

/* Diagnostic: count available RX descriptors */
uint32_t gmac_rx_desc_available(void)
{
    uint32_t avail = 0;
    for (uint32_t d = 0; d < GMAC_RX_BUFFERS; d++) {
        DCACHE_CLEAN_INVALIDATE_BY_ADDR((uint32_t *)&gs_gmac_dev.rx_desc[d], 8);
        if (!(gs_gmac_dev.rx_desc[d].addr & GMAC_RXD_OWNERSHIP)) avail++;
    }
    return avail;
}

uint32_t gmac_rx_desc0_addr(void) {
    DCACHE_CLEAN_INVALIDATE_BY_ADDR((uint32_t *)&gs_gmac_dev.rx_desc[0], 8);
    return gs_gmac_dev.rx_desc[0].addr;
}
uint32_t gmac_rx_desc0_status(void) {
    return gs_gmac_dev.rx_desc[0].status;
}

/* MAC address */
static uint8_t gs_uc_mac_address[] = { 0x00, 0x04, 0x25, 0x1C, 0xA0, 0x02 };

/* ---- GMAC data path debug counters (volatile for ISR safety) ---- */
volatile uint32_t dbg_isr_count = 0;
volatile uint32_t dbg_isr_rcomp = 0;
volatile uint32_t dbg_isr_other = 0;
volatile uint32_t dbg_tx_count = 0;
volatile uint32_t dbg_tx_bytes = 0;
volatile uint32_t dbg_rx_count = 0;
volatile uint32_t dbg_rx_bytes = 0;
volatile uint32_t dbg_rx_none = 0;  /* gmac_task woke but no packet */

/* ---- PHY MIIM access via GMAC_MAN register ---- */

static void mdio_wait_idle(void)
{
    while (!(GMAC_REGS->GMAC_NSR & GMAC_NSR_IDLE_Msk)) {
        /* Wait for MDIO idle */
    }
}

uint16_t mdio_read(uint8_t phy_addr, uint8_t reg_addr)
{
    mdio_wait_idle();
    GMAC_REGS->GMAC_MAN = GMAC_MAN_CLTTO_Msk |  /* Clause 22 */
                           GMAC_MAN_OP(2) |        /* Read operation */
                           GMAC_MAN_WTN(2) |       /* Must be 10 */
                           GMAC_MAN_PHYA(phy_addr) |
                           GMAC_MAN_REGA(reg_addr);
    mdio_wait_idle();
    return (uint16_t)(GMAC_REGS->GMAC_MAN & GMAC_MAN_DATA_Msk);
}

static void mdio_write(uint8_t phy_addr, uint8_t reg_addr, uint16_t value)
{
    mdio_wait_idle();
    GMAC_REGS->GMAC_MAN = GMAC_MAN_CLTTO_Msk |
                           GMAC_MAN_OP(1) |        /* Write operation */
                           GMAC_MAN_WTN(2) |
                           GMAC_MAN_PHYA(phy_addr) |
                           GMAC_MAN_REGA(reg_addr) |
                           GMAC_MAN_DATA(value);
    mdio_wait_idle();
}

/* Simple busy-wait delay (~1ms per call at 300MHz) */
static void phy_delay_ms(uint32_t ms)
{
    volatile uint32_t count;
    for (; ms > 0; ms--) {
        for (count = 0; count < 75000; count++) { }
    }
}

static int phy_init_and_autoneg(void)
{
    uint32_t timeout;
    uint16_t val;

    DBG_LOG("[PHY-01] Enabling GMAC management port (MPE)");
    GMAC_REGS->GMAC_NCR |= GMAC_NCR_MPE_Msk;

    /* Set MDC clock divider (CLK=4 => MCK/96, good for 300MHz/2=150MHz periph clock) */
    GMAC_REGS->GMAC_NCFGR = (GMAC_REGS->GMAC_NCFGR & ~GMAC_NCFGR_CLK_Msk) | GMAC_NCFGR_CLK(4);
    DBG_LOG("[PHY-02] MDC clock divider set");

    /* Read PHY ID to verify MDIO works */
    val = mdio_read(PHY_ADDRESS, PHY_REG_ID1);
    DBG_LOG_VAL("[PHY-03] PHY ID1:", val);
    val = mdio_read(PHY_ADDRESS, PHY_REG_ID2);
    DBG_LOG_VAL("[PHY-04] PHY ID2:", val);

    if (mdio_read(PHY_ADDRESS, PHY_REG_ID1) == 0xFFFF) {
        DBG_LOG("[PHY-!!] MDIO read returns 0xFFFF - PHY not responding!");
        return -1;
    }

    /* Reset PHY */
    DBG_LOG("[PHY-05] Resetting PHY...");
    mdio_write(PHY_ADDRESS, PHY_REG_BCR, PHY_BCR_RESET);

    /* Wait for reset to complete - KSZ8061 needs up to 500ms */
    phy_delay_ms(10);  /* Brief delay before polling */
    for (timeout = 0; timeout < 1000; timeout++) {
        val = mdio_read(PHY_ADDRESS, PHY_REG_BCR);
        if (!(val & PHY_BCR_RESET)) break;
        phy_delay_ms(1);
    }
    DBG_LOG_VAL("[PHY-06] PHY reset complete, BCR:", val);

    /* Brief settle time after reset */
    phy_delay_ms(50);

    /* Start auto-negotiation */
    DBG_LOG("[PHY-07] Starting auto-negotiation...");
    mdio_write(PHY_ADDRESS, PHY_REG_BCR, PHY_BCR_AUTONEG | PHY_BCR_RESTART_AN);

    /* Wait for auto-negotiation complete - can take 3-5 seconds */
    for (timeout = 0; timeout < 5000; timeout++) {
        val = mdio_read(PHY_ADDRESS, PHY_REG_BSR);
        if (val & PHY_BSR_AUTONEG_DONE) break;
        phy_delay_ms(1);
        if (timeout % 1000 == 999) {
            DBG_LOG_VAL("[PHY-07a] Still waiting... BSR:", val);
        }
    }
    DBG_LOG_VAL("[PHY-08] BSR after autoneg:", val);

    if (!(val & PHY_BSR_AUTONEG_DONE)) {
        DBG_LOG("[PHY-!!] Auto-negotiation TIMEOUT (5s)");
        /* Fall through - try to check link anyway */
    }

    /* Check link (read BSR twice - link status is latched-low) */
    val = mdio_read(PHY_ADDRESS, PHY_REG_BSR);
    val = mdio_read(PHY_ADDRESS, PHY_REG_BSR);
    DBG_LOG_VAL("[PHY-08b] BSR (re-read for link):", val);

    if (!(val & PHY_BSR_LINK_UP)) {
        DBG_LOG("[PHY-!!] Link NOT up - check Ethernet cable!");
        return -1;
    }
    DBG_LOG("[PHY-09] Link is UP");

    /* Read link partner ability to set GMAC speed/duplex */
    val = mdio_read(PHY_ADDRESS, PHY_REG_ANLPAR);
    DBG_LOG_VAL("[PHY-10] ANLPAR (link partner ability):", val);
    if (val & PHY_ANLPAR_100TX_FD) {
        GMAC_REGS->GMAC_NCFGR |= GMAC_NCFGR_SPD_Msk | GMAC_NCFGR_FD_Msk;
        DBG_LOG("[PHY-11] Configured: 100Mbps Full Duplex");
    } else if (val & PHY_ANLPAR_100TX_HD) {
        GMAC_REGS->GMAC_NCFGR = (GMAC_REGS->GMAC_NCFGR | GMAC_NCFGR_SPD_Msk) & ~GMAC_NCFGR_FD_Msk;
        DBG_LOG("[PHY-11] Configured: 100Mbps Half Duplex");
    } else if (val & PHY_ANLPAR_10T_FD) {
        GMAC_REGS->GMAC_NCFGR = (GMAC_REGS->GMAC_NCFGR & ~GMAC_NCFGR_SPD_Msk) | GMAC_NCFGR_FD_Msk;
        DBG_LOG("[PHY-11] Configured: 10Mbps Full Duplex");
    } else {
        GMAC_REGS->GMAC_NCFGR &= ~(GMAC_NCFGR_SPD_Msk | GMAC_NCFGR_FD_Msk);
        DBG_LOG("[PHY-11] Configured: 10Mbps Half Duplex");
    }

    return 0;
}

/* ---- RX/TX descriptor management ---- */

static uint32_t rx_populate_count = 0;

static void gmac_rx_populate_queue(struct gmac_device *p_dev)
{
    uint32_t i;
    struct pbuf *p;
    uint32_t allocated = 0;

    for (i = 0; i < GMAC_RX_BUFFERS; i++) {
        if (p_dev->rx_pbuf[i] == NULL) {
            p = pbuf_alloc(PBUF_RAW, (u16_t)GMAC_FRAME_LENTGH_MAX, PBUF_POOL);
            if (p == NULL) {
                if (rx_populate_count == 0) {
                    DBG_LOG("[RX-!!] pbuf_alloc FAILED on initial populate!");
                }
                break;
            }
            allocated++;

            if (i == GMAC_RX_BUFFERS - 1)
                p_dev->rx_desc[i].addr = ((uint32_t)p->payload & GMAC_RXD_ADDR_MASK) | GMAC_RXD_WRAP;
            else
                p_dev->rx_desc[i].addr = (uint32_t)p->payload & GMAC_RXD_ADDR_MASK;

            p_dev->rx_desc[i].status = 0;
            /* Clean cache so GMAC DMA sees the descriptor update */
            DCACHE_CLEAN_BY_ADDR((uint32_t *)&p_dev->rx_desc[i], sizeof(gmac_rx_descriptor_t));
            p_dev->rx_pbuf[i] = p;
        }
    }
    if (rx_populate_count == 0 && allocated > 0) {
        DBG_LOG_VAL("[RX-01] Initial RX pbufs allocated:", allocated);
    }
    rx_populate_count++;
}

static void gmac_rx_init(struct gmac_device *p_dev)
{
    uint32_t i;
    p_dev->us_rx_idx = 0;

    for (i = 0; i < GMAC_RX_BUFFERS; i++) {
        p_dev->rx_pbuf[i] = NULL;
        p_dev->rx_desc[i].addr = 0;
        p_dev->rx_desc[i].status = 0;
    }
    p_dev->rx_desc[GMAC_RX_BUFFERS - 1].addr |= GMAC_RXD_WRAP;

    gmac_rx_populate_queue(p_dev);

    GMAC_REGS->GMAC_RBQB = (uint32_t)&p_dev->rx_desc[0];
}

static void gmac_tx_init(struct gmac_device *p_dev)
{
    uint32_t i;
    p_dev->us_tx_idx = 0;

    for (i = 0; i < GMAC_TX_BUFFERS; i++) {
        p_dev->tx_desc[i].addr = (uint32_t)&p_dev->tx_buf[i][0];
        p_dev->tx_desc[i].status = GMAC_TXD_USED | GMAC_TXD_LAST;
    }
    p_dev->tx_desc[GMAC_TX_BUFFERS - 1].status |= GMAC_TXD_WRAP;

    GMAC_REGS->GMAC_TBQB = (uint32_t)&p_dev->tx_desc[0];
}

/* ---- Low-level GMAC init ---- */

static void gmac_low_level_init(struct netif *netif)
{
    uint32_t i;

    DBG_LOG("[GMAC-01] gmac_low_level_init() enter");

    /* Set MAC hardware address */
    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, gs_uc_mac_address, 6);

    netif->mtu = NET_MTU;
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    DBG_LOG("[GMAC-02] Disabling TX/RX and interrupts");
    /* Disable TX & RX and all interrupts */
    GMAC_REGS->GMAC_NCR = 0;
    GMAC_REGS->GMAC_IDR = ~0u;

    /* Clear statistics */
    GMAC_REGS->GMAC_NCR |= GMAC_NCR_CLRSTAT_Msk;

    /* Clear all status bits */
    GMAC_REGS->GMAC_RSR = GMAC_RSR_BNA_Msk | GMAC_RSR_REC_Msk |
                           GMAC_RSR_RXOVR_Msk | GMAC_RSR_HNO_Msk;
    GMAC_REGS->GMAC_TSR = GMAC_TSR_UBR_Msk | GMAC_TSR_COL_Msk |
                           GMAC_TSR_RLE_Msk | GMAC_TSR_TXGO_Msk |
                           GMAC_TSR_TFC_Msk | GMAC_TSR_TXCOMP_Msk |
                           GMAC_TSR_HRESP_Msk;

    /* Read ISR to clear pending interrupts */
    (void)GMAC_REGS->GMAC_ISR;

    /* Configure NCFGR to match Harmony: SPD, FD, CLK=4, PEN, RFCS */
    GMAC_REGS->GMAC_NCFGR = GMAC_NCFGR_SPD(1) | GMAC_NCFGR_FD(1) |
                              GMAC_NCFGR_CLK(4) | GMAC_NCFGR_PEN(1) |
                              GMAC_NCFGR_RFCS(1);

    /* Configure DCFGR to match Harmony: DRBS=0x18, RXBMS=3, TXPBMS, FBLDO=INCR4, DDRP */
    GMAC_REGS->GMAC_DCFGR = GMAC_DCFGR_DRBS(0x18) |
                              GMAC_DCFGR_RXBMS(3) | GMAC_DCFGR_TXPBMS_Msk |
                              GMAC_DCFGR_FBLDO(4) | GMAC_DCFGR_DDRP_Msk;

    /* Set RMII mode: GMAC_UR RMII bit = 0 for RMII, 1 for MII (inverted logic!) */
    GMAC_REGS->GMAC_UR = 0;

    /* Clear priority queue interrupts and set null descriptors for unused queues */
    gs_tx_desc_null.addr = 0xFFFFFFFF;
    gs_tx_desc_null.status = GMAC_TXD_WRAP | GMAC_TXD_USED;
    gs_rx_desc_null.addr = (0xFFFFFFFF & GMAC_RXD_ADDR_MASK) | GMAC_RXD_WRAP;
    gs_rx_desc_null.status = 0;

    for (i = 1; i < GMAC_NUM_QUEUES; i++) {
        (void)GMAC_REGS->GMAC_ISRPQ[i - 1];  /* Read to clear */
        GMAC_REGS->GMAC_TBQBAPQ[i - 1] = (uint32_t)&gs_tx_desc_null;
        GMAC_REGS->GMAC_RBQBAPQ[i - 1] = (uint32_t)&gs_rx_desc_null;
    }

    /* Set MAC address in GMAC specific address register 1 */
    GMAC_REGS->GMAC_SA[0].GMAC_SAB =
        ((uint32_t)netif->hwaddr[3] << 24) |
        ((uint32_t)netif->hwaddr[2] << 16) |
        ((uint32_t)netif->hwaddr[1] << 8)  |
        ((uint32_t)netif->hwaddr[0]);
    GMAC_REGS->GMAC_SA[0].GMAC_SAT =
        ((uint32_t)netif->hwaddr[5] << 8) |
        ((uint32_t)netif->hwaddr[4]);
    DBG_LOG("[GMAC-03] MAC address set");

    /* Enable GMAC NVIC interrupt (priority=4, safe for FreeRTOS FromISR) */
    NVIC_SetPriority(GMAC_IRQn, 4);
    NVIC_EnableIRQ(GMAC_IRQn);

    /* Init PHY and auto-negotiate FIRST (before enabling RX) */
    DBG_LOG("[GMAC-04] Starting PHY init and auto-negotiation...");
    if (phy_init_and_autoneg() != 0) {
        DBG_LOG("[GMAC-!!] PHY init/autoneg FAILED");
    } else {
        DBG_LOG("[GMAC-05] PHY init/autoneg OK");
    }

    /* NOW set up DMA descriptor rings (after PHY is ready) */
    DBG_LOG("[GMAC-06] Initializing RX/TX descriptor rings");
    gmac_rx_init(&gs_gmac_dev);
    gmac_tx_init(&gs_gmac_dev);

    /* Clean ALL descriptors and pbuf data to RAM so DMA sees correct data */
    DCACHE_CLEAN_BY_ADDR((uint32_t *)&gs_gmac_dev, sizeof(gs_gmac_dev));
    DBG_LOG_VAL("[GMAC-07] RX desc base:", (uint32_t)&gs_gmac_dev.rx_desc[0]);
    DBG_LOG_VAL("[GMAC-08] TX desc base:", (uint32_t)&gs_gmac_dev.tx_desc[0]);

    /* Enable TX, RX, and statistics write AFTER descriptors and PHY are ready */
    GMAC_REGS->GMAC_NCR |= GMAC_NCR_TXEN_Msk | GMAC_NCR_RXEN_Msk | GMAC_NCR_WESTAT_Msk;
    DBG_LOG("[GMAC-09] TX/RX enabled");

    /* Enable GMAC interrupts */
    GMAC_REGS->GMAC_IER = GMAC_INT_GROUP;
    DBG_LOG("[GMAC-10] GMAC interrupts enabled");

    DBG_LOG_VAL("[GMAC-11] GMAC_NCR:", GMAC_REGS->GMAC_NCR);
    DBG_LOG_VAL("[GMAC-12] GMAC_NCFGR:", GMAC_REGS->GMAC_NCFGR);
    DBG_LOG_VAL("[GMAC-13] GMAC_RBQB:", GMAC_REGS->GMAC_RBQB);
    DBG_LOG("[GMAC-14] gmac_low_level_init() complete");
}

/* ---- TX ---- */

static err_t gmac_low_level_output(struct netif *netif, struct pbuf *p)
{
    struct gmac_device *ps_dev = netif->state;
    struct pbuf *q;
    uint8_t *buffer;

    /* Handle TX errors */
    if (GMAC_REGS->GMAC_TSR & GMAC_TX_ERRORS) {
        GMAC_REGS->GMAC_NCR &= ~GMAC_NCR_TXEN_Msk;
        LINK_STATS_INC(link.err);
        LINK_STATS_INC(link.drop);
        gmac_tx_init(ps_dev);
        GMAC_REGS->GMAC_TSR = GMAC_TX_ERRORS;
        GMAC_REGS->GMAC_NCR |= GMAC_NCR_TXEN_Msk;
    }

    buffer = (uint8_t *)ps_dev->tx_desc[ps_dev->us_tx_idx].addr;

    /* Copy pbuf chain into TX buffer */
    for (q = p; q != NULL; q = q->next) {
        memcpy(buffer, q->payload, q->len);
        buffer += q->len;
    }

    /* Clean cache for TX buffer data so DMA reads correct content */
    DCACHE_CLEAN_BY_ADDR((uint32_t *)ps_dev->tx_desc[ps_dev->us_tx_idx].addr, p->tot_len);

    /* Set length and clear used bit to hand to GMAC */
    ps_dev->tx_desc[ps_dev->us_tx_idx].status =
        (ps_dev->tx_desc[ps_dev->us_tx_idx].status & GMAC_TXD_WRAP) |
        GMAC_TXD_LAST | (p->tot_len & GMAC_TXD_LEN_MASK);

    /* Clean cache for TX descriptor so DMA sees the updated status */
    DCACHE_CLEAN_BY_ADDR((uint32_t *)&ps_dev->tx_desc[ps_dev->us_tx_idx], sizeof(gmac_tx_descriptor_t));

    ps_dev->us_tx_idx = (ps_dev->us_tx_idx + 1) % GMAC_TX_BUFFERS;

    /* Start transmission */
    GMAC_REGS->GMAC_NCR |= GMAC_NCR_TSTART_Msk;

    dbg_tx_count++;
    dbg_tx_bytes += p->tot_len;
    if (dbg_tx_count <= 5) {
        DBG_LOG_VAL("[TX] Packet sent, len:", p->tot_len);
        DBG_LOG_VAL("[TX] TSR after send:", GMAC_REGS->GMAC_TSR);
    }

    LINK_STATS_INC(link.xmit);
    return ERR_OK;
}

/* ---- RX ---- */

static struct pbuf *gmac_low_level_input(struct netif *netif)
{
    struct gmac_device *ps_dev = netif->state;
    struct pbuf *p = NULL;
    uint32_t length;
    uint32_t i;
    gmac_rx_descriptor_t *p_rx = &ps_dev->rx_desc[ps_dev->us_rx_idx];

    /* Handle RX errors */
    if (GMAC_REGS->GMAC_RSR & GMAC_RX_ERRORS) {
        GMAC_REGS->GMAC_NCR &= ~GMAC_NCR_RXEN_Msk;
        LINK_STATS_INC(link.err);
        LINK_STATS_INC(link.drop);

        for (i = 0; i < GMAC_RX_BUFFERS; i++) {
            if (ps_dev->rx_pbuf[i] != NULL) {
                pbuf_free(ps_dev->rx_pbuf[i]);
                ps_dev->rx_pbuf[i] = NULL;
            }
        }
        gmac_rx_init(ps_dev);
        GMAC_REGS->GMAC_RSR = GMAC_RX_ERRORS;
        GMAC_REGS->GMAC_NCR |= GMAC_NCR_RXEN_Msk;
    }

    /* Clean+Invalidate cache for RX descriptor (safe for unaligned addresses) */
    DCACHE_CLEAN_INVALIDATE_BY_ADDR((uint32_t *)p_rx, sizeof(gmac_rx_descriptor_t));

    /* Check ownership bit - set by GMAC when frame received */
    if (p_rx->addr & GMAC_RXD_OWNERSHIP) {
        length = p_rx->status & GMAC_RXD_LEN_MASK;

        /* Fetch pre-allocated pbuf */
        p = ps_dev->rx_pbuf[ps_dev->us_rx_idx];

        /* Clean+Invalidate cache for received packet data (safe for unaligned pbuf payload) */
        DCACHE_CLEAN_INVALIDATE_BY_ADDR((uint32_t *)p->payload, GMAC_FRAME_LENTGH_MAX);

        p->len = length;
        p->tot_len = length;

        /* Remove pbuf from descriptor */
        ps_dev->rx_pbuf[ps_dev->us_rx_idx] = NULL;

        LINK_STATS_INC(link.recv);

        /* Refill empty descriptors (sets new addr with ownership=0 and cleans cache) */
        gmac_rx_populate_queue(ps_dev);

        /* DO NOT touch p_rx->addr here - gmac_rx_populate_queue already set
         * ownership=0 and cleaned the cache. Writing here would make the cache
         * line dirty, causing a subsequent CLEAN to overwrite GMAC DMA updates. */

        ps_dev->us_rx_idx = (ps_dev->us_rx_idx + 1) % GMAC_RX_BUFFERS;

        dbg_rx_count++;
        dbg_rx_bytes += length;
        if (dbg_rx_count <= 5) {
            DBG_LOG_VAL("[RX] Packet received, len:", length);
        }
    } else {
        dbg_rx_none++;
    }

    return p;
}

/* ---- GMAC RX task ---- */

static void gmac_task(void *pvParameters)
{
    struct gmac_device *ps_dev = pvParameters;

    while (1) {
        /* Wait for RX interrupt (short timeout for fast polling) */
        sys_arch_sem_wait(&ps_dev->rx_sem, 2);

        /* Drain ALL available packets from the descriptor ring */
        struct pbuf *p;
        while ((p = gmac_low_level_input(ps_dev->netif)) != NULL) {
            struct eth_hdr *ethhdr = p->payload;
            switch (htons(ethhdr->type)) {
            case ETHTYPE_IP:
            case ETHTYPE_ARP:
                if (ps_dev->netif->input(p, ps_dev->netif) != ERR_OK) {
                    pbuf_free(p);
                }
                break;
            default:
                pbuf_free(p);
                break;
            }
        }
    }
}

/* ---- ethernetif_input: dispatch received frame to lwIP ---- */

void ethernetif_input(struct netif *netif)
{
    struct eth_hdr *ethhdr;
    struct pbuf *p;

    p = gmac_low_level_input(netif);
    if (p == NULL) return;

    ethhdr = p->payload;

    switch (htons(ethhdr->type)) {
    case ETHTYPE_IP:
    case ETHTYPE_ARP:
        if (netif->input(p, netif) != ERR_OK) {
            pbuf_free(p);
        }
        break;
    default:
        pbuf_free(p);
        break;
    }
}

/* ---- GMAC interrupt handler (name matches Harmony vector table) ---- */

void GMAC_InterruptHandler(void)
{
    volatile uint32_t isr;
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    isr = GMAC_REGS->GMAC_ISR;  /* Read clears */
    dbg_isr_count++;

    if (isr & (GMAC_ISR_RCOMP_Msk | GMAC_ISR_ROVR_Msk)) {
        dbg_isr_rcomp++;
        xSemaphoreGiveFromISR(gs_gmac_dev.rx_sem, &xHigherPriorityTaskWoken);
    } else {
        dbg_isr_other++;
    }

    portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}

/* ---- ethernetif_init: called by netif_add() ---- */

err_t ethernetif_init(struct netif *netif)
{
    DBG_LOG("[EINIT-01] ethernetif_init() enter");
    LWIP_ASSERT("netif != NULL", (netif != NULL));

    gs_gmac_dev.netif = netif;

#if LWIP_NETIF_HOSTNAME
    netif->hostname = "MCHPBOARD_C";
#endif

    netif->state = &gs_gmac_dev;
    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    netif->output = etharp_output;
    netif->linkoutput = gmac_low_level_output;

    DBG_LOG("[EINIT-02] Calling gmac_low_level_init()...");
    gmac_low_level_init(netif);
    DBG_LOG("[EINIT-03] gmac_low_level_init() returned");

    /* Create RX notification semaphore */
    DBG_LOG("[EINIT-04] Creating RX semaphore...");
    err_t err = sys_sem_new(&gs_gmac_dev.rx_sem, 0);
    if (err != ERR_OK) {
        DBG_LOG("[EINIT-!!] RX semaphore creation FAILED");
        return ERR_MEM;
    }
    DBG_LOG("[EINIT-05] RX semaphore created");

    /* Create GMAC RX processing task */
    DBG_LOG("[EINIT-06] Creating GMAC RX task...");
    sys_thread_t id = sys_thread_new("GMAC", gmac_task, &gs_gmac_dev,
                                     netifINTERFACE_TASK_STACK_SIZE,
                                     netifINTERFACE_TASK_PRIORITY);
    if (id == NULL) {
        DBG_LOG("[EINIT-!!] GMAC RX task creation FAILED");
        return ERR_MEM;
    }
    DBG_LOG("[EINIT-07] GMAC RX task created");
    DBG_LOG("[EINIT-08] ethernetif_init() complete OK");

    return ERR_OK;
}

/* ---- lwip_network_init: called from SYS_Initialize() ---- */

void lwip_network_init(void)
{
    ip_addr_t ipaddr, netmask, gw;

    DBG_LOG("[LWIP-01] lwip_network_init() enter");

    /* Initialize lwIP stack (starts tcpip_thread) */
    DBG_LOG("[LWIP-02] Calling tcpip_init()...");
    tcpip_init(NULL, NULL);
    DBG_LOG("[LWIP-03] tcpip_init() returned OK");

    /* Start with 0.0.0.0 - DHCP will assign the real IP.
     * If DHCP fails, set a static IP manually later. */
    IP4_ADDR(&ipaddr,  0, 0, 0, 0);
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    IP4_ADDR(&gw,      0, 0, 0, 0);

    DBG_LOG("[LWIP-04] Calling netif_add() -> ethernetif_init()...");
    netif_add(&g_netif, &ipaddr, &netmask, &gw, NULL, ethernetif_init, tcpip_input);
    DBG_LOG("[LWIP-05] netif_add() returned");

    netif_set_default(&g_netif);
    netif_set_up(&g_netif);
    DBG_LOG("[LWIP-06] netif set as default and UP");

    DBG_LOG("[LWIP-07] Starting DHCP...");
    dhcp_start(&g_netif);
    DBG_LOG("[LWIP-08] DHCP started");

    DBG_LOG("[LWIP-09] lwip_network_init() complete");
}
