/* GMAC network interface driver for lwIP */
#ifndef ETHERNETIF_H
#define ETHERNETIF_H

#include "lwip/netif.h"
#include "lwip/err.h"

err_t ethernetif_init(struct netif *netif);
void  ethernetif_input(struct netif *netif);
void  lwip_network_init(void);

/* Global netif instance accessible by app code */
extern struct netif g_netif;

#endif /* ETHERNETIF_H */
