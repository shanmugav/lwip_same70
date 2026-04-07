# Porting Berkeley TCP Client from Harmony TCP/IP to lwIP 1.4.1

## Overview
This document describes the step-by-step process for replacing the Microchip Harmony TCP/IP stack with lwIP 1.4.1 on the SAM E70 Xplained Ultra (ATSAME70Q21B) berkeley_tcp_client project.

**Key decisions:**
- lwIP configured with `NO_SYS=0` (FreeRTOS), `LWIP_SOCKET=1` (Berkeley socket API)
- GMAC driver adapted to use Harmony register definitions (`GMAC_REGS->`) instead of ASF functions
- `LWIP_COMPAT_SOCKETS=1` so `socket()`, `connect()`, `send()`, `recv()`, `closesocket()` work transparently

---

## Step 1: Copy lwIP Source Tree

**Source:** Atmel Studio example at `ASF\thirdparty\lwip\lwip-1.4.1\src\`
**Destination:** `src/third_party/lwip/lwip-1.4.1/src/`

Copy:
- `api/` - 8 files (sockets.c, api_lib.c, api_msg.c, tcpip.c, err.c, netbuf.c, netdb.c, netifapi.c)
- `core/` - 16 files (tcp.c, tcp_in.c, tcp_out.c, udp.c, mem.c, memp.c, pbuf.c, netif.c, dhcp.c, dns.c, lwip_init.c, lwip_timers_141.c, raw.c, stats.c, sys.c, def.c)
- `core/ipv4/` - 8 files (ip.c, ip_addr.c, ip_frag.c, icmp.c, igmp.c, inet.c, inet_chksum.c, autoip.c)
- `netif/etharp.c`
- `include/` - entire header tree

## Step 2: Create lwIP Port Layer

### 2.1: `src/config/default/lwip_port/arch/cc.h`
Compiler abstraction. Defines lwIP types (u8_t, u16_t, etc.), struct packing macros, `LWIP_PROVIDE_ERRNO`, `LWIP_COMPAT_MUTEX=1`. Includes `<sys/time.h>` for `struct timeval` compatibility. Guards `BYTE_ORDER` with `#ifndef` to avoid redefinition with XC32 musl.

### 2.2: `src/config/default/lwip_port/arch/perf.h`
Null performance measurement stubs.

### 2.3: `src/config/default/lwip_port/arch/sys_arch.h`
FreeRTOS type definitions for lwIP OS abstraction:
- `sys_sem_t` = `SemaphoreHandle_t`
- `sys_mbox_t` = `QueueHandle_t`
- `sys_thread_t` = `TaskHandle_t`

### 2.4: `src/config/default/lwip_port/sys_arch.c`
FreeRTOS implementation mapping:
- Semaphores -> `xSemaphoreCreateCounting`, `xSemaphoreTake/Give`
- Mailboxes -> `xQueueCreate`, `xQueueSend/Receive`
- Threads -> `xTaskCreate`
- `sys_now()` -> `xTaskGetTickCount() * portTICK_PERIOD_MS`
- Protection -> `taskENTER_CRITICAL/EXIT_CRITICAL`

### 2.5: `src/config/default/lwip_port/ethernetif.c` (most complex)
GMAC network interface driver adapted from ASF `same70_gmac.c`:
- All ASF function calls replaced with `GMAC_REGS->register_name` access
- DMA descriptor types defined locally (gmac_rx/tx_descriptor_t)
- PHY init via direct MIIM register access (`GMAC_REGS->GMAC_MAN`) for LAN8740
- ISR named `GMAC_InterruptHandler` to match Harmony vector table
- `lwip_network_init()` function: calls `tcpip_init()`, `netif_add()`, `dhcp_start()`

### 2.6: `src/config/default/lwip_port/ethernetif.h`
Declares `ethernetif_init()`, `lwip_network_init()`, and `extern struct netif g_netif`.

## Step 3: Create `lwipopts.h`

**File:** `src/config/default/lwipopts.h`

Critical settings:
- `NO_SYS=0`, `LWIP_SOCKET=1`, `LWIP_NETCONN=1`
- `LWIP_COMPAT_SOCKETS=1`, `LWIP_POSIX_SOCKETS_IO_NAMES=0`
- `LWIP_TIMEVAL_PRIVATE=0` (use system's `struct timeval`)
- `LWIP_DHCP=1`, `LWIP_DNS=1`
- `MEM_SIZE=16KB`, `PBUF_POOL_SIZE=16`, `TCP_MSS=1460`
- `TCPIP_THREAD_STACKSIZE=1024`, `TCPIP_THREAD_PRIO=configMAX_PRIORITIES-1`
- `GMAC_RX_BUFFERS=8`, `GMAC_TX_BUFFERS=4`

## Step 4: Modify System Files

### 4.1: `initialization.c`
- Wrapped Harmony TCP/IP init data in `#if 0` block
- Removed `DRV_MIIM_Initialize()`, `NET_PRES_Initialize()`, `TCPIP_STACK_Init()` calls
- Added `lwip_network_init()` call

### 4.2: `tasks.c`
- Removed `_TCPIP_STACK_Task`, `_DRV_MIIM_Task`, `_NET_PRES_Tasks` functions and their `xTaskCreate` calls
- Kept `lSYS_CMD_Tasks` and `lAPP_Tasks`

### 4.3: `definitions.h`
- Replaced Harmony TCP/IP includes with lwIP includes
- Removed `tcpip`, `drvMiim_0`, `netPres` from `SYSTEM_OBJECTS`

### 4.4: `FreeRTOSConfig.h`
- Increased `configTOTAL_HEAP_SIZE` from 40960 to 65536

## Step 5: Modify Application Code

### 5.1: `app.h`
- Replaced `system_config.h`/`system_definitions.h` with `definitions.h`
- Changed `SOCKET socket` to `int socket`

### 5.2: `app.c`
- Replaced Harmony includes with lwIP includes
- `APP_WAITING_FOR_INITIALIZATION`: `netif_is_link_up(&g_netif)` instead of `TCPIP_STACK_Status()`
- `APP_TCPIP_WAIT_FOR_IP`: `g_netif.ip_addr.addr != 0` instead of `TCPIP_STACK_NetIsReady()`
- DNS: `hostInfo->h_addr` with `s_addr` instead of `S_un.S_addr` and `IPV4_ADDR`
- Socket: `-1` instead of `SOCKET_ERROR`, removed `(SOCKET)` cast
- Connect: Added `sin_family = AF_INET`, `htons(port)`
- Removed `_APP_PumpDNS()` (lwIP DNS is synchronous)
- Replaced `#if defined(TCPIP_STACK_COMMAND_ENABLE)` with `#if 1`

### 5.3: `app_commands.c`
- Replaced `tcpip/tcpip.h` include with `<string.h>`
- Replaced `TCPIP_STACK_COMMAND_ENABLE` guard with `#if 1`

## Step 6: Update Build System (`user.cmake`)

- Added all lwIP source files (core, ipv4, api, netif, port)
- Added lwIP include paths: `lwip-1.4.1/src/include`, `include/ipv4`, `lwip_port`
- Added FreeRTOS kernel sources
- Excluded Harmony TCP/IP, GMAC, MIIM, ethphy, net_pres .c files using `set_source_files_properties(... PROPERTIES HEADER_FILE_ONLY TRUE)`

## Step 7: Build Fixes Applied

1. **`arch/sys_arch.h` not found**: Copied `sys_arch.h` into `lwip_port/arch/` subdirectory
2. **`BYTE_ORDER` redefined**: Added `#ifndef BYTE_ORDER` guard in `cc.h`
3. **`struct timeval` redefinition**: Set `LWIP_TIMEVAL_PRIVATE=0` in lwipopts.h and added `#include <sys/time.h>` in cc.h

## Verification

1. **Build**: Clean build succeeds (168/168 steps)
2. **Boot**: Flash and verify console shows initialization
3. **DHCP**: Verify IP address printed on console
4. **Ping**: Ping board from external host
5. **TCP**: `openurl http://www.example.com/` should fetch and display HTTP response

---

## Files Summary

| New Files | Purpose |
|---|---|
| `src/third_party/lwip/lwip-1.4.1/src/**` | lwIP 1.4.1 source |
| `src/config/default/lwipopts.h` | lwIP configuration |
| `src/config/default/lwip_port/arch/cc.h` | Compiler config |
| `src/config/default/lwip_port/arch/perf.h` | Perf stubs |
| `src/config/default/lwip_port/arch/sys_arch.h` | FreeRTOS types |
| `src/config/default/lwip_port/sys_arch.h` | FreeRTOS types (copy) |
| `src/config/default/lwip_port/sys_arch.c` | FreeRTOS implementation |
| `src/config/default/lwip_port/ethernetif.c` | GMAC netif driver |
| `src/config/default/lwip_port/ethernetif.h` | GMAC netif header |

| Modified Files | Change |
|---|---|
| `initialization.c` | Remove Harmony TCP/IP init, add lwIP |
| `tasks.c` | Remove Harmony tasks |
| `definitions.h` | Replace includes, trim SYSTEM_OBJECTS |
| `FreeRTOSConfig.h` | Increase heap to 64KB |
| `app.c` | Adapt to lwIP socket API |
| `app.h` | Replace SOCKET with int |
| `app_commands.c` | Remove Harmony include |
| `user.cmake` | Add lwIP, exclude Harmony sources |
