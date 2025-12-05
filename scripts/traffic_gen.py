#!/usr/bin/env python3
"""
Grid-Watcher Traffic Generator - Real TCP/Modbus Traffic Simulator
Generates various attack patterns for testing GridWatcher
"""

import socket
import time
import random
import struct
import sys
import threading
from typing import List, Tuple

# ANSI Colors
RED = '\033[91m'
GREEN = '\033[92m'
YELLOW = '\033[93m'
BLUE = '\033[94m'
RESET = '\033[0m'

def get_local_ip():
    """Get local IP address"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        return "127.0.0.1"

def create_modbus_request(transaction_id: int, unit_id: int = 1, 
                         function_code: int = 3, address: int = 100, 
                         count: int = 10) -> bytes:
    """Create a valid Modbus TCP request"""
    # MBAP Header (7 bytes)
    mbap = struct.pack(
        '>HHHB',
        transaction_id,  # Transaction ID
        0,               # Protocol ID (0 for Modbus)
        6,               # Length (6 bytes following)
        unit_id          # Unit ID
    )
    
    # PDU (5 bytes for Read Holding Registers)
    pdu = struct.pack(
        '>BHH',
        function_code,   # Function code
        address,         # Starting address
        count            # Quantity
    )
    
    return mbap + pdu

class TrafficGenerator:
    def __init__(self, target_ip: str, target_port: int = 502):
        self.target_ip = target_ip
        self.target_port = target_port
        self.packets_sent = 0
        self.running = True
        
    def send_packet(self, data: bytes, port: int = None) -> bool:
        """Send a single packet"""
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(0.1)
            
            actual_port = port if port else self.target_port
            s.connect((self.target_ip, actual_port))
            s.send(data)
            s.close()
            
            self.packets_sent += 1
            return True
        except:
            return False
    
    def normal_traffic(self, duration: int = 10, rate: int = 10):
        """Generate normal SCADA traffic"""
        print(f"\n{GREEN}[NORMAL]{RESET} Generating normal traffic...")
        print(f"  Duration: {duration}s | Rate: {rate} pps")
        
        start = time.time()
        tid = 1
        
        while time.time() - start < duration and self.running:
            packet = create_modbus_request(
                tid, 
                unit_id=1,
                function_code=3,  # Read Holding Registers
                address=random.randint(100, 200),
                count=random.randint(1, 10)
            )
            
            self.send_packet(packet)
            tid += 1
            time.sleep(1.0 / rate)
        
        print(f"  ✓ Sent {self.packets_sent} normal packets")
    
    def dos_attack(self, duration: int = 10, rate: int = 1000):
        """Simulate DoS flood attack"""
        print(f"\n{RED}[DOS FLOOD]{RESET} Simulating DoS attack...")
        print(f"  Duration: {duration}s | Rate: {rate} pps")
        
        start = time.time()
        tid = 10000
        
        while time.time() - start < duration and self.running:
            packet = create_modbus_request(tid)
            self.send_packet(packet)
            tid += 1
            
            # Brief sleep to achieve target rate
            if rate < 10000:
                time.sleep(1.0 / rate)
        
        print(f"  ✓ Sent {self.packets_sent} attack packets")
    
    def port_scan(self, start_port: int = 500, end_port: int = 520):
        """Simulate port scan attack"""
        print(f"\n{YELLOW}[PORT SCAN]{RESET} Scanning ports {start_port}-{end_port}...")
        
        for port in range(start_port, end_port):
            packet = create_modbus_request(port)
            self.send_packet(packet, port=port)
            time.sleep(0.1)
        
        print(f"  ✓ Scanned {end_port - start_port} ports")
    
    def unauthorized_write(self, count: int = 20):
        """Simulate unauthorized write attempts"""
        print(f"\n{RED}[UNAUTHORIZED WRITE]{RESET} Attempting {count} writes...")
        
        for i in range(count):
            # Function code 0x10 = Write Multiple Registers
            packet = create_modbus_request(
                100 + i,
                function_code=0x10,  # Write Multiple Registers
                address=random.randint(0, 99),  # Critical registers
                count=1
            )
            self.send_packet(packet)
            time.sleep(0.2)
        
        print(f"  ✓ Sent {count} write attempts")
    
    def malformed_packets(self, count: int = 10):
        """Send malformed packets"""
        print(f"\n{RED}[MALFORMED]{RESET} Sending {count} malformed packets...")
        
        for i in range(count):
            # Create invalid Modbus packets
            malformed = bytes([random.randint(0, 255) for _ in range(random.randint(5, 50))])
            self.send_packet(malformed)
            time.sleep(0.1)
        
        print(f"  ✓ Sent {count} malformed packets")
    
    def mixed_attack(self, duration: int = 30):
        """Simulate mixed attack scenario"""
        print(f"\n{RED}[MIXED ATTACK]{RESET} Running mixed attack for {duration}s...")
        
        threads = []
        
        # Spawn multiple attack threads
        def dos_thread():
            for _ in range(5):
                self.dos_attack(duration=2, rate=500)
                time.sleep(1)
        
        def scan_thread():
            for _ in range(3):
                self.port_scan(500, 510)
                time.sleep(3)
        
        def write_thread():
            for _ in range(10):
                self.unauthorized_write(5)
                time.sleep(2)
        
        threads.append(threading.Thread(target=dos_thread))
        threads.append(threading.Thread(target=scan_thread))
        threads.append(threading.Thread(target=write_thread))
        
        for t in threads:
            t.start()
        
        for t in threads:
            t.join()
        
        print(f"  ✓ Mixed attack completed")

def print_banner():
    print(f"""
{BLUE}╔═══════════════════════════════════════════════════════════════════╗
║                                                                   ║
║           GRID-WATCHER TRAFFIC GENERATOR v3.0                     ║
║                                                                   ║
║  Generates real TCP/Modbus traffic for testing                    ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝{RESET}
    """)

def main():
    print_banner()
    
    # Configuration
    TARGET_IP = get_local_ip()
    TARGET_PORT = 502
    
    print(f"Target: {TARGET_IP}:{TARGET_PORT}")
    print(f"\n{YELLOW}⚠️  Make sure Grid-Watcher is running!{RESET}\n")
    time.sleep(2)
    
    gen = TrafficGenerator(TARGET_IP, TARGET_PORT)
    
    print("═" * 70)
    print("SCENARIO MENU")
    print("═" * 70)
    print("1. Normal Traffic (baseline)")
    print("2. DoS Flood Attack")
    print("3. Port Scan Attack")
    print("4. Unauthorized Write Attack")
    print("5. Malformed Packets")
    print("6. Mixed Attack (realistic scenario)")
    print("7. Full Test Suite (all scenarios)")
    print("═" * 70)
    
    try:
        choice = input("\nSelect scenario (1-7): ").strip()
        
        if choice == '1':
            gen.normal_traffic(duration=30, rate=10)
        
        elif choice == '2':
            rate = int(input("Packets per second (100-10000): ") or "1000")
            duration = int(input("Duration in seconds (5-60): ") or "10")
            gen.dos_attack(duration=duration, rate=rate)
        
        elif choice == '3':
            gen.port_scan(500, 520)
        
        elif choice == '4':
            gen.unauthorized_write(count=50)
        
        elif choice == '5':
            gen.malformed_packets(count=20)
        
        elif choice == '6':
            gen.mixed_attack(duration=30)
        
        elif choice == '7':
            print(f"\n{BLUE}[FULL TEST SUITE]{RESET} Running all scenarios...\n")
            
            print("\n" + "="*70)
            print("PHASE 1: Baseline (Normal Traffic)")
            print("="*70)
            gen.normal_traffic(duration=20, rate=5)
            time.sleep(3)
            
            print("\n" + "="*70)
            print("PHASE 2: Port Scan Attack")
            print("="*70)
            gen.port_scan(500, 520)
            time.sleep(3)
            
            print("\n" + "="*70)
            print("PHASE 3: DoS Flood Attack")
            print("="*70)
            gen.dos_attack(duration=15, rate=800)
            time.sleep(3)
            
            print("\n" + "="*70)
            print("PHASE 4: Unauthorized Write Attempts")
            print("="*70)
            gen.unauthorized_write(count=30)
            time.sleep(3)
            
            print("\n" + "="*70)
            print("PHASE 5: Malformed Packets")
            print("="*70)
            gen.malformed_packets(count=15)
            time.sleep(3)
            
            print("\n" + "="*70)
            print("PHASE 6: Recovery (Normal Traffic)")
            print("="*70)
            gen.normal_traffic(duration=20, rate=5)
            
            print(f"\n{GREEN}✓ Full test suite completed!{RESET}")
            print(f"  Total packets sent: {gen.packets_sent}")
        
        else:
            print(f"{RED}Invalid choice{RESET}")
            return
        
        print(f"\n{GREEN}╔═══════════════════════════════════════════╗")
        print(f"║  TRAFFIC GENERATION COMPLETE              ║")
        print(f"║  Total Packets Sent: {gen.packets_sent:6d}              ║")
        print(f"╚═══════════════════════════════════════════╝{RESET}\n")
        
        print("Check the Dashboard: http://localhost:8080/dashboard.html")
    
    except KeyboardInterrupt:
        print(f"\n\n{YELLOW}[STOPPED]{RESET} Traffic generation interrupted")
        gen.running = False
    except Exception as e:
        print(f"\n{RED}[ERROR]{RESET} {e}")

if __name__ == "__main__":
    main()