import argparse
import asyncio
import hashlib
import os
import re
import socket
import subprocess
import time
from dataclasses import dataclass
from datetime import datetime, timezone

from bleak import BleakClient, BleakScanner
from Crypto.Cipher import AES

DEFAULT_SERVICE_UUID = "0000fe60-0000-1000-8000-00805f9b34fb"
DEFAULT_WRITE_CHAR_UUID = "0000fe61-0000-1000-8000-00805f9b34fb"
DEFAULT_NOTIFY_CHAR_UUID = "0000fe62-0000-1000-8000-00805f9b34fb"

CMD_HANDSHAKE = 0x01
CMD_TIME_SYNC = 0x33

RTC_BASE = datetime(2025, 1, 1, tzinfo=timezone.utc)
BOOT_RTC_RE = re.compile(r"boot rtc timestamp=0x([0-9A-Fa-f]{8})")


@dataclass
class Candidate:
    device: object
    advertisement: object


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def build_frame(cmd: int, payload: bytes) -> bytes:
    header = bytes([0xFA, 0xFC, 0x01, cmd, (len(payload) >> 8) & 0xFF, len(payload) & 0xFF])
    crc = crc16_ccitt(header[3:] + payload)
    return header + payload + bytes([(crc >> 8) & 0xFF, crc & 0xFF])


def parse_frame(data: bytes) -> tuple[int | None, bytes, bool]:
    if len(data) < 8 or data[0] != 0xFA or data[1] != 0xFC:
        return None, b"", False
    payload_len = (data[4] << 8) | data[5]
    frame_len = 6 + payload_len + 2
    if len(data) < frame_len:
        return data[3], b"", False
    payload = data[6:6 + payload_len]
    crc_rx = (data[6 + payload_len] << 8) | data[7 + payload_len]
    return data[3], payload, crc_rx == crc16_ccitt(data[3:6 + payload_len])


def build_handshake_payload(device_address: str) -> bytes:
    seed = bytes.fromhex(device_address.replace(":", "").replace("-", ""))
    aes_key = hashlib.md5(seed).digest()
    return AES.new(aes_key, AES.MODE_ECB).encrypt(seed.ljust(16, b"\x00"))


def timestamp_from_now() -> int:
    return int((datetime.now(timezone.utc) - RTC_BASE).total_seconds())


async def scan_candidates(address: str, scan_timeout: float, limit: int) -> list[Candidate]:
    discovered = await BleakScanner.discover(timeout=scan_timeout, return_adv=True)
    candidates: list[Candidate] = []
    for _, payload in discovered.items():
        device, advertisement = payload
        if address and device.address.lower() != address.lower():
            continue
        candidates.append(Candidate(device=device, advertisement=advertisement))

    print(f"discovered {len(candidates)} candidate(s)")
    for candidate in candidates[:limit]:
        uuids = list(candidate.advertisement.service_uuids or [])
        print(f"candidate address={candidate.device.address} name={candidate.device.name!r} rssi={candidate.advertisement.rssi} uuids={uuids}")

    if not candidates:
        raise RuntimeError("no BLE candidate found")
    return candidates[:limit]


async def try_set_rtc_on_candidate(args: argparse.Namespace, candidate: Candidate) -> tuple[str, int] | None:
    notifications: list[bytes] = []

    def on_notify(_: object, data: bytearray) -> None:
        packet = bytes(data)
        notifications.append(packet)
        cmd, payload, crc_ok = parse_frame(packet)
        print(f"notify {candidate.device.address} cmd={cmd if cmd is not None else 'raw'} crc_ok={crc_ok} payload={payload.hex()}")

    try:
        async with BleakClient(candidate.device, timeout=args.connect_timeout) as client:
            print(f"connected address={candidate.device.address} name={candidate.device.name!r}")
            write_char = args.write_char_uuid
            notify_char = args.notify_char_uuid
            await client.start_notify(notify_char, on_notify)

            await client.write_gatt_char(write_char, build_frame(CMD_HANDSHAKE, build_handshake_payload(candidate.device.address)))
            await asyncio.sleep(args.packet_delay)

            timestamp = args.timestamp if args.timestamp is not None else timestamp_from_now()
            await client.write_gatt_char(write_char, build_frame(CMD_TIME_SYNC, timestamp.to_bytes(4, "big")))
            await asyncio.sleep(args.reply_timeout)
            await client.stop_notify(notify_char)
    except Exception as exc:
        print(f"probe-fail address={candidate.device.address} error={type(exc).__name__}: {exc}")
        return None

    if not any(parse_frame(item)[0] == CMD_TIME_SYNC and parse_frame(item)[2] for item in notifications):
        print(f"no time-sync reply address={candidate.device.address}")
        return None

    print(f"rtc set timestamp=0x{timestamp:08X} ({timestamp})")
    return candidate.device.address, timestamp


async def set_rtc_over_ble(args: argparse.Namespace) -> tuple[str, int]:
    candidates = await scan_candidates(args.address, args.scan_timeout, args.limit)
    for candidate in candidates:
        result = await try_set_rtc_on_candidate(args, candidate)
        if result is not None:
            return result
    raise RuntimeError("time-sync reply not received from any candidate")


def wait_for_port(port: int, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(0.2)
            if sock.connect_ex(("127.0.0.1", port)) == 0:
                return
        time.sleep(0.1)
    raise TimeoutError(f"RTT port {port} did not open")


def start_rtt_server(args: argparse.Namespace) -> subprocess.Popen:
    cmd = [
        args.jlink_gdb_server,
        "-device", args.jlink_device,
        "-if", args.jlink_interface,
        "-speed", str(args.jlink_speed),
        "-nohalt",
        "-port", str(args.gdb_port),
        "-swoport", str(args.swo_port),
        "-telnetport", str(args.telnet_port),
        "-RTTTelnetPort", str(args.rtt_port),
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    wait_for_port(args.rtt_port, args.rtt_timeout)
    return proc


def reset_target(args: argparse.Namespace) -> None:
    script = "r\ng\nqc\n"
    subprocess.run(
        [args.jlink_exe, "-device", args.jlink_device, "-if", args.jlink_interface, "-speed", str(args.jlink_speed)],
        input=script,
        text=True,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )


def capture_boot_log(args: argparse.Namespace, expected_timestamp: int) -> str:
    server = start_rtt_server(args)
    log_chunks: list[str] = []
    try:
        reset_target(args)
        deadline = time.monotonic() + args.log_timeout
        with socket.create_connection(("127.0.0.1", args.rtt_port), timeout=args.rtt_timeout) as sock:
            sock.settimeout(0.5)
            while time.monotonic() < deadline:
                try:
                    data = sock.recv(4096)
                except socket.timeout:
                    continue
                if not data:
                    break
                text = data.decode(errors="replace")
                print(text, end="")
                log_chunks.append(text)
                joined = "".join(log_chunks)
                match = BOOT_RTC_RE.search(joined)
                if match:
                    boot_timestamp = int(match.group(1), 16)
                    delta = boot_timestamp - expected_timestamp
                    if 0 <= delta <= args.allowed_delta:
                        print(f"\nboot rtc ok timestamp=0x{boot_timestamp:08X} delta={delta}s")
                        return joined
                    raise RuntimeError(
                        f"boot rtc timestamp mismatch: got=0x{boot_timestamp:08X} expected>=0x{expected_timestamp:08X} delta={delta}s"
                    )
        raise RuntimeError("boot rtc log not observed")
    finally:
        server.terminate()
        try:
            server.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server.kill()


def run_gdb(args: argparse.Namespace, commands: list[str]) -> str:
    cmd = [args.gdb_path, "-q", "build/CprSensor.elf"]
    for command in commands:
        cmd.extend(["-ex", command])
    result = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True)
    return result.stdout


def set_and_verify_rtc_with_gdb(args: argparse.Namespace) -> int:
    timestamp = args.timestamp if args.timestamp is not None else timestamp_from_now()
    rtc_crl = 0x40002804
    rtc_cnth = 0x40002818
    rtc_cntl = 0x4000281C
    server = start_rtt_server(args)
    try:
        output = run_gdb(args, [
            "set confirm off",
            f"target remote :{args.gdb_port}",
            f"set *(unsigned short*)0x{rtc_crl:08X} = *(unsigned short*)0x{rtc_crl:08X} | 0x0010",
            f"set *(unsigned short*)0x{rtc_cnth:08X} = 0x{(timestamp >> 16) & 0xFFFF:04X}",
            f"set *(unsigned short*)0x{rtc_cntl:08X} = 0x{timestamp & 0xFFFF:04X}",
            f"set *(unsigned short*)0x{rtc_crl:08X} = *(unsigned short*)0x{rtc_crl:08X} & 0xFFEF",
            f"p/x cprAlgMgrGetRtcTime()",
            "monitor reset",
            "monitor go",
            "detach",
            "quit",
        ])
        print(output)

        time.sleep(args.gdb_boot_wait)
        output = run_gdb(args, [
            "set confirm off",
            f"target remote :{args.gdb_port}",
            "p/x s_CPR_Data",
            "p/x cprAlgMgrGetRtcTime()",
            "detach",
            "quit",
        ])
        print(output)

        match = re.search(r"BootTimeStamp = 0x([0-9A-Fa-f]+)", output)
        if match is None:
            raise RuntimeError("BootTimeStamp not found in GDB output")

        boot_timestamp = int(match.group(1), 16)
        delta = boot_timestamp - timestamp
        if not (0 <= delta <= args.allowed_delta):
            raise RuntimeError(
                f"boot rtc timestamp mismatch: got=0x{boot_timestamp:08X} expected>=0x{timestamp:08X} delta={delta}s"
            )

        print(f"gdb rtc ok set=0x{timestamp:08X} boot=0x{boot_timestamp:08X} delta={delta}s")
        return timestamp
    finally:
        server.terminate()
        try:
            server.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server.kill()


async def main() -> int:
    parser = argparse.ArgumentParser(description="Set RTC with protocol 0x33, reset once, and verify boot RTC log.")
    parser.add_argument("--address", default=os.environ.get("CPR_BLE_ADDRESS", ""))
    parser.add_argument("--service-uuid", default=DEFAULT_SERVICE_UUID)
    parser.add_argument("--write-char-uuid", default=DEFAULT_WRITE_CHAR_UUID)
    parser.add_argument("--notify-char-uuid", default=DEFAULT_NOTIFY_CHAR_UUID)
    parser.add_argument("--scan-timeout", type=float, default=8.0)
    parser.add_argument("--connect-timeout", type=float, default=10.0)
    parser.add_argument("--reply-timeout", type=float, default=2.0)
    parser.add_argument("--packet-delay", type=float, default=0.5)
    parser.add_argument("--limit", type=int, default=8)
    parser.add_argument("--timestamp", type=lambda value: int(value, 0), default=None)
    parser.add_argument("--allowed-delta", type=int, default=20)
    parser.add_argument("--log-timeout", type=float, default=15.0)
    parser.add_argument("--rtt-timeout", type=float, default=8.0)
    parser.add_argument("--jlink-device", default=os.environ.get("JLINK_DEVICE", "STM32F103RE"))
    parser.add_argument("--jlink-interface", default=os.environ.get("JLINK_INTERFACE", "SWD"))
    parser.add_argument("--jlink-speed", type=int, default=int(os.environ.get("JLINK_SPEED_KHZ", "4000")))
    parser.add_argument("--gdb-port", type=int, default=3331)
    parser.add_argument("--swo-port", type=int, default=3332)
    parser.add_argument("--telnet-port", type=int, default=3333)
    parser.add_argument("--rtt-port", type=int, default=19021)
    parser.add_argument("--jlink-exe", default="JLinkExe")
    parser.add_argument("--jlink-gdb-server", default="JLinkGDBServer")
    parser.add_argument("--gdb-path", default=".tools/arm-gnu-toolchain/bin/arm-none-eabi-gdb")
    parser.add_argument("--gdb-boot-wait", type=float, default=3.0)
    parser.add_argument("--gdb-only", action="store_true")
    parser.add_argument("--allow-gdb-fallback", action="store_true")
    args = parser.parse_args()

    if args.gdb_only:
        set_and_verify_rtc_with_gdb(args)
        return 0

    try:
        _, timestamp = await set_rtc_over_ble(args)
        capture_boot_log(args, timestamp)
    except Exception:
        if not args.allow_gdb_fallback:
            raise
        set_and_verify_rtc_with_gdb(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
