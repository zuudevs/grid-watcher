import socket
import time
import sys
import random

def get_lan_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        return "127.0.0.1"

TARGET_IP = get_lan_ip()
TARGET_PORT = 502 

print(f"\n{'='*60}")
print(f"🚀 TRAFFIC GENERATOR: TCP SYN FLOOD SIMULATION")
print(f"{'='*60}")
print(f"Target: {TARGET_IP}:{TARGET_PORT}")
print(f"Mode  : TCP Connection Spam (Generates SYN Packets)")
print(f"{'-'*60}")
print(f"⚠️  GridWatcher seharusnya mendeteksi ini sebagai trafik TCP.")
print(f"   Pastikan GridWatcher jalan SEBELUM script ini.")
print(f"{'-'*60}")
time.sleep(2)

packet_count = 0
start_time = time.time()

try:
    while True:
        try:
            # 1. Buka Socket TCP
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(0.01) # Timeout super cepat biar ga nunggu
            
            # 2. Coba Connect (Ini ngirim paket TCP SYN)
            # Kita gak peduli connect berhasil atau gagal, yang penting paket SYN terkirim
            s.connect_ex((TARGET_IP, TARGET_PORT))
            
            # 3. Langsung tutup (Ini ngirim paket TCP RST/FIN)
            s.close()
            
            packet_count += 1 # Hitung 1 cycle (bisa 2-3 paket di wireshark: SYN, RST)
            
            if packet_count % 50 == 0:
                elapsed = time.time() - start_time
                rate = packet_count / elapsed if elapsed > 0 else 0
                print(f" >> Attacks sent: {packet_count} | Rate: {rate:.2f} conn/s", end='\r')
                
            # Ga usah sleep lama-lama, hajar terus
            # time.sleep(0.001) 
            
        except Exception as e:
            pass # Ignore errors, just spam

except KeyboardInterrupt:
    print(f"\n\n[ATTACKER] 🛑 Berhenti.")
    print(f"Total Connection Attempts: {packet_count}")