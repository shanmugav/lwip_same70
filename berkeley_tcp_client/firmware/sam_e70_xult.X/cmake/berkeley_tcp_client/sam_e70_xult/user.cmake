# user.cmake - lwIP TCP/IP stack integration
# Replaces Harmony TCP/IP stack with lwIP 1.4.1

set(SRC_BASE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src")
set(FREERTOS_SRC_DIR "${SRC_BASE}/third_party/rtos/FreeRTOS/Source")
set(LWIP_SRC_DIR "${SRC_BASE}/third_party/lwip/lwip-1.4.1/src")
set(LWIP_PORT_DIR "${SRC_BASE}/config/default/lwip_port")

# === FreeRTOS sources ===
set(FREERTOS_SOURCES
    "${FREERTOS_SRC_DIR}/FreeRTOS_tasks.c"
    "${FREERTOS_SRC_DIR}/queue.c"
    "${FREERTOS_SRC_DIR}/list.c"
    "${FREERTOS_SRC_DIR}/portable/GCC/SAM/CM7/port.c"
    "${FREERTOS_SRC_DIR}/portable/MemMang/heap_1.c"
)

# === lwIP core sources ===
set(LWIP_CORE_SOURCES
    "${LWIP_SRC_DIR}/core/def.c"
    "${LWIP_SRC_DIR}/core/dhcp.c"
    "${LWIP_SRC_DIR}/core/dns.c"
    "${LWIP_SRC_DIR}/core/lwip_init.c"
    "${LWIP_SRC_DIR}/core/lwip_timers_141.c"
    "${LWIP_SRC_DIR}/core/mem.c"
    "${LWIP_SRC_DIR}/core/memp.c"
    "${LWIP_SRC_DIR}/core/netif.c"
    "${LWIP_SRC_DIR}/core/pbuf.c"
    "${LWIP_SRC_DIR}/core/raw.c"
    "${LWIP_SRC_DIR}/core/stats.c"
    "${LWIP_SRC_DIR}/core/sys.c"
    "${LWIP_SRC_DIR}/core/tcp.c"
    "${LWIP_SRC_DIR}/core/tcp_in.c"
    "${LWIP_SRC_DIR}/core/tcp_out.c"
    "${LWIP_SRC_DIR}/core/udp.c"
)

set(LWIP_IPV4_SOURCES
    "${LWIP_SRC_DIR}/core/ipv4/autoip.c"
    "${LWIP_SRC_DIR}/core/ipv4/icmp.c"
    "${LWIP_SRC_DIR}/core/ipv4/igmp.c"
    "${LWIP_SRC_DIR}/core/ipv4/inet.c"
    "${LWIP_SRC_DIR}/core/ipv4/inet_chksum.c"
    "${LWIP_SRC_DIR}/core/ipv4/ip.c"
    "${LWIP_SRC_DIR}/core/ipv4/ip_addr.c"
    "${LWIP_SRC_DIR}/core/ipv4/ip_frag.c"
)

set(LWIP_API_SOURCES
    "${LWIP_SRC_DIR}/api/api_lib.c"
    "${LWIP_SRC_DIR}/api/api_msg.c"
    "${LWIP_SRC_DIR}/api/err.c"
    "${LWIP_SRC_DIR}/api/netbuf.c"
    "${LWIP_SRC_DIR}/api/netdb.c"
    "${LWIP_SRC_DIR}/api/netifapi.c"
    "${LWIP_SRC_DIR}/api/sockets.c"
    "${LWIP_SRC_DIR}/api/tcpip.c"
)

set(LWIP_NETIF_SOURCES
    "${LWIP_SRC_DIR}/netif/etharp.c"
)

# === lwIP port sources ===
set(LWIP_PORT_SOURCES
    "${LWIP_PORT_DIR}/sys_arch.c"
    "${LWIP_PORT_DIR}/ethernetif.c"
)

# === Add all sources ===
target_sources(berkeley_tcp_client_sam_e70_xult_sam_e70_xult_XC32_compile PRIVATE
    ${FREERTOS_SOURCES}
    ${LWIP_CORE_SOURCES}
    ${LWIP_IPV4_SOURCES}
    ${LWIP_API_SOURCES}
    ${LWIP_NETIF_SOURCES}
    ${LWIP_PORT_SOURCES}
)

# === Add lwIP include paths ===
target_include_directories(berkeley_tcp_client_sam_e70_xult_sam_e70_xult_XC32_compile PRIVATE
    "${LWIP_SRC_DIR}/include"
    "${LWIP_SRC_DIR}/include/ipv4"
    "${LWIP_PORT_DIR}"
    "${SRC_BASE}/config/default"
)

# === Exclude Harmony TCP/IP, GMAC, MIIM, ethphy, net_pres sources from build ===
set(HARMONY_TCPIP_EXCLUDE
    "${SRC_BASE}/config/default/library/tcpip/src/arp.c"
    "${SRC_BASE}/config/default/library/tcpip/src/berkeley_api.c"
    "${SRC_BASE}/config/default/library/tcpip/src/dhcp.c"
    "${SRC_BASE}/config/default/library/tcpip/src/dns.c"
    "${SRC_BASE}/config/default/library/tcpip/src/hash_fnv.c"
    "${SRC_BASE}/config/default/library/tcpip/src/helpers.c"
    "${SRC_BASE}/config/default/library/tcpip/src/icmp.c"
    "${SRC_BASE}/config/default/library/tcpip/src/ipv4.c"
    "${SRC_BASE}/config/default/library/tcpip/src/oahash.c"
    "${SRC_BASE}/config/default/library/tcpip/src/tcp.c"
    "${SRC_BASE}/config/default/library/tcpip/src/tcpip_announce.c"
    "${SRC_BASE}/config/default/library/tcpip/src/tcpip_commands.c"
    "${SRC_BASE}/config/default/library/tcpip/src/tcpip_heap_alloc.c"
    "${SRC_BASE}/config/default/library/tcpip/src/tcpip_heap_internal.c"
    "${SRC_BASE}/config/default/library/tcpip/src/tcpip_helpers.c"
    "${SRC_BASE}/config/default/library/tcpip/src/tcpip_manager.c"
    "${SRC_BASE}/config/default/library/tcpip/src/tcpip_notify.c"
    "${SRC_BASE}/config/default/library/tcpip/src/tcpip_packet.c"
    "${SRC_BASE}/config/default/library/tcpip/src/udp.c"
    "${SRC_BASE}/config/default/driver/gmac/src/dynamic/drv_gmac.c"
    "${SRC_BASE}/config/default/driver/gmac/src/dynamic/drv_gmac_lib_samE7x_V7x.c"
    "${SRC_BASE}/config/default/driver/miim/src/dynamic/drv_miim.c"
    "${SRC_BASE}/config/default/driver/miim/src/dynamic/drv_miim_gmac.c"
    "${SRC_BASE}/config/default/driver/ethphy/src/dynamic/drv_ethphy.c"
    "${SRC_BASE}/config/default/driver/ethphy/src/dynamic/drv_extphy_lan8740.c"
    "${SRC_BASE}/config/default/net_pres/pres/net_pres_cert_store.c"
    "${SRC_BASE}/config/default/net_pres/pres/net_pres_enc_glue.c"
    "${SRC_BASE}/config/default/net_pres/pres/src/net_pres.c"
)

set_source_files_properties(${HARMONY_TCPIP_EXCLUDE} PROPERTIES HEADER_FILE_ONLY TRUE)
