/* lwIP configuration for Berkeley TCP Client on SAM E70 with FreeRTOS */
#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#include "FreeRTOSConfig.h"

/* --- OS/Threading --- */
#define NO_SYS                      0
#define SYS_LIGHTWEIGHT_PROT        1
#define LWIP_COMPAT_MUTEX           1

/* --- Memory --- */
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    (16 * 1024)
#define MEMP_NUM_PBUF               16
#define MEMP_NUM_UDP_PCB            4
#define MEMP_NUM_TCP_PCB            5
#define MEMP_NUM_TCP_PCB_LISTEN     4
#define MEMP_NUM_TCP_SEG            16
#define MEMP_NUM_NETBUF             8
#define MEMP_NUM_NETCONN            8
#define MEMP_NUM_SYS_TIMEOUT        8

/* --- Pbuf --- */
#define PBUF_POOL_SIZE              16
#define PBUF_POOL_BUFSIZE           1536

/* --- TCP --- */
#define LWIP_TCP                    1
#define TCP_MSS                     1460
#define TCP_WND                     (4 * TCP_MSS)
#define TCP_SND_BUF                 (2 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * TCP_SND_BUF) / TCP_MSS)
#define TCP_QUEUE_OOSEQ             1

/* --- UDP --- */
#define LWIP_UDP                    1

/* --- DHCP --- */
#define LWIP_DHCP                   1

/* --- DNS --- */
#define LWIP_DNS                    1
#define DNS_MAX_SERVERS             2

/* --- ICMP --- */
#define LWIP_ICMP                   1

/* --- ARP --- */
#define LWIP_ARP                    1
#define ARP_TABLE_SIZE              10
#define ARP_QUEUEING                1

/* --- IP --- */
#define IP_FORWARD                  0
#define IP_FRAG                     1
#define IP_REASSEMBLY               1

/* --- Socket API --- */
#define LWIP_SOCKET                 1
#define LWIP_NETCONN                1
#define LWIP_COMPAT_SOCKETS         1
#define LWIP_POSIX_SOCKETS_IO_NAMES 0
#define LWIP_SO_RCVTIMEO            1
#define LWIP_TIMEVAL_PRIVATE        0

/* --- TCPIP Thread --- */
#define TCPIP_THREAD_STACKSIZE      1024
#define TCPIP_THREAD_PRIO           (configMAX_PRIORITIES - 1)
#define TCPIP_MBOX_SIZE             16
#define DEFAULT_THREAD_STACKSIZE    512
#define DEFAULT_ACCEPTMBOX_SIZE     8
#define DEFAULT_RAW_RECVMBOX_SIZE   8
#define DEFAULT_UDP_RECVMBOX_SIZE   8
#define DEFAULT_TCP_RECVMBOX_SIZE   8

/* --- Netif --- */
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1

/* --- Statistics --- */
#define LWIP_STATS                  0
#define LWIP_STATS_DISPLAY          0

/* --- Debug --- */
#define LWIP_DEBUG                  0

/* --- Checksum --- */
#define CHECKSUM_GEN_IP             1
#define CHECKSUM_GEN_UDP            1
#define CHECKSUM_GEN_TCP            1
#define CHECKSUM_CHECK_IP           1
#define CHECKSUM_CHECK_UDP          1
#define CHECKSUM_CHECK_TCP          1

/* --- GMAC driver config (used in ethernetif.c) --- */
#define GMAC_RX_BUFFERS             8
#define GMAC_TX_BUFFERS             4
#define GMAC_TX_UNITSIZE            1536
#define GMAC_FRAME_LENTGH_MAX       1536

#define netifINTERFACE_TASK_STACK_SIZE  512
#define netifINTERFACE_TASK_PRIORITY    (configMAX_PRIORITIES - 1)

#endif /* LWIPOPTS_H */
