"""
GMAC Port Validation Script for lwIP Berkeley TCP Client on SAM E70.

Usage:
  Interactive (serial):  python test_gmac_port.py --port COM5 --baud 115200
  Log file analysis:     python test_gmac_port.py --logfile boot_log.txt
  Manual paste mode:     python test_gmac_port.py --manual

Tests validated:
  1. System boot and lwIP initialization
  2. GMAC link up (PHY auto-negotiation success)
  3. DHCP IP address acquisition (proves GMAC RX works)
  4. TCP connection + HTTP GET (proves GMAC TX+RX, full stack)
  5. Connection close (clean shutdown)
"""

import argparse
import re
import sys
import time

# --- Test definitions ---

class TestResult:
    PASS = "PASS"
    FAIL = "FAIL"
    SKIP = "SKIP"
    TIMEOUT = "TIMEOUT"

TESTS = [
    {
        "id": "T1",
        "name": "System Boot",
        "description": "Board boots and reaches lwIP init",
        "pass_patterns": [
            r"(?:Network interface|IP Address|link is up|MCHPBOARD)",
        ],
        "fail_patterns": [
            r"(?:Hard Fault|HardFault|assert|ASSERT|stack overflow|malloc failed)",
        ],
        "gmac_relevance": "Indirect - confirms GMAC clock/peripheral init didn't crash",
    },
    {
        "id": "T2",
        "name": "GMAC Link Up",
        "description": "Ethernet PHY auto-negotiation completed, link established",
        "pass_patterns": [
            r"[Ll]ink\s+is\s+up",
            r"[Nn]etwork\s+interface.*up",
        ],
        "fail_patterns": [
            r"PHY\s+init.*(?:ERROR|fail)",
            r"auto\s+negotiate.*(?:ERROR|fail)",
            r"set\s+link.*ERROR",
        ],
        "gmac_relevance": "CRITICAL - Validates GMAC register init, MIIM/MDIO PHY access, auto-negotiation",
    },
    {
        "id": "T3",
        "name": "DHCP IP Acquisition",
        "description": "Valid IP address obtained via DHCP (or static fallback)",
        "pass_patterns": [
            r"IP\s+Address:\s*(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})",
        ],
        "fail_patterns": [
            r"IP\s+Address:\s*0\.0\.0\.0",
        ],
        "gmac_relevance": "CRITICAL - DHCP requires TX (discover/request) and RX (offer/ack). "
                          "Proves GMAC DMA descriptors, interrupt handler, and pbuf management work.",
    },
    {
        "id": "T4",
        "name": "DNS Resolution",
        "description": "Hostname resolved to IP address",
        "pass_patterns": [
            r"(?:Connecting|BSD\s+TCP\s+client:\s+Connecting)",
        ],
        "fail_patterns": [
            r"DNS\s+resolution\s+failed",
            r"Could\s+not\s+parse\s+URL",
        ],
        "gmac_relevance": "Validates UDP TX/RX through GMAC (DNS uses UDP)",
    },
    {
        "id": "T5",
        "name": "HTTP Response Received",
        "description": "TCP data received from remote server",
        "pass_patterns": [
            r"HTTP/\d\.\d\s+\d{3}",
            r"(?:Content-Type|Server|Date|Content-Length):",
        ],
        "fail_patterns": [],
        "gmac_relevance": "CRITICAL - Proves full TCP handshake (SYN/SYN-ACK/ACK) and data transfer "
                          "through GMAC. Validates TX descriptor management, RX interrupt-driven path, "
                          "and sustained multi-packet operation.",
    },
    {
        "id": "T6",
        "name": "Connection Close",
        "description": "TCP connection cleanly closed",
        "pass_patterns": [
            r"Connection\s+Closed",
        ],
        "fail_patterns": [],
        "gmac_relevance": "Validates TCP FIN/ACK exchange through GMAC",
    },
]

# --- Validation engine ---

def validate_log(log_text, verbose=True):
    """Analyze log text against all test cases. Returns (results, summary)."""
    results = []
    all_pass = True

    for test in TESTS:
        result = {
            "id": test["id"],
            "name": test["name"],
            "status": TestResult.FAIL,
            "detail": "",
            "gmac_relevance": test["gmac_relevance"],
        }

        # Check fail patterns first
        failed = False
        for pattern in test["fail_patterns"]:
            match = re.search(pattern, log_text, re.IGNORECASE)
            if match:
                result["status"] = TestResult.FAIL
                result["detail"] = f"Failure pattern matched: '{match.group()}'"
                failed = True
                break

        if not failed:
            # Check pass patterns
            passed = False
            for pattern in test["pass_patterns"]:
                match = re.search(pattern, log_text, re.IGNORECASE)
                if match:
                    result["status"] = TestResult.PASS
                    result["detail"] = f"Matched: '{match.group()}'"
                    passed = True
                    break

            if not passed:
                result["status"] = TestResult.SKIP
                result["detail"] = "Expected pattern not found in log"

        if result["status"] != TestResult.PASS:
            all_pass = False

        results.append(result)

    return results, all_pass


def extract_ip_address(log_text):
    """Extract the IP address from the log if present."""
    match = re.search(r"IP\s+Address:\s*(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})", log_text)
    if match:
        ip = match.group(1)
        octets = ip.split(".")
        if all(0 <= int(o) <= 255 for o in octets) and ip != "0.0.0.0":
            return ip
    return None


def check_gmac_health(log_text):
    """Additional GMAC-specific health checks beyond the main tests."""
    issues = []

    # Check for GMAC error indicators
    if re.search(r"GMAC\s+ERROR", log_text, re.IGNORECASE):
        issues.append("GMAC error detected (TX/RX descriptor issue)")

    if re.search(r"reinit\s+TX", log_text, re.IGNORECASE):
        issues.append("TX descriptor reinit triggered (underrun or AHB error)")

    if re.search(r"pbuf\s+allocation\s+failure", log_text, re.IGNORECASE):
        issues.append("pbuf allocation failure (memory pressure)")

    if re.search(r"IP\s+input\s+error", log_text, re.IGNORECASE):
        issues.append("lwIP IP input error (packet processing failure)")

    # Check for crash indicators
    if re.search(r"(?:Hard\s*Fault|Usage\s*Fault|Bus\s*Fault|MemManage)", log_text, re.IGNORECASE):
        issues.append("ARM fault exception detected - likely GMAC DMA or alignment issue")

    if re.search(r"stack\s+overflow", log_text, re.IGNORECASE):
        issues.append("FreeRTOS stack overflow - GMAC task or tcpip_thread stack too small")

    if re.search(r"malloc\s+failed|heap.*fail", log_text, re.IGNORECASE):
        issues.append("Memory allocation failure - increase FreeRTOS heap or lwIP MEM_SIZE")

    return issues


def print_results(results, all_pass, gmac_issues, ip_addr):
    """Print formatted test results."""
    print("\n" + "=" * 70)
    print("  GMAC Port Validation Results")
    print("=" * 70)

    for r in results:
        icon = {"PASS": "[OK]", "FAIL": "[!!]", "SKIP": "[--]", "TIMEOUT": "[TO]"}[r["status"]]
        color_status = r["status"]
        print(f"\n  {icon} {r['id']}: {r['name']} - {color_status}")
        print(f"      {r['detail']}")
        print(f"      GMAC relevance: {r['gmac_relevance']}")

    print("\n" + "-" * 70)

    if ip_addr:
        print(f"  Acquired IP: {ip_addr}")

    if gmac_issues:
        print("\n  GMAC Health Warnings:")
        for issue in gmac_issues:
            print(f"    [!] {issue}")
    else:
        print("\n  GMAC Health: No error indicators found in log")

    print("\n" + "-" * 70)

    passed = sum(1 for r in results if r["status"] == TestResult.PASS)
    failed = sum(1 for r in results if r["status"] == TestResult.FAIL)
    skipped = sum(1 for r in results if r["status"] == TestResult.SKIP)

    print(f"  Summary: {passed} passed, {failed} failed, {skipped} skipped out of {len(results)}")

    if all_pass:
        print("\n  >>> GMAC PORT VALIDATION: ALL TESTS PASSED <<<")
    elif failed > 0:
        print("\n  >>> GMAC PORT VALIDATION: FAILURES DETECTED <<<")
    else:
        print("\n  >>> GMAC PORT VALIDATION: INCOMPLETE (some tests not reached) <<<")
        print("  Tip: Ensure you ran 'openurl http://example.com/' on the console")

    print("=" * 70 + "\n")

    return 0 if all_pass else 1


# --- Serial interactive mode ---

def run_serial_test(port, baud, test_url="http://example.com/"):
    """Connect to serial port, capture boot log, send openurl command, validate."""
    try:
        import serial
    except ImportError:
        print("ERROR: pyserial not installed. Install with: pip install pyserial")
        print("       Or use --logfile or --manual mode instead.")
        return 1

    print(f"Connecting to {port} at {baud} baud...")
    print("Waiting for board to boot (reset the board if needed)...")
    print("Press Ctrl+C to stop capture and analyze.\n")

    log_lines = []
    state = "WAIT_BOOT"
    command_sent = False

    try:
        ser = serial.Serial(port, baud, timeout=1)
        start_time = time.time()

        while True:
            line = ser.readline()
            if line:
                try:
                    text = line.decode("utf-8", errors="replace").rstrip()
                except:
                    text = str(line)

                log_lines.append(text)
                print(f"  [{time.time()-start_time:6.1f}s] {text}")

                # Auto-send openurl command when IP address is acquired
                if not command_sent and re.search(r"IP\s+Address:\s*\d+\.\d+\.\d+\.\d+", text):
                    time.sleep(1)  # Brief delay for command readiness
                    cmd = f"openurl {test_url}\r\n"
                    ser.write(cmd.encode())
                    print(f"  [SENT] {cmd.strip()}")
                    command_sent = True

                # Auto-stop after connection closed
                if command_sent and re.search(r"Connection\s+Closed", text):
                    time.sleep(1)  # Capture any trailing output
                    break

            # Timeout: 60s for boot+DHCP, additional 30s after command
            elapsed = time.time() - start_time
            if not command_sent and elapsed > 60:
                print("\n  TIMEOUT: No IP address acquired within 60 seconds")
                break
            if command_sent and elapsed > 120:
                print("\n  TIMEOUT: HTTP response not received within timeout")
                break

    except KeyboardInterrupt:
        print("\n  Capture stopped by user.")
    except serial.SerialException as e:
        print(f"\n  Serial error: {e}")
        return 1
    finally:
        try:
            ser.close()
        except:
            pass

    full_log = "\n".join(log_lines)
    results, all_pass = validate_log(full_log)
    gmac_issues = check_gmac_health(full_log)
    ip_addr = extract_ip_address(full_log)

    return print_results(results, all_pass, gmac_issues, ip_addr)


# --- Manual / logfile mode ---

def run_manual_mode():
    """Accept pasted log from stdin."""
    print("Paste the serial console log below, then press Ctrl+D (Unix) or Ctrl+Z+Enter (Windows):")
    print("-" * 40)

    lines = []
    try:
        while True:
            line = input()
            lines.append(line)
    except EOFError:
        pass

    if not lines:
        print("No input received.")
        return 1

    full_log = "\n".join(lines)
    results, all_pass = validate_log(full_log)
    gmac_issues = check_gmac_health(full_log)
    ip_addr = extract_ip_address(full_log)

    return print_results(results, all_pass, gmac_issues, ip_addr)


def run_logfile_mode(filepath):
    """Analyze a saved log file."""
    try:
        with open(filepath, "r", encoding="utf-8", errors="replace") as f:
            full_log = f.read()
    except FileNotFoundError:
        print(f"ERROR: File not found: {filepath}")
        return 1

    print(f"Analyzing log file: {filepath} ({len(full_log)} bytes)")

    results, all_pass = validate_log(full_log)
    gmac_issues = check_gmac_health(full_log)
    ip_addr = extract_ip_address(full_log)

    return print_results(results, all_pass, gmac_issues, ip_addr)


# --- Main ---

def main():
    parser = argparse.ArgumentParser(
        description="GMAC Port Validation for lwIP Berkeley TCP Client on SAM E70",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python test_gmac_port.py --port COM5
  python test_gmac_port.py --port COM5 --url http://httpbin.org/get
  python test_gmac_port.py --logfile captured_log.txt
  python test_gmac_port.py --manual
        """,
    )
    parser.add_argument("--port", help="Serial port (e.g., COM5, /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--url", default="http://example.com/",
                        help="URL to test with openurl command (default: http://example.com/)")
    parser.add_argument("--logfile", help="Path to a saved log file to analyze")
    parser.add_argument("--manual", action="store_true", help="Manual mode: paste log from stdin")

    args = parser.parse_args()

    if args.logfile:
        return run_logfile_mode(args.logfile)
    elif args.manual:
        return run_manual_mode()
    elif args.port:
        return run_serial_test(args.port, args.baud, args.url)
    else:
        parser.print_help()
        print("\nTip: Use --manual to paste log output directly")
        return 1


if __name__ == "__main__":
    sys.exit(main())
