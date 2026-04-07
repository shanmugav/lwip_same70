"""
Incremental GMAC Port Validation Script.

Validates the lwIP GMAC port layer-by-layer, from hardware init up to
full TCP data transfer. Each layer depends on the previous one, so the
FIRST failure pinpoints the exact subsystem that needs debugging.

Usage:
  Serial:    python test_gmac_incremental.py --port COM5
  Log file:  python test_gmac_incremental.py --logfile boot_log.txt
  Manual:    python test_gmac_incremental.py --manual

Validation layers (bottom-up):
  L1: Boot & Peripheral Init  (GMAC clock, PIO, no crash)
  L2: GMAC Register Init      (GMAC NCR/NCFGR configured, no hard fault)
  L3: PHY MDIO Access         (MIIM read/write works, PHY detected)
  L4: PHY Auto-Negotiation    (Link speed/duplex negotiated)
  L5: GMAC Link Up            (Physical Ethernet link established)
  L6: GMAC RX Path            (DMA descriptors, interrupt, pbuf allocation)
  L7: GMAC TX Path            (DMA TX descriptors, transmission start)
  L8: ARP/IP Layer            (IP stack processes packets correctly)
  L9: DHCP Complete           (Full TX+RX cycle: discover->offer->request->ack)
  L10: DNS Resolution         (UDP TX/RX through stack)
  L11: TCP Handshake          (SYN/SYN-ACK/ACK through GMAC)
  L12: TCP Data Transfer      (Sustained multi-packet RX)
  L13: TCP Close              (FIN/ACK exchange, clean shutdown)
"""

import argparse
import re
import sys
import textwrap

# ---- Layer definitions ----
# Each layer has:
#   - pass_patterns: any match = layer passed
#   - fail_patterns: any match = layer definitively failed
#   - inferred_from: if these higher layers pass, this layer is implicitly validated
#   - diagnosis: what to check if this layer fails

LAYERS = [
    {
        "id": "L1",
        "name": "Boot & Peripheral Init",
        "what": "MCU boots, clocks configured, no crash during GMAC peripheral enable",
        "pass_patterns": [
            r"(?:Network|IP Address|link|Interface|Connecting|HTTP|openurl)",
        ],
        "fail_patterns": [
            r"(?:Hard\s*Fault|Usage\s*Fault|Bus\s*Fault|MemManage\s*Fault)",
            r"stack\s+overflow",
        ],
        "inferred_from": ["L2"],
        "diagnosis": [
            "If no output at all: check USART1 connection, baud rate (115200), board power",
            "If Hard Fault: GMAC clock not enabled, or GMAC_REGS address (0x40050000) wrong",
            "If stack overflow: increase FreeRTOS configTOTAL_HEAP_SIZE",
            "Check: CLOCK_Initialize() enables GMAC peripheral clock",
            "Check: PIO_Initialize() configures GMAC pins (PA0-PA4 for RMII)",
        ],
    },
    {
        "id": "L2",
        "name": "GMAC Register Init",
        "what": "GMAC NCR/NCFGR/DCFGR written, DMA descriptors configured, no fault",
        "pass_patterns": [
            r"(?:link\s+is\s+up|IP\s+Address|PHY)",
        ],
        "fail_patterns": [
            r"Hard\s*Fault",
            r"Bus\s*Fault",
        ],
        "inferred_from": ["L3"],
        "diagnosis": [
            "Hard Fault here = bad register address or unaligned DMA descriptor",
            "Check: GMAC_REGS points to 0x40050000 (from atsame70q21b.h)",
            "Check: DMA descriptors are 8-byte aligned (__attribute__((aligned(8))))",
            "Check: GMAC_RBQB and GMAC_TBQB set to descriptor array addresses",
            "Check: Unused queues (1-5) have null descriptors with WRAP+USED bits",
            "Debug: Add printf after each GMAC_REGS write in gmac_low_level_init()",
        ],
    },
    {
        "id": "L3",
        "name": "PHY MDIO Access",
        "what": "MIIM read/write via GMAC_MAN register works, LAN8740 PHY responds",
        "pass_patterns": [
            r"(?:link\s+is\s+up|auto\s+negotiate|IP\s+Address)",
        ],
        "fail_patterns": [
            r"PHY\s+init.*(?:ERROR|fail|timeout)",
        ],
        "inferred_from": ["L4"],
        "diagnosis": [
            "PHY not responding = MDIO clock or data pin issue",
            "Check: GMAC_NCR has MPE bit set (management port enable)",
            "Check: GMAC_NCFGR CLK field = 4 (MCK/96 for 150MHz periph clock)",
            "Check: PHY address = 0 (LAN8740 on SAM E70 Xplained Ultra)",
            "Check: GMAC_MAN register format: CLTTO=1, OP=2(read)/1(write), WTN=2",
            "Check: Wait for GMAC_NSR IDLE bit before and after each MDIO access",
            "Debug: Read PHY ID registers (reg 2 & 3), expect LAN8740 ID 0x0007C130",
        ],
    },
    {
        "id": "L4",
        "name": "PHY Auto-Negotiation",
        "what": "Speed and duplex negotiated with link partner (switch/router)",
        "pass_patterns": [
            r"link\s+is\s+up",
            r"IP\s+Address:\s*(?!0\.0\.0\.0)\d+\.\d+\.\d+\.\d+",
        ],
        "fail_patterns": [
            r"auto\s+negotiate.*(?:ERROR|fail|timeout)",
            r"set\s+link.*ERROR",
        ],
        "inferred_from": ["L5"],
        "diagnosis": [
            "Ethernet cable connected? Link LED on board and switch should be lit",
            "Check: PHY BCR register (reg 0) bit 12 (auto-neg enable) is set",
            "Check: PHY BCR register bit 9 (restart auto-neg) triggered",
            "Check: Poll PHY BSR register (reg 1) bit 5 for auto-neg complete",
            "Check: Timeout adequate (>2 seconds for auto-neg)",
            "Check: After auto-neg, read ANLPAR (reg 5) to set GMAC speed/duplex",
            "Check: GMAC_NCFGR SPD and FD bits match negotiated speed/duplex",
        ],
    },
    {
        "id": "L5",
        "name": "GMAC Link Up",
        "what": "Ethernet physical link established, netif flags set",
        "pass_patterns": [
            r"[Ll]ink\s+is\s+up",
            r"[Nn]etwork\s+interface\s+link\s+is\s+up",
        ],
        "fail_patterns": [
            r"link\s+.*(?:down|fail)",
        ],
        "inferred_from": ["L6"],
        "diagnosis": [
            "If auto-neg passed but link not up: check netif->flags |= NETIF_FLAG_LINK_UP",
            "Check: Ethernet cable firmly connected to both board and switch",
            "Check: ethernetif_init() sets NETIF_FLAG_LINK_UP after PHY init",
            "Check: GMAC_NCR has TXEN and RXEN bits set",
        ],
    },
    {
        "id": "L6",
        "name": "GMAC RX Path (Interrupt + DMA)",
        "what": "GMAC receives packets, ISR fires, RX semaphore signaled, pbufs delivered to lwIP",
        "pass_patterns": [
            r"IP\s+Address:\s*(?!0\.0\.0\.0)\d+\.\d+\.\d+\.\d+",
        ],
        "fail_patterns": [
            r"pbuf\s+allocation\s+failure",
            r"GMAC\s+ERROR.*reinit",
            r"RX\s+overrun",
        ],
        "inferred_from": ["L9"],
        "diagnosis": [
            "DHCP needs RX to receive DHCP Offer/Ack from server",
            "If stuck at 'link is up' with no IP: RX path is broken",
            "Check: GMAC_IER enables RCOMP interrupt (receive complete)",
            "Check: NVIC_EnableIRQ(GMAC_IRQn) called",
            "Check: GMAC_InterruptHandler() reads GMAC_ISR and signals rx_sem",
            "Check: ISR name matches vector table (GMAC_InterruptHandler, not GMAC_Handler)",
            "Check: RX descriptor ownership bit logic correct",
            "Check: pbuf payload addresses are 4-byte aligned",
            "Check: gmac_task() calls sys_arch_sem_wait() then ethernetif_input()",
            "Debug: Toggle LED in GMAC_InterruptHandler to verify ISR fires",
            "Debug: Add counter in gmac_low_level_input() to track received frames",
        ],
    },
    {
        "id": "L7",
        "name": "GMAC TX Path (DMA)",
        "what": "GMAC transmits packets via DMA descriptors",
        "pass_patterns": [
            r"IP\s+Address:\s*(?!0\.0\.0\.0)\d+\.\d+\.\d+\.\d+",
        ],
        "fail_patterns": [
            r"GMAC\s+ERROR.*reinit\s+TX",
            r"TX\s+underrun",
        ],
        "inferred_from": ["L9"],
        "diagnosis": [
            "DHCP needs TX to send DHCP Discover/Request",
            "Check: TX descriptor buffer addresses point to valid tx_buf[] entries",
            "Check: TX descriptor status word clears USED bit before TSTART",
            "Check: GMAC_NCR TSTART bit set after preparing descriptor",
            "Check: TX descriptor LAST bit set, length field correct",
            "Check: WRAP bit set on last TX descriptor in ring",
            "Debug: Read GMAC_TSR after TX attempt to check for errors",
        ],
    },
    {
        "id": "L8",
        "name": "ARP/IP Layer",
        "what": "lwIP processes received Ethernet frames, ARP resolution works",
        "pass_patterns": [
            r"IP\s+Address:\s*(?!0\.0\.0\.0)\d+\.\d+\.\d+\.\d+",
        ],
        "fail_patterns": [
            r"IP\s+input\s+error",
        ],
        "inferred_from": ["L9"],
        "diagnosis": [
            "If RX ISR fires but no IP: ethernetif_input() dispatch may be wrong",
            "Check: ethernetif_input() handles ETHTYPE_IP and ETHTYPE_ARP",
            "Check: netif->input set to tcpip_input (not ip_input - threaded mode)",
            "Check: etharp_output assigned to netif->output",
            "Check: gmac_low_level_output assigned to netif->linkoutput",
        ],
    },
    {
        "id": "L9",
        "name": "DHCP Complete",
        "what": "Full DHCP cycle: Discover(TX)->Offer(RX)->Request(TX)->Ack(RX)",
        "pass_patterns": [
            r"IP\s+Address:\s*(?!0\.0\.0\.0)(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})",
        ],
        "fail_patterns": [
            r"IP\s+Address:\s*0\.0\.0\.0",
        ],
        "inferred_from": [],
        "diagnosis": [
            "THIS IS THE KEY GMAC VALIDATION CHECKPOINT",
            "If IP acquired: GMAC TX, RX, interrupts, DMA, and PHY all work!",
            "If stuck here: see L6 (RX) and L7 (TX) diagnosis above",
            "Check: dhcp_start(&g_netif) called in lwip_network_init()",
            "Check: DHCP server exists on the network (router/switch)",
            "Check: If using static IP fallback, verify netif_add() parameters",
            "Check: lwipopts.h has LWIP_DHCP=1",
            "Fallback test: use static IP and try to ping the board",
        ],
    },
    {
        "id": "L10",
        "name": "DNS Resolution",
        "what": "Hostname resolved via UDP DNS query",
        "pass_patterns": [
            r"(?:BSD\s+TCP\s+client:\s+)?Connecting",
        ],
        "fail_patterns": [
            r"DNS\s+resolution\s+failed",
            r"Could\s+not\s+parse\s+URL",
        ],
        "inferred_from": [],
        "diagnosis": [
            "Check: lwipopts.h has LWIP_DNS=1",
            "Check: DHCP provided a DNS server (or configure manually)",
            "Check: dns_init() called by lwip_init()",
            "Try: Use IP address instead of hostname to bypass DNS",
            "'Could not parse URL': URL format must be http://host/path",
        ],
    },
    {
        "id": "L11",
        "name": "TCP Handshake",
        "what": "TCP SYN sent, SYN-ACK received, connection established",
        "pass_patterns": [
            r"HTTP/\d\.\d",
            r"Connection\s+Closed",
        ],
        "fail_patterns": [
            r"connect.*(?:fail|error|refused|timeout)",
        ],
        "inferred_from": ["L12"],
        "diagnosis": [
            "If DNS passed but no HTTP response: TCP connect failed",
            "Check: Remote server reachable (try curl from PC on same network)",
            "Check: Port 80 not blocked by firewall",
            "Check: appData.addr.sin_family = AF_INET is set",
            "Check: appData.addr.sin_port = htons(port) uses network byte order",
            "Check: lwipopts.h TCP_MSS=1460, reasonable TCP_WND and TCP_SND_BUF",
        ],
    },
    {
        "id": "L12",
        "name": "TCP Data Transfer",
        "what": "HTTP response data received over TCP",
        "pass_patterns": [
            r"HTTP/\d\.\d\s+\d{3}",
            r"(?:Content-Type|Server|Date):",
        ],
        "fail_patterns": [],
        "inferred_from": [],
        "diagnosis": [
            "If connected but no data: recv() may be blocking or returning errors",
            "Check: recv() buffer size adequate",
            "Check: errno handling (EWOULDBLOCK vs real errors)",
            "Check: GMAC handles multi-packet reception correctly",
            "Check: RX descriptor ring repopulated after each received frame",
        ],
    },
    {
        "id": "L13",
        "name": "TCP Connection Close",
        "what": "TCP FIN/ACK exchange, socket closed cleanly",
        "pass_patterns": [
            r"Connection\s+Closed",
        ],
        "fail_patterns": [],
        "inferred_from": [],
        "diagnosis": [
            "If HTTP received but no 'Connection Closed': closesocket() issue",
            "Check: closesocket() mapped to lwip_close() via LWIP_COMPAT_SOCKETS",
            "Non-critical if data was received - app state machine may just not reach close",
        ],
    },
]


def analyze_log(log_text):
    """Run incremental layer validation. Stop at first failure for diagnosis focus."""
    results = []
    first_failure = None

    for layer in LAYERS:
        result = {
            "id": layer["id"],
            "name": layer["name"],
            "what": layer["what"],
            "status": "UNKNOWN",
            "detail": "",
            "diagnosis": layer["diagnosis"],
        }

        # Check explicit fail patterns
        failed = False
        for pattern in layer["fail_patterns"]:
            match = re.search(pattern, log_text, re.IGNORECASE)
            if match:
                result["status"] = "FAIL"
                result["detail"] = f"Failure indicator: '{match.group()}'"
                failed = True
                if not first_failure:
                    first_failure = result
                break

        if not failed:
            # Check pass patterns
            passed = False
            for pattern in layer["pass_patterns"]:
                match = re.search(pattern, log_text, re.IGNORECASE)
                if match:
                    result["status"] = "PASS"
                    result["detail"] = f"Confirmed: '{match.group()}'"
                    passed = True
                    break

            # Check if inferred from higher layers
            if not passed:
                for inf_id in layer.get("inferred_from", []):
                    for prev in results:
                        if prev["id"] == inf_id and prev["status"] == "PASS":
                            result["status"] = "PASS (inferred)"
                            result["detail"] = f"Inferred from {inf_id} passing"
                            passed = True
                            break
                    if passed:
                        break

            if not passed:
                result["status"] = "NOT REACHED"
                result["detail"] = "Expected output not found in log"
                if not first_failure:
                    first_failure = result

        results.append(result)

    return results, first_failure


def print_incremental_results(results, first_failure, log_text):
    """Print layer-by-layer results with diagnosis for first failure."""

    ip_addr = None
    match = re.search(r"IP\s+Address:\s*(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})", log_text)
    if match and match.group(1) != "0.0.0.0":
        ip_addr = match.group(1)

    print("\n" + "=" * 72)
    print("  INCREMENTAL GMAC PORT VALIDATION")
    print("  Layer-by-layer analysis (bottom-up, hardware to application)")
    print("=" * 72)

    all_pass = True
    highest_pass = None

    for r in results:
        if "PASS" in r["status"]:
            icon = " [OK] "
            highest_pass = r["id"]
        elif r["status"] == "FAIL":
            icon = " [!!] "
            all_pass = False
        else:
            icon = " [..] "
            all_pass = False

        print(f"\n{icon}{r['id']}: {r['name']}")
        print(f"       {r['what']}")
        print(f"       Status: {r['status']} - {r['detail']}")

    # Summary
    print("\n" + "=" * 72)

    if ip_addr:
        print(f"  Acquired IP: {ip_addr}")

    passed_count = sum(1 for r in results if "PASS" in r["status"])
    print(f"\n  Layers validated: {passed_count}/{len(results)}")

    if highest_pass:
        print(f"  Highest layer passed: {highest_pass}")

    if all_pass:
        print("\n  RESULT: ALL LAYERS PASSED - GMAC port is fully functional!")
        print("=" * 72)
        return 0

    # Show diagnosis for first failure
    if first_failure:
        print(f"\n  FIRST ISSUE at {first_failure['id']}: {first_failure['name']}")
        print(f"  Status: {first_failure['status']} - {first_failure['detail']}")
        print(f"\n  DIAGNOSIS - What to check:")
        for i, diag in enumerate(first_failure["diagnosis"], 1):
            wrapped = textwrap.fill(diag, width=66, subsequent_indent="     ")
            print(f"    {i}. {wrapped}")

        # Show what the failure means for GMAC
        idx = next(i for i, r in enumerate(results) if r["id"] == first_failure["id"])
        if idx <= 1:
            print("\n  IMPACT: Hardware-level issue. GMAC registers or clocks not configured.")
        elif idx <= 4:
            print("\n  IMPACT: PHY/link issue. GMAC itself may be OK but can't talk to network.")
        elif idx <= 8:
            print("\n  IMPACT: GMAC data path issue. DMA, interrupts, or pbuf management broken.")
        elif idx <= 9:
            print("\n  IMPACT: Full GMAC path issue. Both TX and RX needed for DHCP.")
        else:
            print("\n  IMPACT: Higher-layer issue. GMAC hardware port is likely working.")

    print("=" * 72 + "\n")
    return 1


# ---- Input modes ----

def run_serial(port, baud, test_url):
    try:
        import serial
    except ImportError:
        print("ERROR: pyserial not installed. Run: pip install pyserial")
        print("       Or use --logfile / --manual mode instead.")
        return 1

    print(f"Connecting to {port} at {baud} baud...")
    print("Reset the board now. Will auto-send 'openurl' after IP acquired.")
    print("Press Ctrl+C to stop early and analyze collected output.\n")

    log_lines = []
    command_sent = False

    try:
        ser = serial.Serial(port, baud, timeout=1)
        start = time.time()

        while True:
            line = ser.readline()
            if line:
                try:
                    text = line.decode("utf-8", errors="replace").rstrip()
                except:
                    text = str(line)
                log_lines.append(text)
                elapsed = time.time() - start
                print(f"  [{elapsed:6.1f}s] {text}")

                if not command_sent and re.search(r"IP\s+Address:\s*(?!0\.0\.0\.0)\d+", text):
                    time.sleep(1.5)
                    cmd = f"openurl {test_url}\r\n"
                    ser.write(cmd.encode())
                    log_lines.append(f"[SENT] {cmd.strip()}")
                    print(f"  [SENT] {cmd.strip()}")
                    command_sent = True

                if command_sent and re.search(r"Connection\s+Closed", text):
                    time.sleep(0.5)
                    break

            elapsed = time.time() - start
            if not command_sent and elapsed > 60:
                print("\n  TIMEOUT: No IP address within 60s. Analyzing what we have...")
                break
            if command_sent and elapsed > 120:
                print("\n  TIMEOUT: No HTTP response within timeout. Analyzing...")
                break

    except KeyboardInterrupt:
        print("\n  Stopped by user. Analyzing collected output...")
    except Exception as e:
        print(f"\n  Error: {e}")
    finally:
        try:
            ser.close()
        except:
            pass

    full_log = "\n".join(log_lines)
    results, first_failure = analyze_log(full_log)
    return print_incremental_results(results, first_failure, full_log)


def run_logfile(filepath):
    try:
        with open(filepath, "r", encoding="utf-8", errors="replace") as f:
            log_text = f.read()
    except FileNotFoundError:
        print(f"ERROR: File not found: {filepath}")
        return 1

    print(f"Analyzing: {filepath} ({len(log_text)} bytes)")
    results, first_failure = analyze_log(log_text)
    return print_incremental_results(results, first_failure, log_text)


def run_manual():
    print("Paste serial console log below.")
    print("Press Ctrl+Z then Enter (Windows) or Ctrl+D (Linux/Mac) when done:")
    print("-" * 40)
    lines = []
    try:
        while True:
            lines.append(input())
    except EOFError:
        pass

    if not lines:
        print("No input.")
        return 1

    log_text = "\n".join(lines)
    results, first_failure = analyze_log(log_text)
    return print_incremental_results(results, first_failure, log_text)


# ---- Entry point ----

import time

def main():
    parser = argparse.ArgumentParser(
        description="Incremental GMAC Port Validation for lwIP on SAM E70",
    )
    parser.add_argument("--port", help="Serial port (COM5, /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--url", default="http://example.com/")
    parser.add_argument("--logfile", help="Saved log file to analyze")
    parser.add_argument("--manual", action="store_true", help="Paste log from stdin")
    args = parser.parse_args()

    if args.logfile:
        return run_logfile(args.logfile)
    elif args.manual:
        return run_manual()
    elif args.port:
        return run_serial(args.port, args.baud, args.url)
    else:
        parser.print_help()
        print("\nQuick start: python test_gmac_incremental.py --manual")
        return 1


if __name__ == "__main__":
    sys.exit(main())
