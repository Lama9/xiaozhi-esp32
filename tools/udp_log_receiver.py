#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
UDP Log Receiver for XiaoZhi ESP32
功能：接收并显示来自ESP32的UDP广播日志
监听端口：12345
"""

import socket
import sys
import time

def main():
    # 配置
    HOST = '0.0.0.0'  # 监听所有网卡
    PORT = 12345      # 监听端口
    
    # 创建UDP Socket
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # 允许端口复用
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # 绑定端口
        sock.bind((HOST, PORT))
        print(f"[*] Started UDP Log Receiver on port {PORT}")
        print("[*] Waiting for logs... (Press Ctrl+C to stop)")
        print("-" * 60)
        
    except Exception as e:
        print(f"[!] Error starting server: {e}")
        sys.exit(1)

    try:
        while True:
            # 接收数据
            data, addr = sock.recvfrom(1024)
            
            # 获取当前时间
            timestamp = time.strftime("%H:%M:%S", time.localtime())
            
            # 解码数据
            try:
                message = data.decode('utf-8', errors='replace').strip()
            except:
                message = str(data)
                
            # 打印日志
            print(f"[{timestamp}] From {addr[0]}: {message}")
            if message.startswith("==="):
                print("-" * 40)
                
    except KeyboardInterrupt:
        print("\n[*] Stopping server...")
    except Exception as e:
        print(f"\n[!] Error receiving data: {e}")
    finally:
        sock.close()

if __name__ == '__main__':
    main()
