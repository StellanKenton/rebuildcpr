import argparse
import asyncio
import hashlib
from dataclasses import dataclass

from bleak import BleakClient, BleakScanner
from Crypto.Cipher import AES

DEFAULT_SERVICE_UUID = "0000fe60-0000-1000-8000-00805f9b34fb"
DEFAULT_WRITE_CHAR_UUID = "0000fe61-0000-1000-8000-00805f9b34fb"
DEFAULT_NOTIFY_CHAR_UUID = "0000fe62-0000-1000-8000-00805f9b34fb"

CMD_HANDSHAKE = 0x01
CMD_HEARTBEAT = 0x03
CMD_GET_DEVICE_INFO = 0x11
CMD_GET_BLE_INFO = 0x13


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


def build_handshake_payload(device_address: str) -> bytes:
    address_hex = device_address.replace(":", "").replace("-", "")
    seed = bytes.fromhex(address_hex)
    aes_key = hashlib.md5(seed).digest()
    cipher = AES.new(aes_key, AES.MODE_ECB)
    return cipher.encrypt(seed.ljust(16, b"\x00"))


def parse_frame(data: bytes) -> str:
    if len(data) < 8:
        return f"short:{data.hex()}"
    if data[0] != 0xFA or data[1] != 0xFC:
        return f"raw:{data.hex()}"

    cmd = data[3]
    payload_len = (data[4] << 8) | data[5]
    frame_len = 6 + payload_len + 2
    if len(data) < frame_len:
        return f"truncated:{data.hex()}"

    crc_rx = (data[6 + payload_len] << 8) | data[7 + payload_len]
    crc_calc = crc16_ccitt(data[3:6 + payload_len])
    payload = data[6:6 + payload_len]
    return f"cmd=0x{cmd:02X} len={payload_len} crc_ok={crc_rx == crc_calc} payload={payload.hex()}"


@dataclass
class Candidate:
    device: object
    advertisement: object


async def probe_candidate(candidate: Candidate, service_uuid: str, write_char_uuid: str, notify_char_uuid: str, timeout: float) -> bool:
    notifications = []

    def handle_notification(_: object, data: bytearray) -> None:
        notifications.append(bytes(data))
        print(f"notify {candidate.device.address}: {parse_frame(bytes(data))}")

    try:
        async with BleakClient(candidate.device, timeout=timeout) as client:
            print(f"connected address={candidate.device.address} name={candidate.device.name!r} rssi={candidate.advertisement.rssi}")
            for service in client.services:
                print(f"  service {service.uuid}")
                for characteristic in service.characteristics:
                    print(f"    char {characteristic.uuid} props={list(characteristic.properties)}")

            target_char = None
            for service in client.services:
                for characteristic in service.characteristics:
                    if characteristic.uuid.lower() == write_char_uuid.lower():
                        target_char = characteristic.uuid
                        break
                if target_char is not None:
                    break

            if target_char is None:
                for service in client.services:
                    for characteristic in service.characteristics:
                        if "write" in characteristic.properties or "write-without-response" in characteristic.properties:
                            target_char = characteristic.uuid
                            break
                    if target_char is not None:
                        break

            notify_char = None
            for service in client.services:
                for characteristic in service.characteristics:
                    if characteristic.uuid.lower() == notify_char_uuid.lower() and "notify" in characteristic.properties:
                        notify_char = characteristic.uuid
                        break
                if notify_char is not None:
                    break

            if notify_char is None:
                for service in client.services:
                    for characteristic in service.characteristics:
                        if "notify" in characteristic.properties:
                            notify_char = characteristic.uuid
                            break
                    if notify_char is not None:
                        break

            if notify_char is not None:
                await client.start_notify(notify_char, handle_notification)

            if target_char is None:
                print("  no writable characteristic")
                return False

            handshake = build_frame(CMD_HANDSHAKE, build_handshake_payload(candidate.device.address))
            heartbeat = build_frame(CMD_HEARTBEAT, b"")
            device_info = build_frame(CMD_GET_DEVICE_INFO, b"")
            ble_info = build_frame(CMD_GET_BLE_INFO, b"")

            for label, packet in (("handshake", handshake), ("heartbeat", heartbeat), ("device-info", device_info), ("ble-info", ble_info)):
                print(f"  send {label}: {packet.hex()}")
                await client.write_gatt_char(target_char, packet)
                await asyncio.sleep(1.0)

            if notify_char is not None:
                await asyncio.sleep(2.0)
                await client.stop_notify(notify_char)

            return any(item.startswith(b"\xFA\xFC") for item in notifications)
    except asyncio.CancelledError as exc:
        print(f"probe-fail address={candidate.device.address} error=CancelledError: {exc}")
        return False
    except Exception as exc:
        print(f"probe-fail address={candidate.device.address} error={type(exc).__name__}: {exc}")
        return False


async def main() -> int:
    parser = argparse.ArgumentParser(description="Probe nearby BLE devices against the current CPR sensor protocol")
    parser.add_argument("--service-uuid", default=DEFAULT_SERVICE_UUID)
    parser.add_argument("--write-char-uuid", default=DEFAULT_WRITE_CHAR_UUID)
    parser.add_argument("--notify-char-uuid", default=DEFAULT_NOTIFY_CHAR_UUID)
    parser.add_argument("--scan-timeout", type=float, default=8.0)
    parser.add_argument("--connect-timeout", type=float, default=10.0)
    parser.add_argument("--limit", type=int, default=12)
    parser.add_argument("--address", default="", help="Probe only a specific BLE address")
    args = parser.parse_args()

    discovered = await BleakScanner.discover(timeout=args.scan_timeout, return_adv=True)
    candidates = []
    for _, payload in discovered.items():
        device, advertisement = payload
        if args.address and device.address.lower() != args.address.lower():
            continue
        candidates.append(Candidate(device=device, advertisement=advertisement))

    print(f"discovered {len(candidates)} candidate(s)")
    for candidate in candidates[:args.limit]:
        uuids = list(candidate.advertisement.service_uuids or [])
        print(f"candidate address={candidate.device.address} name={candidate.device.name!r} rssi={candidate.advertisement.rssi} uuids={uuids}")

    for candidate in candidates[:args.limit]:
        if await probe_candidate(candidate,
                                 args.service_uuid,
                                 args.write_char_uuid,
                                 args.notify_char_uuid,
                                 args.connect_timeout):
            print(f"match address={candidate.device.address}")
            return 0

    print("no protocol match found")
    return 1


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))