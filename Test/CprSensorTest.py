import asyncio
import threading
import time
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox, filedialog
from bleak import BleakScanner, BleakClient
from bleak.backends.characteristic import BleakGATTCharacteristic
from Crypto.Cipher import AES
import binascii
import struct
import datetime
import socket
import select
import hashlib
import zlib
import os

DEFAULT_SERVICE_UUID = "0000fe60-0000-1000-8000-00805f9b34fb"
DEFAULT_WRITE_CHAR_UUID = "0000fe61-0000-1000-8000-00805f9b34fb"
DEFAULT_NOTIFY_CHAR_UUID = "0000fe62-0000-1000-8000-00805f9b34fb"
DEFAULT_CHAR_UUID = DEFAULT_WRITE_CHAR_UUID
TEST_HANDSHAKE_MAC_ADDRESS = "3C:1A:CC:4B:26:B2"

CMD_HANDSHAKE = 0x01
CMD_HEARTBEAT = 0x03
CMD_DISCONNECT = 0x04
CMD_GET_DEVICE_INFO = 0x11
CMD_GET_BLE_INFO = 0x13
CMD_TIME_SYNC = 0x33

COMMAND_SPECS = {
    0x01: {"name": "HANDSHAKE", "zh": "握手", "hint": "mac=3C1ACC4B26B2 或 6字节HEX"},
    0x03: {"name": "HEARTBEAT", "zh": "心跳", "hint": "空"},
    0x04: {"name": "DISCONNECT", "zh": "断开", "hint": "空"},
    0x05: {"name": "SELF_CHECK", "zh": "自检", "hint": "空"},
    0x11: {"name": "DEV_INFO", "zh": "设备信息", "hint": "空"},
    0x13: {"name": "BLE_INFO", "zh": "BLE信息", "hint": "空"},
    0x14: {"name": "WIFI_SETTING", "zh": "WiFi设置", "hint": "ssid=xxx,pwd=xxx 或 HEX"},
    0x15: {"name": "COMM_SETTING", "zh": "通信设置", "hint": "priority=0(BLE)/1(WIFI) 或 00/01"},
    0x16: {"name": "TCP_SETTING", "zh": "TCP设置", "hint": "ip=192.168.1.10,port=8080 或 HEX"},
    0x30: {"name": "UPLOAD_METHOD", "zh": "上传方式", "hint": "uploadMethod=0/1 或 00/01"},
    0x31: {"name": "CPR_DATA", "zh": "CPR数据", "hint": "通常为设备上传"},
    0x33: {"name": "TIME_SYNC", "zh": "时间同步", "hint": "worldTime=秒 或 4字节HEX；空则发送当前时间"},
    0x34: {"name": "BATTERY", "zh": "电池", "hint": "空"},
    0x35: {"name": "LANGUAGE", "zh": "语言", "hint": "language=0/1 或 00/01"},
    0x36: {"name": "VOLUME", "zh": "音量", "hint": "volume=0-255 或 1字节HEX"},
    0x37: {"name": "CPR_RAW_DATA", "zh": "CPR原始波形", "hint": "通常为设备上传"},
    0x38: {"name": "CLEAR_MEMORY", "zh": "清除存储", "hint": "空"},
    0x39: {"name": "BOOT_TIME", "zh": "开机时间", "hint": "bootTime=秒 或 4字节HEX"},
    0x3A: {"name": "METRONOME", "zh": "节拍器", "hint": "metronomeFreq=100 或 1字节HEX"},
}

COMMAND_CHOICES = [
    f"0x{cmd:02X} {spec['name']} {spec['zh']}"
    for cmd, spec in sorted(COMMAND_SPECS.items())
]

class BLEApp:
    def __init__(self, root):
        self.root = root
        self.root.title("BLE设备管理工具")
        self.root.geometry("800x600")
        
        # BLE管理器
        self.manager = BLEDeviceManager(self.update_received_data)
        
        # 创建UI
        self.create_widgets()
        
        # 启动事件循环的线程
        self.loop_thread = threading.Thread(target=self.run_loop, daemon=True)
        self.loop_thread.start()
        # 启动工作线程
        self.work_thread = threading.Thread(target=self.work_loop, daemon=True)
        self.work_thread.start()
        
        # 启动TCP服务器
        self.tcp_thread = threading.Thread(target=self.start_tcp_server, daemon=True)
        self.tcp_thread.start()
        
        # 定期检查连接状态
        self.update_connection_status()
    
    def create_widgets(self):
        # 主框架
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # 扫描区域
        scan_frame = ttk.LabelFrame(main_frame, text="设备扫描", padding="5")
        scan_frame.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5) 
        
        ttk.Button(scan_frame, text="扫描设备", command=self.scan_devices).grid(row=0, column=0, padx=5)
        self.scan_status = ttk.Label(scan_frame, text="就绪")
        self.scan_status.grid(row=0, column=1, padx=5)
        
        # 设备列表
        devices_frame = ttk.LabelFrame(main_frame, text="可用设备", padding="5")
        devices_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S), pady=5)
        
        self.devices_listbox = tk.Listbox(devices_frame, height=6)
        self.devices_listbox.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        scrollbar = ttk.Scrollbar(devices_frame, orient=tk.VERTICAL, command=self.devices_listbox.yview)
        scrollbar.grid(row=0, column=1, sticky=(tk.N, tk.S))
        self.devices_listbox.configure(yscrollcommand=scrollbar.set)
        
        # 连接控制
        connect_frame = ttk.Frame(main_frame)
        connect_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        ttk.Button(connect_frame, text="连接设备", command=self.connect_device).grid(row=0, column=0, padx=5)
        ttk.Button(connect_frame, text="断开连接", command=self.disconnect_device).grid(row=0, column=1, padx=5)
        ttk.Button(connect_frame, text="OTA升级", command=self.open_ota_window).grid(row=0, column=2, padx=5)
        self.connection_status = ttk.Label(connect_frame, text="未连接")
        self.connection_status.grid(row=0, column=3, padx=5)
        
        # TCP状态
        tcp_frame = ttk.Frame(main_frame)
        tcp_frame.grid(row=2, column=3, sticky=(tk.W, tk.E), pady=5)
        
        self.tcp_status = ttk.Label(tcp_frame, text="TCP: 未连接")
        self.tcp_status.grid(row=0, column=0, padx=5)
        
        # 数据接收区域
        receive_frame = ttk.LabelFrame(main_frame, text="接收数据", padding="5")
        receive_frame.grid(row=3, column=0, columnspan=4, sticky=(tk.W, tk.E, tk.N, tk.S), pady=5)
        
        self.received_data = scrolledtext.ScrolledText(receive_frame, height=10)
        self.received_data.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # 数据发送区域
        send_frame = ttk.LabelFrame(main_frame, text="发送数据", padding="5")
        send_frame.grid(row=4, column=0, columnspan=4, sticky=(tk.W, tk.E), pady=5)
        
        # 发送类型选择
        send_type_frame = ttk.Frame(send_frame)
        send_type_frame.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=2)
        
        ttk.Label(send_type_frame, text="发送类型:").grid(row=0, column=0)
        self.send_type = tk.StringVar(value="raw")
        ttk.Radiobutton(send_type_frame, text="加密数据", variable=self.send_type, value="encrypted").grid(row=0, column=1, padx=5)
        ttk.Radiobutton(send_type_frame, text="原始数据", variable=self.send_type, value="raw").grid(row=0, column=2, padx=5)
        
        # 命令选择
        ttk.Label(send_type_frame, text="命令:").grid(row=0, column=3, padx=(10, 0))
        self.command_entry = ttk.Entry(send_type_frame, width=4)
        self.command_entry.grid(row=0, column=4, padx=2)
        self.command_entry.insert(0, f"{CMD_GET_DEVICE_INFO:02X}")
        self.command_combo = ttk.Combobox(send_type_frame, values=COMMAND_CHOICES, width=28, state="readonly")
        self.command_combo.grid(row=0, column=5, padx=5)
        self.command_combo.set(self.format_command_choice(CMD_GET_DEVICE_INFO))
        self.command_combo.bind("<<ComboboxSelected>>", self.command_selected)
        
        # 数据输入框
        self.send_data_entry = ttk.Entry(send_frame, width=50)
        self.send_data_entry.grid(row=1, column=0, padx=5, pady=5)
        self.send_data_entry.bind("<Return>", self.send_data_enter)
        
        ttk.Button(send_frame, text="发送", command=self.send_data).grid(row=1, column=1, padx=5)
        self.payload_hint = ttk.Label(send_frame, text=self.get_payload_hint(CMD_GET_DEVICE_INFO))
        self.payload_hint.grid(row=2, column=0, columnspan=2, sticky=tk.W, padx=5)
        
        # UUID设置
        uuid_frame = ttk.Frame(main_frame)
        uuid_frame.grid(row=5, column=0, columnspan=4, sticky=(tk.W, tk.E), pady=5)
        
        ttk.Label(uuid_frame, text="服务UUID:").grid(row=0, column=0)
        self.service_uuid_entry = ttk.Entry(uuid_frame, width=40)
        self.service_uuid_entry.grid(row=0, column=1, padx=5)
        self.service_uuid_entry.insert(0, DEFAULT_SERVICE_UUID)
        
        ttk.Label(uuid_frame, text="特性UUID:").grid(row=0, column=2)
        self.char_uuid_entry = ttk.Entry(uuid_frame, width=40)
        self.char_uuid_entry.grid(row=0, column=3, padx=5)
        self.char_uuid_entry.insert(0, DEFAULT_CHAR_UUID)
        
        # 配置权重
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(1, weight=1)
        main_frame.rowconfigure(3, weight=1)
        devices_frame.columnconfigure(0, weight=1)
        devices_frame.rowconfigure(0, weight=1)
        receive_frame.columnconfigure(0, weight=1)
        receive_frame.rowconfigure(0, weight=1)

    def format_command_choice(self, cmd):
        spec = COMMAND_SPECS.get(cmd)
        if not spec:
            return f"0x{cmd:02X}"
        return f"0x{cmd:02X} {spec['name']} {spec['zh']}"

    def get_payload_hint(self, cmd):
        spec = COMMAND_SPECS.get(cmd, {})
        return f"内容格式: {spec.get('hint', 'HEX')}"

    def command_selected(self, event=None):
        choice = self.command_combo.get()
        try:
            cmd = int(choice.split()[0], 16)
        except (ValueError, IndexError):
            return

        self.command_entry.delete(0, tk.END)
        self.command_entry.insert(0, f"{cmd:02X}")
        self.payload_hint.config(text=self.get_payload_hint(cmd))
    
    def run_loop(self):
        asyncio.set_event_loop(self.manager.loop)
        self.manager.loop.run_forever()
    
    def send_hand_shake(self):
        handshake_data = self.manager.create_handshake_packet()
        if handshake_data:
            asyncio.run_coroutine_threadsafe(
                self.manager.send_data(handshake_data, self.service_uuid_entry.get() or DEFAULT_SERVICE_UUID, self.char_uuid_entry.get() or DEFAULT_CHAR_UUID),
                self.manager.loop
            )
    
    def send_heartbeat(self):
        heartbeat_data = self.manager.create_heartbeat_packet()
        if heartbeat_data:
            asyncio.run_coroutine_threadsafe(
                self.manager.send_data(heartbeat_data, self.service_uuid_entry.get() or DEFAULT_SERVICE_UUID, self.char_uuid_entry.get() or DEFAULT_CHAR_UUID),
                self.manager.loop
            )
    
    def work_loop(self):
        handshake_sent = False
        next_handshake_time = 0.0
        next_heartbeat_time = 0.0
        while True:
            time.sleep(0.05)
            is_connected = self.manager.client and self.manager.client.is_connected
            now = time.time()

            if not is_connected:
                handshake_sent = False
                next_handshake_time = 0.0
                next_heartbeat_time = 0.0
                continue

            if not self.manager.handshake_completed:
                next_heartbeat_time = 0.0
                if (not handshake_sent) or (now >= next_handshake_time):
                    self.send_hand_shake()
                    handshake_sent = True
                    next_handshake_time = now + 3.0
                continue

            handshake_sent = False
            if next_heartbeat_time == 0.0:
                next_heartbeat_time = now + 1.0
            elif now >= next_heartbeat_time:
                self.send_heartbeat()
                next_heartbeat_time = now + 1.0
                
    def start_tcp_server(self):
        """启动TCP服务器"""
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(('0.0.0.0', 8080))
        server.listen(1)
        server.setblocking(False)
        
        self.update_received_data("TCP服务器已启动，监听端口 8080")
        
        inputs = [server]
        outputs = []
        
        while True:
            readable, writable, exceptional = select.select(inputs, outputs, inputs, 1)
            
            for s in readable:
                if s is server:
                    # 有新的TCP连接
                    client_socket, addr = s.accept()
                    client_socket.setblocking(False)
                    
                    # 断开蓝牙连接
                    self.disconnect_device()
                    
                    # 更新TCP状态
                    self.update_tcp_status(f"TCP: 已连接 ({addr[0]}:{addr[1]})")
                    self.update_received_data(f"TCP客户端已连接: {addr[0]}:{addr[1]}")
                    
                    # 将新连接添加到输入列表
                    inputs.append(client_socket)
                    self.manager.tcp_client = client_socket
                else:
                    # 处理TCP数据
                    try:
                        data = s.recv(1024)
                        if data:
                            # 处理接收到的TCP数据
                            self.manager.handle_tcp_data(data)
                        else:
                            # 客户端断开连接
                            self.update_tcp_status("TCP: 未连接")
                            self.update_received_data("TCP客户端已断开")
                            inputs.remove(s)
                            s.close()
                            self.manager.tcp_client = None
                    except Exception as e:
                        self.update_received_data(f"TCP接收错误: {e}")
                        inputs.remove(s)
                        s.close()
                        self.manager.tcp_client = None
                        self.update_tcp_status("TCP: 未连接")
            
            for s in exceptional:
                self.update_received_data(f"TCP连接异常: {s.getpeername()}")
                inputs.remove(s)
                s.close()
                self.manager.tcp_client = None
                self.update_tcp_status("TCP: 未连接")
    
    def scan_devices(self):
        self.scan_status.config(text="扫描中...")
        self.devices_listbox.delete(0, tk.END)
        self.devices_listbox.insert(tk.END, "扫描中，请等待...")
        
        def scan_complete(future):
            try:
                devices = future.result()
                self.devices_listbox.delete(0, tk.END)
                for i, entry in devices.items():
                    device = entry["device"]
                    if not device.name:
                        continue
                    advertisement = entry["advertisement"]
                    name = device.name
                    uuids = ",".join(advertisement.service_uuids or [])
                    suffix = f" RSSI={advertisement.rssi}"
                    if uuids:
                        suffix += f" UUID={uuids}"
                    self.devices_listbox.insert(tk.END, f"{i}: {name} - {device.address}{suffix}")
                self.scan_status.config(text=f"找到 {len(devices)} 个设备")
            except Exception as e:
                self.devices_listbox.delete(0, tk.END)
                self.devices_listbox.insert(tk.END, f"扫描错误: {e}")
                self.scan_status.config(text="扫描失败")
        
        future = asyncio.run_coroutine_threadsafe(self.manager.scan_devices(), self.manager.loop)
        future.add_done_callback(scan_complete)
    
    def connect_device(self):
        selection = self.devices_listbox.curselection()
        if not selection:
            self.update_received_data("请先选择一个设备")
            return
        
        item = self.devices_listbox.get(selection[0])
        try:
            index = int(item.split(":")[0])
        except:
            self.update_received_data("无法解析设备索引")
            return
        
        def connect_complete(future):
            try:
                success = future.result()
                if success:
                    self.connection_status.config(text="已连接")
                    self.update_received_data("连接成功!")
                else:
                    self.connection_status.config(text="连接失败")
                    self.update_received_data("连接失败")
            except Exception as e:
                self.connection_status.config(text="连接错误")
                self.update_received_data(f"连接错误: {e}")
        
        self.update_received_data("正在连接...")
        future = asyncio.run_coroutine_threadsafe(self.manager.connect_device(index), self.manager.loop)
        future.add_done_callback(connect_complete)
    
    def disconnect_device(self):
        def disconnect_complete(future):
            try:
                future.result()
                self.connection_status.config(text="未连接")
                self.update_received_data("已断开连接")
            except Exception as e:
                self.update_received_data(f"断开连接错误: {e}")
        
        future = asyncio.run_coroutine_threadsafe(self.manager.disconnect_device(), self.manager.loop)
        future.add_done_callback(disconnect_complete)

    def parse_key_value_payload(self, data):
        items = {}
        for part in data.replace(";", ",").split(","):
            if "=" not in part:
                continue
            key, value = part.split("=", 1)
            items[key.strip()] = value.strip()
        return items

    def parse_int_value(self, text):
        text = text.strip()
        if text.lower().startswith("0x"):
            return int(text, 16)
        return int(text, 10)

    def parse_payload_by_command(self, cmd, data):
        if not data:
            if cmd == CMD_TIME_SYNC:
                return self.manager.create_time_sync_payload()
            return b''

        kv = self.parse_key_value_payload(data)
        try:
            if cmd == 0x01 and "mac" in kv:
                return bytes.fromhex(kv["mac"].replace(":", "").replace("-", "").replace(" ", ""))
            if cmd == 0x14 and ("ssid" in kv or "pwd" in kv):
                ssid = kv.get("ssid", "").encode("utf-8")
                pwd = kv.get("pwd", "").encode("utf-8")
                if len(ssid) > 255 or len(pwd) > 255:
                    raise ValueError("ssid/pwd长度不能超过255字节")
                return bytes([len(ssid)]) + ssid + bytes([len(pwd)]) + pwd
            if cmd == 0x15 and "priority" in kv:
                return bytes([self.parse_int_value(kv["priority"]) & 0xFF])
            if cmd == 0x16 and ("ip" in kv or "port" in kv):
                ip = kv.get("ip", "").encode("ascii")
                port = str(kv.get("port", "")).encode("ascii")
                if len(ip) > 255 or len(port) > 255:
                    raise ValueError("ip/port长度不能超过255字节")
                return bytes([len(ip)]) + ip + bytes([len(port)]) + port
            if cmd == 0x30 and "uploadMethod" in kv:
                return bytes([self.parse_int_value(kv["uploadMethod"]) & 0xFF])
            if cmd == 0x33 and "worldTime" in kv:
                return self.parse_int_value(kv["worldTime"]).to_bytes(4, byteorder="big", signed=False)
            if cmd == 0x35 and "language" in kv:
                return bytes([self.parse_int_value(kv["language"]) & 0xFF])
            if cmd == 0x36 and "volume" in kv:
                return bytes([self.parse_int_value(kv["volume"]) & 0xFF])
            if cmd == 0x39 and "bootTime" in kv:
                return self.parse_int_value(kv["bootTime"]).to_bytes(4, byteorder="big", signed=False)
            if cmd == 0x3A and "metronomeFreq" in kv:
                return bytes([self.parse_int_value(kv["metronomeFreq"]) & 0xFF])
        except ValueError as e:
            raise ValueError(f"变量内容无效: {e}") from e

        return bytes.fromhex(data.replace(" ", ""))
    
    def send_data(self):
        data = self.send_data_entry.get().strip()
        
        # 获取命令
        try:
            cmd = int(self.command_entry.get().strip(), 16)
        except ValueError:
            messagebox.showerror("错误", "命令必须是有效的十六进制数")
            return

        payload_bytes = b''
        
        # 根据发送类型处理数据
        if self.send_type.get() == "encrypted":
            if data:
                # 处理加密数据
                try:
                    payload_bytes = self.parse_payload_by_command(cmd, data)
                    # 创建数据包
                    encrypted_data = self.manager.create_data_packet(payload_bytes, cmd, encrypt=True)
                    if not encrypted_data:
                        messagebox.showerror("错误", "创建数据包失败")
                        return
                    
                    data_to_send = encrypted_data
                    display_data = self.manager.format_sent_data(cmd, payload_bytes, encrypted_data, "加密数据")
                except ValueError as e:
                    messagebox.showerror("错误", f"请输入有效内容: {e}")
                    return
            else:
                try:
                    payload_bytes = self.parse_payload_by_command(cmd, data)
                    encrypted_data = self.manager.create_data_packet(payload_bytes, cmd, encrypt=True)
                    if not encrypted_data:
                        messagebox.showerror("错误", "创建数据包失败")
                        return
                    
                    data_to_send = encrypted_data
                    display_data = self.manager.format_sent_data(cmd, payload_bytes, encrypted_data, "命令包")
                except Exception as e:
                    messagebox.showerror("错误", f"创建命令包失败: {e}")
                    return
        else:
            # 处理原始数据
            try:
                payload_bytes = self.parse_payload_by_command(cmd, data)
                data_to_send = self.manager.create_data_packet(payload_bytes, cmd, encrypt=False)
                if not data_to_send:
                    messagebox.showerror("错误", "创建原始协议包失败")
                    return
                display_data = self.manager.format_sent_data(cmd, payload_bytes, data_to_send, "原始协议包")
            except ValueError as e:
                messagebox.showerror("错误", f"请输入有效内容: {e}")
                return
        
        # 根据当前连接类型发送数据
        if self.manager.tcp_client:
            # 通过TCP发送
            try:
                # 创建TCP数据包（不加密）
                tcp_packet = self.manager.create_data_packet(payload_bytes, cmd, encrypt=False)
                if not tcp_packet:
                    messagebox.showerror("错误", "创建TCP数据包失败")
                    return
                
                # 发送TCP数据包
                self.manager.tcp_client.sendall(tcp_packet)
                self.update_received_data(f"已通过TCP发送: {display_data}")
                self.send_data_entry.delete(0, tk.END)
            except Exception as e:
                self.update_received_data(f"TCP发送错误: {e}")
        else:
            # 通过蓝牙发送
            if cmd != CMD_HANDSHAKE and not self.manager.handshake_completed:
                self.update_received_data("尚未收到固件握手MAC回复，已拦截非握手命令")
                return

            service_uuid = self.service_uuid_entry.get() or None
            char_uuid = self.char_uuid_entry.get() or None
            
            def send_complete(future):
                try:
                    success = future.result()
                    if success:
                        self.update_received_data(f"已发送: {display_data}")
                        self.send_data_entry.delete(0, tk.END)
                    else:
                        self.update_received_data("发送失败")
                except Exception as e:
                    self.update_received_data(f"发送错误: {e}")
            
            future = asyncio.run_coroutine_threadsafe(
                self.manager.send_data(data_to_send, service_uuid, char_uuid), 
                self.manager.loop
            )
            future.add_done_callback(send_complete)
    
    def send_data_enter(self, event):
        self.send_data()
    
    def update_received_data(self, text):
        def update():
            self.received_data.insert(tk.END, text + "\n")
            self.received_data.see(tk.END)
        
        self.root.after(0, update)
    
    def update_tcp_status(self, text):
        def update():
            self.tcp_status.config(text=text)
        
        self.root.after(0, update)
    
    def update_connection_status(self):
        def check_connection():
            if self.manager.client and self.manager.client.is_connected:
                self.connection_status.config(text="已连接")
            else:
                self.connection_status.config(text="未连接")
            
            # 每隔1秒检查一次
            self.root.after(1000, check_connection)
        
        check_connection()
    
    def open_ota_window(self):
        """打开OTA升级窗口"""
        if not hasattr(self, 'ota_window') or not self.ota_window.winfo_exists():
            self.ota_window = OTAWindow(self.root, self.manager)

class BLEDeviceManager:
    def __init__(self, update_callback):
        self.devices = {}
        self.connected_device = None
        self.client = None
        self.loop = asyncio.new_event_loop()
        self.notification_characteristic = None
        self.write_characteristic_uuid = DEFAULT_WRITE_CHAR_UUID
        self.update_callback = update_callback
        self.aes_key = b'0011223344556677'  # 改为字节类型
        self.handshake_completed = False
        self.tcp_client = None  # TCP客户端套接字

    def get_handshake_seed(self):
        fixed_seed = self.parse_mac_seed(TEST_HANDSHAKE_MAC_ADDRESS)
        if fixed_seed is not None:
            self.update_callback(f"使用固定握手MAC: {TEST_HANDSHAKE_MAC_ADDRESS}")
            return fixed_seed

        if not self.connected_device:
            return None

        address_hex = self.connected_device.address.replace(':', '').replace('-', '')
        if len(address_hex) == 12:
            return bytes.fromhex(address_hex)

        device_name = self.connected_device.name or ""
        if device_name.startswith("PRIMEDIC-CPRSensor-"):
            suffix = device_name.rsplit('-', 1)[-1]
            if len(suffix) == 6:
                try:
                    return bytes.fromhex(suffix)
                except ValueError:
                    return None

        return None

    def parse_mac_seed(self, mac_text):
        if not mac_text:
            return None

        address_hex = mac_text.replace(':', '').replace('-', '').replace(' ', '')
        if len(address_hex) != 12:
            return None

        try:
            return bytes.fromhex(address_hex)
        except ValueError:
            return None

    def create_heartbeat_packet(self):
        return self.create_data_packet(b'', CMD_HEARTBEAT, encrypt=False)

    def create_time_sync_payload(self):
        CST = datetime.timezone(datetime.timedelta(hours=8))
        RTC_BASE = datetime.datetime(2025, 1, 1, tzinfo=CST)
        timestamp = int((datetime.datetime.now(CST) - RTC_BASE).total_seconds()) & 0xFFFFFFFF
        self.update_callback(f"时间同步: {timestamp}")
        return timestamp.to_bytes(4, byteorder='big')

    def create_time_sync_packet(self):
        payload = self.create_time_sync_payload()
        return self.create_data_packet(payload, CMD_TIME_SYNC, encrypt=True)
    
    # 扫描BLE设备
    async def scan_devices(self, timeout=5):
        self.update_callback("开始扫描BLE设备...")
        self.devices = {}
        devices = await BleakScanner.discover(timeout=timeout, return_adv=True)

        indexed_devices = {}
        for index, (address, payload) in enumerate(devices.items()):
            device, advertisement = payload
            indexed_devices[index] = {
                "device": device,
                "advertisement": advertisement,
                "address": address,
            }

        self.devices = indexed_devices
        return self.devices
    
    # 连接设备
    async def connect_device(self, device_index):
        if device_index not in self.devices:
            self.update_callback("设备索引无效")
            return False
        
        entry = self.devices[device_index]
        device = entry["device"]
        self.update_callback(f"正在连接: {device.name} ({device.address})")

        self.client = BleakClient(device, loop=self.loop)
        try:
            await self.client.connect()
            self.connected_device = device
            self.notification_characteristic = None
            self.write_characteristic_uuid = DEFAULT_WRITE_CHAR_UUID
            
            services = self.client.services
            self.update_callback("发现的服务:")
            for service in services:
                self.update_callback(f"服务: {service.uuid}")
                for char in service.characteristics:
                    self.update_callback(f"  特性: {char.uuid} - 属性: {char.properties}")
                    
                    if char.uuid.lower() == DEFAULT_WRITE_CHAR_UUID.lower() and (
                        "write" in char.properties or "write-without-response" in char.properties
                    ):
                        self.write_characteristic_uuid = char.uuid

                    if char.uuid.lower() == DEFAULT_NOTIFY_CHAR_UUID.lower() and "notify" in char.properties:
                        self.notification_characteristic = char
                        self.update_callback(f"找到通知特性: {char.uuid}")

            if self.notification_characteristic is None:
                for service in services:
                    for char in service.characteristics:
                        if "notify" in char.properties:
                            self.notification_characteristic = char
                            self.update_callback(f"找到通知特性: {char.uuid}")
                            break
                    if self.notification_characteristic is not None:
                        break
            
            # 如果找到通知特性，启用通知
            if self.notification_characteristic:
                await self.client.start_notify(
                    self.notification_characteristic.uuid, 
                    self.notification_handler
                )
                self.update_callback("已启用通知")
            
            self.update_callback("连接成功!")
            return True
        except Exception as e:
            self.update_callback(f"连接失败: {e}")
            return False
    
    # 断开连接
    async def disconnect_device(self):
        if self.client and self.client.is_connected:
            if self.notification_characteristic:
                await self.client.stop_notify(self.notification_characteristic.uuid)
            await self.client.disconnect()
            self.update_callback("已断开连接")
        self.connected_device = None
        self.client = None
        self.handshake_completed = False
    
    def crc16_compute(self,data):
        crc = 0xFFFF
        polynomial = 0x1021

        for byte in data:
            crc ^= byte << 8
            for _ in range(8):
                if crc & 0x8000:
                    crc = (crc << 1) ^ polynomial
                else:
                    crc <<= 1
                crc &= 0xFFFF

        return crc
    
    def create_handshake_packet(self):
        if not self.connected_device:
            self.update_callback("未连接设备，无法创建握手包")
            return None
        
        try:
            seed = self.get_handshake_seed()
            if seed is None:
                self.update_callback("无法从设备地址或名称推导握手种子")
                return None

            self.aes_key = hashlib.md5(seed).digest()
            cipher = AES.new(self.aes_key, AES.MODE_ECB)
            encrypted_mac = cipher.encrypt(seed.ljust(16, b'\x00'))
            
            # 3. 构建数据包
            # 包头
            header = bytes([0xFA, 0xFC, 0x01])
            # 命令
            cmd = bytes([CMD_HANDSHAKE])
            # 数据长度 (高字节在前)
            data_len = 16
            len_bytes = bytes([(data_len >> 8) & 0xFF, data_len & 0xFF])
            
            # 4. 计算CRC (从CMD开始到数据结束)
            crc_data = cmd + len_bytes + encrypted_mac
            crc = self.crc16_compute(crc_data)
            crc_bytes = bytes([(crc >> 8) & 0xFF, crc & 0xFF])
            
            # 5. 组合完整数据包
            packet = header + cmd + len_bytes + encrypted_mac + crc_bytes
            self.update_callback(f"握手包: {binascii.hexlify(packet).decode()}")
            return packet
            
        except Exception as e:
            self.update_callback(f"创建握手包失败: {e}")
            return None
    
    def create_data_packet(self, data, cmd=0x02, encrypt=True):
        try:
            orig_data_len = len(data)
            
            if encrypt:
                block_count = (orig_data_len + 15) // 16
                if block_count > 16:
                    block_count = 16
                    data = data[:256]

                padded_len = block_count * 16
                padded_data = data.ljust(padded_len, b'\x00')

                cipher = AES.new(self.aes_key, AES.MODE_ECB)
                encrypted_blocks = []
                for i in range(block_count):
                    block = padded_data[i*16:(i+1)*16]
                    encrypted_block = cipher.encrypt(block)
                    encrypted_blocks.append(encrypted_block)
                
                payload_data = b''.join(encrypted_blocks)
            else:
                if orig_data_len > 48:
                    orig_data_len = 48
                    payload_data = data[:48]
                else:
                    payload_data = data
            
            header = bytes([0xFA, 0xFC, 0x01])
            cmd_byte = bytes([cmd])
            frame_payload_len = len(payload_data)
            len_bytes = bytes([(frame_payload_len >> 8) & 0xFF, frame_payload_len & 0xFF])

            crc_data = cmd_byte + len_bytes + payload_data
            crc = self.crc16_compute(crc_data)
            crc_bytes = bytes([(crc >> 8) & 0xFF, crc & 0xFF])

            packet = header + cmd_byte + len_bytes + payload_data + crc_bytes
            self.update_callback(f"数据包({frame_payload_len}字节): {binascii.hexlify(packet).decode()}")
            return packet
        except Exception as e:
            self.update_callback(f"创建数据包失败: {e}")
            return None

    def get_command_name(self, cmd):
        spec = COMMAND_SPECS.get(cmd)
        if not spec:
            return f"UNKNOWN_0x{cmd:02X}"
        return f"{spec['name']}({spec['zh']})"

    def read_u16_be(self, data, offset):
        return (data[offset] << 8) | data[offset + 1]

    def read_u32_be(self, data, offset):
        return (
            (data[offset] << 24) |
            (data[offset + 1] << 16) |
            (data[offset + 2] << 8) |
            data[offset + 3]
        )

    def decode_ascii(self, data):
        return data.rstrip(b'\x00').decode("utf-8", errors="replace")

    def format_payload_for_display(self, cmd, payload, prefix="上传"):
        cmd_name = self.get_command_name(cmd)
        variables = self.decode_payload_variables(cmd, payload)
        if variables:
            return f"{prefix}: 命令=0x{cmd:02X} {cmd_name}, " + ", ".join(variables)
        return f"{prefix}: 命令=0x{cmd:02X} {cmd_name}, 数据为空"

    def format_sent_data(self, cmd, payload, packet, prefix):
        variables = self.decode_payload_variables(cmd, payload)
        payload_text = ", ".join(variables) if variables else "数据为空"
        return f"{prefix}: 命令=0x{cmd:02X} {self.get_command_name(cmd)}, {payload_text}, 包={packet.hex()}"

    def append_raw_if_extra(self, variables, payload, used_len):
        if len(payload) > used_len:
            variables.append(f"extra={payload[used_len:].hex()}")

    def decode_payload_variables(self, cmd, payload):
        variables = []
        payload_len = len(payload)

        try:
            if cmd == 0x01:
                if payload_len >= 6:
                    variables.append(f"mac={payload[:6].hex(':').upper()}")
                    self.append_raw_if_extra(variables, payload, 6)
            elif cmd in (0x03, 0x04, 0x38):
                if payload_len > 0:
                    variables.append(f"raw={payload.hex()}")
            elif cmd == 0x05:
                if payload_len >= 9:
                    variables.extend([
                        f"feedbackSelfCheck={payload[0]}",
                        f"powerSelfCheck={payload[1]}",
                        f"audioSelfCheck={payload[2]}",
                        f"wirelessSelfCheck={payload[3]}",
                        f"memorySelfCheck={payload[4]}",
                        f"timestamp={self.read_u32_be(payload, 5)}",
                    ])
                    self.append_raw_if_extra(variables, payload, 9)
            elif cmd == 0x11:
                if payload_len >= 18:
                    variables.extend([
                        f"deviceType={payload[0]}",
                        f"deviceSn={self.decode_ascii(payload[1:14])}",
                        f"protocolOrFlag={payload[14]}",
                        f"swVersion={payload[15]}",
                        f"swSubVersion={payload[16]}",
                        f"swBuildVersion={payload[17]}",
                    ])
                    self.append_raw_if_extra(variables, payload, 18)
            elif cmd == 0x13:
                if payload_len >= 1:
                    variables.append(f"bleVersion={self.decode_ascii(payload[:33])}")
                    self.append_raw_if_extra(variables, payload, min(payload_len, 33))
            elif cmd == 0x14:
                if payload_len >= 2:
                    ssid_len = payload[0]
                    if payload_len >= 2 + ssid_len:
                        pwd_len_offset = 1 + ssid_len
                        pwd_len = payload[pwd_len_offset]
                        pwd_offset = pwd_len_offset + 1
                        variables.extend([
                            f"ssidLen={ssid_len}",
                            f"ssid={self.decode_ascii(payload[1:pwd_len_offset])}",
                            f"pwdLen={pwd_len}",
                        ])
                        if payload_len >= pwd_offset + pwd_len:
                            variables.append(f"pwd={self.decode_ascii(payload[pwd_offset:pwd_offset + pwd_len])}")
                            self.append_raw_if_extra(variables, payload, pwd_offset + pwd_len)
            elif cmd == 0x15:
                if payload_len >= 1:
                    variables.append(f"priority={payload[0]}")
                    self.append_raw_if_extra(variables, payload, 1)
            elif cmd == 0x16:
                if payload_len >= 2:
                    ip_len = payload[0]
                    if payload_len >= 2 + ip_len:
                        port_len_offset = 1 + ip_len
                        port_len = payload[port_len_offset]
                        port_offset = port_len_offset + 1
                        variables.extend([
                            f"ipLen={ip_len}",
                            f"ip={self.decode_ascii(payload[1:port_len_offset])}",
                            f"portLen={port_len}",
                        ])
                        if payload_len >= port_offset + port_len:
                            variables.append(f"port={self.decode_ascii(payload[port_offset:port_offset + port_len])}")
                            self.append_raw_if_extra(variables, payload, port_offset + port_len)
            elif cmd == 0x30:
                if payload_len >= 1:
                    variables.append(f"uploadMethod={payload[0]}")
                    self.append_raw_if_extra(variables, payload, 1)
            elif cmd == 0x31:
                if payload_len >= 13:
                    variables.extend([
                        f"timestamp={self.read_u32_be(payload, 0)}",
                        f"freq={self.read_u16_be(payload, 4)}",
                        f"depth={payload[6]}",
                        f"realseDepth={payload[7]}",
                        f"interval={payload[8]}",
                        f"bootTimestamp={self.read_u32_be(payload, 9)}",
                    ])
                    self.append_raw_if_extra(variables, payload, 13)
            elif cmd in (0x33, 0x39):
                if payload_len >= 4:
                    var_name = "worldTime" if cmd == 0x33 else "bootTime"
                    variables.append(f"{var_name}={self.read_u32_be(payload, 0)}")
                    self.append_raw_if_extra(variables, payload, 4)
            elif cmd == 0x34:
                if payload_len >= 4:
                    variables.extend([
                        f"batPercent={payload[0]}",
                        f"batMv={self.read_u16_be(payload, 1)}",
                        f"chargeState={payload[3]}",
                    ])
                    self.append_raw_if_extra(variables, payload, 4)
            elif cmd == 0x35:
                if payload_len >= 1:
                    variables.append(f"language={payload[0]}")
                    self.append_raw_if_extra(variables, payload, 1)
            elif cmd == 0x36:
                if payload_len >= 1:
                    variables.append(f"volume={payload[0]}")
                    self.append_raw_if_extra(variables, payload, 1)
            elif cmd == 0x37:
                if payload_len >= 1:
                    variables.append(f"rawWave={payload[:8].hex()}")
                    self.append_raw_if_extra(variables, payload, min(payload_len, 8))
            elif cmd == 0x3A:
                if payload_len >= 1:
                    variables.append(f"metronomeFreq={payload[0]}")
                    self.append_raw_if_extra(variables, payload, 1)
        except Exception:
            variables = []

        if not variables and payload_len > 0:
            variables.append(f"raw={payload.hex()}")
        return variables

    def expected_payload_len(self, cmd):
        expected_lens = {
            0x01: 6,
            0x03: 0,
            0x04: 0,
            0x11: 0,
            0x13: 0,
            0x15: 1,
            0x33: 4,
            0x34: 0,
            0x35: 1,
            0x36: 1,
            0x38: 0,
            0x3A: 1,
        }
        return expected_lens.get(cmd)

    def parse_frame(self, data, decrypt=False, report_crc=True):
        if len(data) < 8 or data[0] != 0xFA or data[1] != 0xFC:
            return None

        cmd = data[3]
        data_len = (data[4] << 8) + data[5]
        frame_len = 6 + data_len + 2
        if len(data) < frame_len:
            return None

        received_crc = (data[6 + data_len] << 8) + data[7 + data_len]
        calculated_crc = self.crc16_compute(data[3:6 + data_len])
        if received_crc != calculated_crc:
            if report_crc:
                self.update_callback(f"CRC校验失败: 收到 {received_crc:04X}, 计算 {calculated_crc:04X}")
            return None

        payload = bytes(data[6:6 + data_len])
        if decrypt and data_len > 0:
            if (data_len % 16) != 0:
                self.update_callback("解密数据长度不是16字节对齐")
                return None
            cipher = AES.new(self.aes_key, AES.MODE_ECB)
            payload = cipher.decrypt(payload)
            expected_len = self.expected_payload_len(cmd)
            if expected_len is not None and data_len != expected_len:
                payload = payload[:expected_len]

        return {
            "cmd": cmd,
            "payload": payload,
            "encoded_len": data_len,
            "frame_len": frame_len,
        }
    
    def parse_received_data(self, data, decrypt=True):
        """解析接收到的数据包
        decrypt: 是否进行AES解密
        """
        try:
            frame = self.parse_frame(data, decrypt=decrypt)
            if frame is None:
                return None

            return self.format_payload_for_display(frame["cmd"], frame["payload"], prefix="上传")
        except Exception as e:
            self.update_callback(f"解析数据包错误: {e}")
            return None

    def get_frame_cmd(self, data):
        try:
            frame = self.parse_frame(data, decrypt=False, report_crc=False)
            if frame is None:
                return None
            return frame["cmd"]
        except Exception:
            return None

    def is_valid_handshake_reply(self, data):
        frame = self.parse_frame(data, decrypt=True, report_crc=False)
        if frame is None or frame["cmd"] != CMD_HANDSHAKE:
            return False

        expected_mac = self.get_handshake_seed()
        if expected_mac is None:
            self.update_callback("无法校验握手回复MAC")
            return False

        reply_mac = frame["payload"][:6]
        if reply_mac != expected_mac:
            self.update_callback(
                f"握手回复MAC不匹配: 收到 {reply_mac.hex(':').upper()}, 期望 {expected_mac.hex(':').upper()}"
            )
            return False

        return True

    def send_time_sync_after_handshake(self, data):
        if self.handshake_completed:
            return

        if not self.is_valid_handshake_reply(data):
            self.update_callback("收到握手命令帧，但不是有效的固件MAC回复")
            return

        self.handshake_completed = True
        packet = self.create_time_sync_packet()
        if not packet:
            return

        asyncio.run_coroutine_threadsafe(
            self.send_data(packet, DEFAULT_SERVICE_UUID, self.write_characteristic_uuid),
            self.loop
        )
        self.update_callback("握手成功，已发送0x33时间同步")
    
    # 通知处理函数
    def notification_handler(self, characteristic: BleakGATTCharacteristic, data: bytearray):
        try:
            should_decrypt = self.get_frame_cmd(data) == CMD_HANDSHAKE
            parsed_data = self.parse_received_data(data, decrypt=should_decrypt)
            if parsed_data:
                self.update_callback(parsed_data)
            else:
                hex_data = data.hex()
                self.update_callback(f"收到原始数据: {hex_data}")

            if self.get_frame_cmd(data) == CMD_HANDSHAKE:
                self.send_time_sync_after_handshake(data)
        except Exception as e:
            self.update_callback(f"处理通知错误: {e}")
    
    def handle_tcp_data(self, data):
        """处理从TCP接收到的数据"""
        try:
            # TCP数据不需要解密
            parsed_data = self.parse_received_data(data, decrypt=False)
            if parsed_data:
                self.update_callback(parsed_data)
            else:
                # 如果无法解析，显示原始十六进制数据
                hex_data = data.hex()
                self.update_callback(f"收到TCP数据: {hex_data}")
        except Exception as e:
            self.update_callback(f"处理TCP数据错误: {e}")
    
    # 发送数据
    async def send_data(self, data, service_uuid, characteristic_uuid):
        if not self.client or not self.client.is_connected:
            self.update_callback("未连接设备")
            return False
        
        try:
            # 如果数据是字符串，转换为字节
            if isinstance(data, str):
                data = data.encode('utf-8')
            
            # 如果没有指定UUID，尝试使用找到的通知特性
            if not characteristic_uuid:
                characteristic_uuid = self.write_characteristic_uuid
            
            if not characteristic_uuid:
                self.update_callback("未指定特性UUID")
                return False
            
            await self.client.write_gatt_char(characteristic_uuid, data)
            return True
        except Exception as e:
            self.update_callback(f"发送数据失败: {e}")
            return False


def parse_intel_hex(file_path, return_start_address=False):
    """解析Intel HEX格式文件。

    对于OTA升级：
    - 返回的数据会从文件的最小地址开始连续提取（地址归零成 offset=0）。
    - 可选返回"起始地址"：第一条数据记录(type=0x00)对应的实际地址。
      这通常对应 HEX 的第二行（第一行多为扩展线性地址记录）。
    """
    memory = {}  # 使用字典存储地址和数据
    base_address = 0  # 扩展线性地址
    min_address = None  # 最小地址
    max_address = 0
    first_data_address = None
    
    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line or not line.startswith(':'):
                continue
            
            try:
                # 移除冒号
                hex_data = line[1:]
                
                # 检查数据长度（至少需要8个字符：2字节长度+2字节地址+2字节类型+2字节校验和）
                if len(hex_data) < 8:
                    continue
                
                # 解析记录长度、地址、类型
                data_len = int(hex_data[0:2], 16)
                
                # 检查数据长度是否合理（每行最多255字节数据）
                if data_len > 255:
                    continue
                
                # 检查是否有足够的数据
                expected_len = 8 + data_len * 2 + 2  # 长度+地址+类型+数据+校验和
                if len(hex_data) < expected_len:
                    continue
                
                address = int(hex_data[2:6], 16)
                record_type = int(hex_data[6:8], 16)
                
                # 解析数据
                data_hex = hex_data[8:8+data_len*2]
                data_bytes = bytes.fromhex(data_hex)
                
                # 解析校验和
                checksum_hex = hex_data[8+data_len*2:10+data_len*2]
                checksum = int(checksum_hex, 16)
                
                # 计算校验和（所有字节的和，取二进制补码的低8位）
                calc_sum = data_len + (address >> 8) + (address & 0xFF) + record_type
                for b in data_bytes:
                    calc_sum += b
                calc_sum = (256 - (calc_sum % 256)) % 256
                
                if calc_sum != checksum:
                    raise ValueError(f"行 {line_num} 校验和错误: 期望 {checksum:02X}, 计算 {calc_sum:02X}")
                
                if record_type == 0x00:  # 数据记录
                    # 计算实际地址
                    actual_address = base_address + address
                    if first_data_address is None:
                        first_data_address = actual_address
                    # 存储数据
                    for i, byte_val in enumerate(data_bytes):
                        memory[actual_address + i] = byte_val
                        if min_address is None or actual_address + i < min_address:
                            min_address = actual_address + i
                        max_address = max(max_address, actual_address + i + 1)
                
                elif record_type == 0x01:  # 文件结束记录
                    break
                
                elif record_type == 0x04:  # 扩展线性地址记录
                    if data_len == 2:
                        base_address = (data_bytes[0] << 24) | (data_bytes[1] << 16)
                    else:
                        raise ValueError(f"行 {line_num} 扩展线性地址记录长度错误")
                
                elif record_type == 0x02:  # 扩展段地址记录（16位段地址，较少使用）
                    if data_len == 2:
                        base_address = ((data_bytes[0] << 8) | data_bytes[1]) << 4
                    else:
                        raise ValueError(f"行 {line_num} 扩展段地址记录长度错误")
                
                # 其他记录类型忽略（0x03=起始段地址，0x05=起始线性地址等）
                
            except ValueError as e:
                # 校验和错误等严重错误，抛出异常
                raise
            except Exception as e:
                # 其他错误，记录但继续处理
                import sys
                print(f"警告: 行 {line_num} 解析错误: {e}", file=sys.stderr)
                continue
    
    # 将字典转换为连续的字节数组（从最小地址开始）
    if not memory or min_address is None:
        return (b'', None) if return_start_address else b''
    
    # 创建连续的字节数组（从最小地址开始，偏移到0）
    data_size = max_address - min_address
    binary_data = bytearray(data_size)
    for addr, byte_val in memory.items():
        offset = addr - min_address
        if 0 <= offset < data_size:
            binary_data[offset] = byte_val
    
    result_bytes = bytes(binary_data)
    if return_start_address:
        return result_bytes, first_data_address
    return result_bytes


def calculate_file_crc32(data):
    """计算数据的CRC32值（与设备端 Bootloader_CRC32Update 一致）。

    设备端使用方式：第一次传入 crc=0，随后按收到的数据分段（如 128B）累加更新。
    Python 侧可直接使用 zlib.crc32 做增量计算：crc = zlib.crc32(chunk, crc)。

    参数可以是文件路径（字符串）或二进制数据（bytes/bytearray）。
    """
    crc = 0
    
    if isinstance(data, str):
        # 如果是文件路径，读取文件
        with open(data, 'rb') as f:
            while True:
                chunk = f.read(4096)
                if not chunk:
                    break
                crc = zlib.crc32(chunk, crc)
    else:
        # 如果是二进制数据，直接计算
        crc = zlib.crc32(data, crc)
    
    return crc & 0xFFFFFFFF


class OTAWindow:
    def __init__(self, parent, manager):
        self.parent = parent
        self.manager = manager
        self.ota_file_path = None
        self.ota_file_data = None
        self.ota_file_size = 0
        self.ota_file_crc32 = 0
        self.ota_state = "idle"  # idle, requesting, file_info_sent, transferring, completed, error
        self.packet_max_len = 128
        self.device_version = [0, 0, 0]
        self.is_ota_allowed = False
        # requested_file_offset: 主机发送给设备的偏移（通常等于 APP_START_ADDR 的低 20bit）
        # file_offset: 设备响应回来的偏移（用于显示/后续流程）
        self.requested_file_offset = 0
        self.file_offset = 0
        self.packet_sequence = 0
        self.expected_response_cmd = None
        
        # 创建窗口
        self.window = tk.Toplevel(parent)
        self.window.title("OTA升级")
        self.window.geometry("700x600")
        self.window.protocol("WM_DELETE_WINDOW", self.on_close)
        
        # 创建UI
        self.create_widgets()
        
        # 设置数据接收回调
        self.original_callback = manager.update_callback
        manager.update_callback = self.ota_data_callback
    
    def create_widgets(self):
        main_frame = ttk.Frame(self.window, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # 文件选择区域
        file_frame = ttk.LabelFrame(main_frame, text="OTA文件选择", padding="5")
        file_frame.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        self.file_path_label = ttk.Label(file_frame, text="未选择文件")
        self.file_path_label.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), padx=5, pady=5)
        
        ttk.Button(file_frame, text="选择文件", command=self.select_file).grid(row=1, column=0, padx=5)
        
        file_info_frame = ttk.Frame(file_frame)
        file_info_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        self.file_info_label = ttk.Label(file_info_frame, text="")
        self.file_info_label.grid(row=0, column=0, sticky=tk.W)
        
        # 交互信息区域
        info_frame = ttk.LabelFrame(main_frame, text="设备交互信息", padding="5")
        info_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S), pady=5)
        
        self.info_text = scrolledtext.ScrolledText(info_frame, height=15, wrap=tk.WORD)
        self.info_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # 控制按钮区域
        control_frame = ttk.Frame(main_frame)
        control_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        self.start_button = ttk.Button(control_frame, text="开始升级", command=self.start_ota, state=tk.DISABLED)
        self.start_button.grid(row=0, column=0, padx=5)
        
        self.cancel_button = ttk.Button(control_frame, text="取消", command=self.on_close)
        self.cancel_button.grid(row=0, column=1, padx=5)
        
        # 进度条
        progress_frame = ttk.Frame(main_frame)
        progress_frame.grid(row=3, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        self.progress_label = ttk.Label(progress_frame, text="就绪")
        self.progress_label.grid(row=0, column=0, sticky=tk.W, padx=5)
        
        self.progress_bar = ttk.Progressbar(progress_frame, mode='determinate')
        self.progress_bar.grid(row=1, column=0, sticky=(tk.W, tk.E), padx=5, pady=5)
        
        # 配置权重
        self.window.columnconfigure(0, weight=1)
        self.window.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(1, weight=1)
        file_frame.columnconfigure(0, weight=1)
        info_frame.columnconfigure(0, weight=1)
        info_frame.rowconfigure(0, weight=1)
        progress_frame.columnconfigure(0, weight=1)
        
        self.log("OTA升级窗口已打开")
        self.log("请先选择OTA升级文件")
    
    def log(self, message):
        """在信息区域添加日志"""
        timestamp = datetime.datetime.now().strftime("%H:%M:%S")
        self.info_text.insert(tk.END, f"[{timestamp}] {message}\n")
        self.info_text.see(tk.END)
        self.window.update_idletasks()
    
    def select_file(self):
        """选择OTA文件"""
        file_path = filedialog.askopenfilename(
            title="选择OTA升级文件",
            filetypes=[
                ("HEX文件", "*.hex"),
                ("二进制文件", "*.bin"),
                ("所有文件", "*.*")
            ]
        )
        
        if file_path:
            try:
                file_name = os.path.basename(file_path)
                file_ext = os.path.splitext(file_path)[1].lower()
                
                # 根据文件扩展名选择解析方式
                if file_ext == '.hex':
                    self.log(f"检测到HEX文件，正在解析...")
                    self.ota_file_data, start_address = parse_intel_hex(file_path, return_start_address=True)
                    # 偏移值按设备端逻辑：APP_START_ADDR & 0x000FFFFF
                    # HEX 的"第二行"通常是第一条数据记录，对应的实际地址低 20bit 就是期望偏移。
                    if start_address is not None:
                        self.requested_file_offset = start_address & 0x000FFFFF
                    else:
                        self.requested_file_offset = 0
                    self.log(f"HEX文件解析完成")
                else:
                    # 二进制文件，直接读取
                    with open(file_path, 'rb') as f:
                        self.ota_file_data = f.read()
                    self.requested_file_offset = 0
                
                self.ota_file_path = file_path
                self.ota_file_size = len(self.ota_file_data)
                
                if self.ota_file_size == 0:
                    messagebox.showerror("错误", "文件为空或解析失败")
                    self.log("错误: 文件为空或解析失败")
                    return
                
                # 计算CRC32（使用二进制数据）
                self.ota_file_crc32 = calculate_file_crc32(self.ota_file_data)
                
                self.file_path_label.config(text=f"文件: {file_name}")
                
                file_size_kb = self.ota_file_size / 1024
                file_type_text = "HEX" if file_ext == '.hex' else "BIN"
                self.file_info_label.config(
                    text=f"类型: {file_type_text} | 大小: {self.ota_file_size} 字节 ({file_size_kb:.2f} KB) | CRC32: 0x{self.ota_file_crc32:08X} | 偏移: 0x{self.requested_file_offset:08X}"
                )
                
                self.start_button.config(state=tk.NORMAL)
                self.log(f"已选择文件: {file_name} ({file_type_text}格式)")
                self.log(f"文件大小: {self.ota_file_size} 字节")
                self.log(f"CRC32: 0x{self.ota_file_crc32:08X}")
                self.log(f"将发送的偏移(取HEX第二行/首条数据记录地址低20bit): 0x{self.requested_file_offset:08X}")
            except Exception as e:
                messagebox.showerror("错误", f"读取文件失败: {e}")
                self.log(f"读取文件失败: {e}")
                import traceback
                self.log(f"错误详情: {traceback.format_exc()}")
    
    def ota_data_callback(self, text):
        """OTA数据回调函数，用于接收设备响应"""
        # 调用原始回调
        if self.original_callback:
            self.original_callback(text)
        
        # 解析OTA响应
        if self.ota_state != "idle":
            self.parse_ota_response(text)
    
    def parse_ota_response(self, text):
        """解析OTA响应"""
        try:
            # 这里需要解析接收到的数据包
            # 由于数据包是加密的，需要在BLEDeviceManager中解析后传递命令信息
            # 为了简化，我们通过解析文本来判断
            if "命令:" in text:
                # 尝试提取命令
                parts = text.split(",")
                for part in parts:
                    if "命令:" in part:
                        cmd_hex = part.split(":")[1].strip().split()[0]
                        cmd = int(cmd_hex, 16)
                        self.handle_ota_response(cmd, text)
                        break
        except Exception as e:
            self.log(f"解析响应错误: {e}")
    
    def handle_ota_response(self, cmd, full_text):
        """处理OTA响应命令"""
        if cmd == 0xEA:  # OTA_REQUEST响应
            if self.ota_state == "requesting":
                # 解析版本和最大长度信息
                # 格式: 命令: EA, 数据(十六进制): 0101...
                try:
                    data_part = full_text.split("数据(十六进制):")[1].strip()
                    data_bytes = bytes.fromhex(data_part)
                    if len(data_bytes) >= 9:
                        self.device_version = [data_bytes[2], data_bytes[3], data_bytes[4]]
                        self.packet_max_len = (data_bytes[7] << 8) | data_bytes[8]
                        self.log(f"设备版本: v{self.device_version[0]}.{self.device_version[1]}.{self.device_version[2]}")
                        self.log(f"最大包长度: {self.packet_max_len} 字节")
                        self.ota_state = "request_received"
                        self.send_file_info()
                except Exception as e:
                    self.log(f"解析OTA请求响应失败: {e}")
        
        elif cmd == 0xEB:  # OTA_FILE_INFO响应
            if self.ota_state == "file_info_sent":
                try:
                    data_part = full_text.split("数据(十六进制):")[1].strip()
                    data_bytes = bytes.fromhex(data_part)
                    if len(data_bytes) >= 1:
                        self.is_ota_allowed = (data_bytes[0] == 0x01)
                        if self.is_ota_allowed:
                            self.log("设备允许OTA升级")
                            self.ota_state = "file_info_ack"
                            self.send_offset_info()
                        else:
                            self.log("设备拒绝OTA升级")
                            self.ota_state = "error"
                            self.progress_label.config(text="升级失败: 设备拒绝")
                except Exception as e:
                    self.log(f"解析文件信息响应失败: {e}")
        
        elif cmd == 0xEC:  # OTA_OFFSET响应
            if self.ota_state == "offset_sent":
                try:
                    data_part = full_text.split("数据(十六进制):")[1].strip()
                    data_bytes = bytes.fromhex(data_part)
                    if len(data_bytes) >= 4:
                        self.file_offset = (data_bytes[0] | (data_bytes[1] << 8) | 
                                           (data_bytes[2] << 16) | (data_bytes[3] << 24))
                        self.log(f"设备文件偏移: {self.file_offset}")
                        self.ota_state = "offset_ack"
                        self.start_data_transfer()
                except Exception as e:
                    self.log(f"解析偏移信息响应失败: {e}")
        
        elif cmd == 0xED:  # OTA_DATA响应
            if self.ota_state == "transferring":
                try:
                    data_part = full_text.split("数据(十六进制):")[1].strip()
                    data_bytes = bytes.fromhex(data_part)
                    if len(data_bytes) >= 1:
                        pack_status = data_bytes[0]
                        # 取消超时检查
                        if hasattr(self, 'packet_timeout_id'):
                            self.window.after_cancel(self.packet_timeout_id)
                        
                        if pack_status == 0:  # BL_PACK_S_SUCCESS
                            # 使用记录的上次数据包大小
                            self.transferred_size += self.last_packet_size
                            
                            # 更新进度
                            progress = (self.transferred_size / self.ota_file_size) * 100
                            self.progress_bar['value'] = progress
                            self.progress_label.config(text=f"发送进度: {progress:.1f}% ({self.transferred_size}/{self.ota_file_size})")
                            
                            self.log(f"数据包 {self.packet_sequence - 1} 发送成功，已传输: {self.transferred_size} 字节")
                            # 等待一小段时间后发送下一个包，避免发送太快
                            self.window.after(50, self.send_next_packet)
                        else:
                            error_msgs = {0: "成功", 1: "包错误", 2: "长度错误", 3: "CRC错误", 4: "错误"}
                            error_msg = error_msgs.get(pack_status, f"未知错误({pack_status})")
                            self.log(f"数据包发送失败，状态: {error_msg}")
                            self.ota_state = "error"
                            self.progress_label.config(text=f"升级失败: {error_msg}")
                            self.start_button.config(state=tk.NORMAL)
                except Exception as e:
                    self.log(f"解析数据包响应失败: {e}")
        
        elif cmd == 0xEE:  # OTA_RESULT响应
            try:
                data_part = full_text.split("数据(十六进制):")[1].strip()
                data_bytes = bytes.fromhex(data_part)
                if len(data_bytes) >= 1:
                    result = data_bytes[0]
                    if result == 0:  # BL_UPDATE_S_SUCCESS
                        self.log("OTA升级成功！")
                        self.ota_state = "completed"
                        self.progress_label.config(text="升级成功")
                        self.progress_bar['value'] = 100
                    else:
                        error_msg = ["成功", "数据长度错误", "CRC错误", "Flash错误"][result] if result < 4 else "未知错误"
                        self.log(f"OTA升级失败: {error_msg}")
                        self.ota_state = "error"
                        self.progress_label.config(text=f"升级失败: {error_msg}")
            except Exception as e:
                self.log(f"解析升级结果失败: {e}")
    
    def start_ota(self):
        """开始OTA升级流程"""
        if not self.ota_file_data:
            messagebox.showerror("错误", "请先选择OTA文件")
            return
        
        if not self.manager.client or not self.manager.client.is_connected:
            messagebox.showerror("错误", "设备未连接，请先连接设备")
            return
        
        self.ota_state = "requesting"
        self.packet_sequence = 0
        self.file_offset = 0
        self.progress_bar['value'] = 0
        self.start_button.config(state=tk.DISABLED)
        
        self.log("开始OTA升级流程...")
        self.send_ota_request()
    
    def send_ota_request(self):
        """发送OTA请求（请求最大包长度）"""
        try:
            # 发送命令 0xEA，数据为 0x00C8
            request_data = bytes([0x00, 0xC8])
            packet = self.manager.create_data_packet(request_data, 0xEA, encrypt=True)
            if packet:
                service_uuid = '0000fe60-0000-1000-8000-00805f9b34fb'
                char_uuid = '0000fe61-0000-1000-8000-00805f9b34fb'
                future = asyncio.run_coroutine_threadsafe(
                    self.manager.send_data(packet, service_uuid, char_uuid),
                    self.manager.loop
                )
                self.log("已发送OTA请求")
                # 等待响应，设置超时
                self.window.after(10000, self.check_request_timeout)
        except Exception as e:
            self.log(f"发送OTA请求失败: {e}")
            self.ota_state = "error"
    
    def check_request_timeout(self):
        """检查请求超时"""
        if self.ota_state == "requesting":
            self.log("OTA请求超时")
            self.ota_state = "error"
            self.start_button.config(state=tk.NORMAL)
    
    def send_file_info(self):
        """发送文件信息"""
        try:
            self.ota_state = "file_info_sent"
            # 文件信息：版本(4字节) + 文件大小(4字节) + CRC32(4字节)
            # 版本使用文件名的版本号，或者使用默认值
            version = [1, 0, 0, 0]  # 默认版本
            
            file_info = bytes(version[:4])  # 版本 4字节
            file_info += struct.pack('<I', self.ota_file_size)  # 文件大小 4字节，小端
            file_info += struct.pack('<I', self.ota_file_crc32)  # CRC32 4字节，小端
            
            packet = self.manager.create_data_packet(file_info, 0xEB, encrypt=True)
            if packet:
                service_uuid = '0000fe60-0000-1000-8000-00805f9b34fb'
                char_uuid = '0000fe61-0000-1000-8000-00805f9b34fb'
                future = asyncio.run_coroutine_threadsafe(
                    self.manager.send_data(packet, service_uuid, char_uuid),
                    self.manager.loop
                )
                self.log("已发送文件信息")
                self.window.after(3000, self.check_file_info_timeout)
        except Exception as e:
            self.log(f"发送文件信息失败: {e}")
            self.ota_state = "error"
    
    def check_file_info_timeout(self):
        """检查文件信息超时"""
        if self.ota_state == "file_info_sent":
            self.log("文件信息响应超时")
            self.ota_state = "error"
            self.start_button.config(state=tk.NORMAL)
    
    def send_offset_info(self):
        """发送偏移信息"""
        try:
            self.ota_state = "offset_sent"
            # 偏移信息：4字节偏移量（小端）
            offset_data = struct.pack('<I', self.requested_file_offset)
            
            packet = self.manager.create_data_packet(offset_data, 0xEC, encrypt=True)
            if packet:
                service_uuid = '0000fe60-0000-1000-8000-00805f9b34fb'
                char_uuid = '0000fe61-0000-1000-8000-00805f9b34fb'
                future = asyncio.run_coroutine_threadsafe(
                    self.manager.send_data(packet, service_uuid, char_uuid),
                    self.manager.loop
                )
                self.log(f"已发送偏移信息: {self.requested_file_offset} (0x{self.requested_file_offset:08X})")
                self.window.after(3000, self.check_offset_timeout)
        except Exception as e:
            self.log(f"发送偏移信息失败: {e}")
            self.ota_state = "error"
    
    def check_offset_timeout(self):
        """检查偏移信息超时"""
        if self.ota_state == "offset_sent":
            self.log("偏移信息响应超时")
            self.ota_state = "error"
            self.start_button.config(state=tk.NORMAL)
    
    def start_data_transfer(self):
        """开始数据传输"""
        self.ota_state = "transferring"
        self.packet_sequence = 0
        self.transferred_size = 0  # 已传输的数据大小（从文件开头开始）
        self.last_packet_size = 0  # 上次发送的数据包大小
        self.send_next_packet()
    
    def send_next_packet(self):
        """发送下一个数据包"""
        if self.ota_state != "transferring":
            return
        
        try:
            # 从文件开头开始传输，current_offset就是已传输的大小
            current_offset = self.transferred_size
            if current_offset >= self.ota_file_size:
                # 所有数据已发送，请求升级结果
                self.request_update_result()
                return
            
            # 计算本次发送的数据长度（packet_max_len是数据部分的最大长度，通常是128字节）
            max_data_len = min(128, self.ota_file_size - current_offset)
            packet_data = self.ota_file_data[current_offset:current_offset + max_data_len]
            
            # 记录本次发送的数据包大小
            self.last_packet_size = len(packet_data)
            
            # 构建数据包：序列号(2字节) + 数据长度(2字节) + CRC16(2字节) + 数据
            packet_sn_bytes = struct.pack('<H', self.packet_sequence)
            packet_len_bytes = struct.pack('<H', len(packet_data))
            
            # 计算数据部分的CRC16
            packet_crc16 = self.manager.crc16_compute(packet_data)
            packet_crc16_bytes = struct.pack('<H', packet_crc16)
            
            # 组合数据包内容
            ota_packet_content = packet_sn_bytes + packet_len_bytes + packet_crc16_bytes + packet_data
            
            # 使用OTA_DATA命令发送
            packet = self.manager.create_data_packet(ota_packet_content, 0xED, encrypt=True)
            if packet:
                service_uuid = '0000fe60-0000-1000-8000-00805f9b34fb'
                char_uuid = '0000fe61-0000-1000-8000-00805f9b34fb'
                future = asyncio.run_coroutine_threadsafe(
                    self.manager.send_data(packet, service_uuid, char_uuid),
                    self.manager.loop
                )
                
                self.log(f"发送数据包 {self.packet_sequence}, 偏移: {current_offset}, 长度: {len(packet_data)} 字节")
                
                self.packet_sequence += 1
                
                # 设置超时检查（在收到响应后会被取消）
                self.packet_timeout_id = self.window.after(5000, self.check_packet_timeout)
        except Exception as e:
            self.log(f"发送数据包失败: {e}")
            self.ota_state = "error"
            self.start_button.config(state=tk.NORMAL)
    
    def check_packet_timeout(self):
        """检查数据包超时"""
        if self.ota_state == "transferring":
            self.log("数据包响应超时")
            self.ota_state = "error"
            self.progress_label.config(text="升级失败: 数据包响应超时")
            self.start_button.config(state=tk.NORMAL)
    
    def request_update_result(self):
        """请求升级结果"""
        try:
            self.ota_state = "waiting_result"
            # 发送空数据包请求结果
            packet = self.manager.create_data_packet(b'', 0xEE, encrypt=True)
            if packet:
                service_uuid = '0000fe60-0000-1000-8000-00805f9b34fb'
                char_uuid = '0000fe61-0000-1000-8000-00805f9b34fb'
                future = asyncio.run_coroutine_threadsafe(
                    self.manager.send_data(packet, service_uuid, char_uuid),
                    self.manager.loop
                )
                self.log("已请求升级结果")
                self.window.after(10000, self.check_result_timeout)
        except Exception as e:
            self.log(f"请求升级结果失败: {e}")
            self.ota_state = "error"
            self.start_button.config(state=tk.NORMAL)
    
    def check_result_timeout(self):
        """检查结果超时"""
        if self.ota_state == "waiting_result":
            self.log("升级结果响应超时")
            self.ota_state = "error"
            self.start_button.config(state=tk.NORMAL)
    
    def on_close(self):
        """关闭窗口"""
        # 恢复原始回调
        if hasattr(self, 'original_callback'):
            self.manager.update_callback = self.original_callback
        self.window.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = BLEApp(root)
    root.mainloop()
