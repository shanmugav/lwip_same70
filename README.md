# Berkeley TCP Client - lwIP 1.4.1 on SAM E70 Xplained Ultra

## Overview

This project ports the Microchip Harmony Berkeley TCP Client demo from the Harmony TCP/IP stack to **lwIP 1.4.1** on the **SAM E70 Xplained Ultra** (ATSAME70Q21B) with FreeRTOS.

- lwIP configured with `NO_SYS=0` (FreeRTOS), `LWIP_SOCKET=1` (Berkeley socket API)
- GMAC driver with DMA descriptors in non-cacheable SRAM (MPU region) for Cortex-M7 D-cache coherency
- PHY: LAN8740 at 100Mbps Full Duplex via RMII

## Build and Program

### Prerequisites

- MPLAB X IDE v6.30+
- XC32 Compiler v5.10+
- SAM E70 Xplained Ultra board connected via EDBG USB

### Build (Command Line)

```bash
# Configure
cmake -S sam_e70_xult.X/cmake/berkeley_tcp_client/sam_e70_xult \
      -B sam_e70_xult.X/_build/berkeley_tcp_client/sam_e70_xult \
      --preset berkeley_tcp_client_sam_e70_xult_conf

# Build
cmake --build sam_e70_xult.X/_build/berkeley_tcp_client/sam_e70_xult
```

Output: `sam_e70_xult.X/out/berkeley_tcp_client/sam_e70_xult.hex`

### Program

Using MDB (Microchip Debugger):

```
device ATSAME70Q21B
set communication.interface swd
hwtool edbg
program sam_e70_xult.X/out/berkeley_tcp_client/sam_e70_xult.hex
reset
quit
```

## Testing

### Serial Console

Connect to the EDBG Virtual COM port at **115200 baud, 8N1**.

### 1. DHCP (Tested - Working)

**Steps:**
1. Connect the board to a network with a DHCP server via Ethernet
2. Reset or power-cycle the board
3. Observe the serial console

**Expected output:**
```
Network interface link is up
IP Address: x.x.x.x
```

The board obtains an IP address via DHCP automatically on boot.

### 2. HTTP GET via `openurl` (Tested - Working)

**Steps:**
1. Ensure the board has an IP address (DHCP complete)
2. Start an HTTP server on a host PC reachable from the board, e.g.:
   ```
   python -m http.server 9000
   ```
3. In the serial console, type:
   ```
   openurl http://<host-ip>:9000/
   ```
4. Observe the HTTP response printed on the console

**Expected output:**
```
[APP] DNS: resolving host...
[APP] Connected OK!
[APP] Sending HTTP GET...
[APP] --- Received data ---
HTTP/1.0 200 OK
...
[APP] --- End data ---
[APP] Connection Closed
```

**Notes:**
- The socket has a 5-second receive timeout (`SO_RCVTIMEO`). If the server is slow, the connection closes after 5 seconds of inactivity.
- On connection failure, the app cleanly closes the socket and returns to the command prompt for retry.

### 3. Ping / ICMP (Tested - Partially Working)

**Steps:**
1. Ensure the board has an IP address
2. From a host PC, run:
   ```
   ping <board-ip>
   ```

**Expected:** Replies with 1-2ms latency.

## Known Issues

- **ICMP (Ping) intermittent timeouts:** Ping replies work but approximately 50% of ICMP echo requests result in "Request timed out" on the host PC. DHCP, ARP, TCP, and UDP traffic are unaffected. The root cause is under investigation and is suspected to be related to Cortex-M7 D-cache interactions with the lwIP pbuf pool during ICMP echo reply generation (lwIP reuses the received pbuf in-place for the reply path, unlike TCP/UDP which copy to separate buffers).
