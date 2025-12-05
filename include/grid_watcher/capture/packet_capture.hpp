#pragma once

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <winsock2.h>
    #include "pcap.h"
    #pragma comment(lib, "wpcap.lib")
    #pragma comment(lib, "Packet.lib")
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <pcap.h>
#endif

#include "../grid_watcher.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <iostream>
#include <cstring>

namespace gw::capture {

// ============================================================================
// Packet Capture Engine (Windows/Linux compatible)
// ============================================================================
class PacketCapture {
private:
    pcap_t* handle_{nullptr};
    std::atomic<bool> running_{false};
    std::thread capture_thread_;
    scada::GridWatcher& watcher_;
    std::string interface_name_;
    
    // Statistics
    alignas(64) std::atomic<uint64_t> packets_captured_{0};
    alignas(64) std::atomic<uint64_t> packets_processed_{0};
    alignas(64) std::atomic<uint64_t> packets_dropped_{0};
    
public:
    explicit PacketCapture(scada::GridWatcher& watcher) 
        : watcher_(watcher) {}
    
    ~PacketCapture() {
        stop();
    }
    
    // ========================================================================
    // List Available Network Interfaces
    // ========================================================================
    static std::vector<std::string> listInterfaces() {
        std::vector<std::string> interfaces;
        char errbuf[PCAP_ERRBUF_SIZE];
        pcap_if_t* alldevs;
        
        if (pcap_findalldevs(&alldevs, errbuf) == -1) {
            std::cerr << "[ERROR] " << errbuf << "\n";
            return interfaces;
        }
        
        std::cout << "\n╔═══════════════════════════════════════════════════════╗\n";
        std::cout << "║         AVAILABLE NETWORK INTERFACES                  ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";
        
        int i = 0;
        for (pcap_if_t* dev = alldevs; dev != nullptr; dev = dev->next) {
            std::cout << "[" << i++ << "] " << dev->name << "\n";
            if (dev->description) {
                std::cout << "    Description: " << dev->description << "\n";
            }
            
            // Show addresses
            for (pcap_addr_t* addr = dev->addresses; addr != nullptr; addr = addr->next) {
                if (addr->addr && addr->addr->sa_family == AF_INET) {
                    sockaddr_in* sa = (sockaddr_in*)addr->addr;
                    char ip_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sa->sin_addr, ip_str, sizeof(ip_str));
                    std::cout << "    IP: " << ip_str << "\n";
                }
            }
            std::cout << "\n";
            
            interfaces.push_back(dev->name);
        }
        
        pcap_freealldevs(alldevs);
        return interfaces;
    }
    
    // ========================================================================
    // Start Capturing (with BPF filter)
    // ========================================================================
    bool start(const std::string& interface = "any", 
               const std::string& filter = "tcp port 502") {
        
        interface_name_ = interface;
        char errbuf[PCAP_ERRBUF_SIZE];
        
        std::cout << "\n[CAPTURE] Opening interface: " << interface << "\n";
        std::cout << "[CAPTURE] Filter: " << filter << "\n";
        
        // Open live capture
        handle_ = pcap_open_live(
            interface.c_str(),
            65536,  // snaplen (max packet size)
            1,      // promiscuous mode
            1000,   // read timeout (ms)
            errbuf
        );
        
        if (!handle_) {
            std::cerr << "[ERROR] Failed to open device: " << errbuf << "\n";
            std::cerr << "\n💡 Tip: Run as Administrator/root\n";
            std::cerr << "💡 Tip: Use --list-interfaces to see available devices\n";
            return false;
        }
        
        // Compile and set filter
        struct bpf_program fp;
        if (pcap_compile(handle_, &fp, filter.c_str(), 0, PCAP_NETMASK_UNKNOWN) == -1) {
            std::cerr << "[ERROR] Failed to compile filter: " << pcap_geterr(handle_) << "\n";
            pcap_close(handle_);
            handle_ = nullptr;
            return false;
        }
        
        if (pcap_setfilter(handle_, &fp) == -1) {
            std::cerr << "[ERROR] Failed to set filter: " << pcap_geterr(handle_) << "\n";
            pcap_freecode(&fp);
            pcap_close(handle_);
            handle_ = nullptr;
            return false;
        }
        
        pcap_freecode(&fp);
        
        // Start capture thread
        running_ = true;
        capture_thread_ = std::thread([this]() { captureLoop(); });
        
        std::cout << "[CAPTURE] ✓ Successfully started\n";
        std::cout << "[CAPTURE] Listening for packets...\n\n";
        
        return true;
    }
    
    // ========================================================================
    // Stop Capturing
    // ========================================================================
    void stop() {
        if (running_.exchange(false)) {
            std::cout << "\n[CAPTURE] Stopping...\n";
            
            if (handle_) {
                pcap_breakloop(handle_);
            }
            
            if (capture_thread_.joinable()) {
                capture_thread_.join();
            }
            
            if (handle_) {
                // Get pcap stats
                struct pcap_stat stats;
                if (pcap_stats(handle_, &stats) == 0) {
                    std::cout << "[CAPTURE] Pcap Stats:\n";
                    std::cout << "  Received:  " << stats.ps_recv << " packets\n";
                    std::cout << "  Dropped:   " << stats.ps_drop << " packets\n";
                }
                
                pcap_close(handle_);
                handle_ = nullptr;
            }
            
            std::cout << "[CAPTURE] ✓ Stopped\n";
            std::cout << "\nCapture Statistics:\n";
            std::cout << "  Captured:  " << packets_captured_.load() << " packets\n";
            std::cout << "  Processed: " << packets_processed_.load() << " packets\n";
            std::cout << "  Dropped:   " << packets_dropped_.load() << " packets\n";
        }
    }
    
    // ========================================================================
    // Get Statistics
    // ========================================================================
    struct Stats {
        uint64_t packets_captured;
        uint64_t packets_processed;
        uint64_t packets_dropped;
    };
    
    Stats getStats() const noexcept {
        return Stats{
            packets_captured_.load(std::memory_order_relaxed),
            packets_processed_.load(std::memory_order_relaxed),
            packets_dropped_.load(std::memory_order_relaxed)
        };
    }
    
private:
    // ========================================================================
    // Main Capture Loop (runs in background thread)
    // ========================================================================
    void captureLoop() {
        struct pcap_pkthdr* header;
        const u_char* packet;
        
        while (running_.load(std::memory_order_relaxed)) {
            int res = pcap_next_ex(handle_, &header, &packet);
            
            if (res == 1) {
                // Packet captured successfully
                packets_captured_.fetch_add(1, std::memory_order_relaxed);
                
                try {
                    if (processRawPacket(header, packet)) {
                        packets_processed_.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        packets_dropped_.fetch_add(1, std::memory_order_relaxed);
                    }
                } catch (...) {
                    packets_dropped_.fetch_add(1, std::memory_order_relaxed);
                }
                
            } else if (res == 0) {
                // Timeout, no packet
                std::this_thread::yield();
            } else if (res == -1) {
                // Error
                std::cerr << "[ERROR] pcap_next_ex: " << pcap_geterr(handle_) << "\n";
                break;
            } else if (res == -2) {
                // EOF (should not happen in live capture)
                break;
            }
        }
    }
    
    // ========================================================================
    // Parse Raw Packet (Ethernet -> IP -> TCP -> Payload)
    // ========================================================================
    bool processRawPacket(const pcap_pkthdr* header, const u_char* packet) {
        // Minimum packet size check
        if (header->len < 14) return false; // Ethernet header
        
        // Parse Ethernet header (14 bytes)
        // We skip it since we only care about IP layer
        
        // Parse IP header (starts at offset 14)
        if (header->len < 34) return false; // Ethernet + IP min
        
        const u_char* ip_header = packet + 14;
        
        // Check IP version
        uint8_t version = (ip_header[0] >> 4) & 0x0F;
        if (version != 4) return false; // Only IPv4
        
        // Get IP header length
        uint8_t ip_header_len = (ip_header[0] & 0x0F) * 4;
        if (ip_header_len < 20) return false; // Invalid
        
        // Check protocol (TCP = 6)
        uint8_t protocol = ip_header[9];
        if (protocol != 6) return false; // Not TCP
        
        // Extract source and destination IPs
        net::ipv4 src_ip({ip_header[12], ip_header[13], ip_header[14], ip_header[15]});
        net::ipv4 dst_ip({ip_header[16], ip_header[17], ip_header[18], ip_header[19]});
        
        // Parse TCP header
        if (header->len < 14 + ip_header_len + 20) return false; // Not enough data
        
        const u_char* tcp_header = ip_header + ip_header_len;
        
        // Extract ports
        uint16_t src_port = (tcp_header[0] << 8) | tcp_header[1];
        uint16_t dst_port = (tcp_header[2] << 8) | tcp_header[3];
        
        // Get TCP header length
        uint8_t tcp_header_len = ((tcp_header[12] >> 4) & 0x0F) * 4;
        if (tcp_header_len < 20) return false; // Invalid
        
        // Calculate payload offset and length
        size_t payload_offset = 14 + ip_header_len + tcp_header_len;
        if (header->len <= payload_offset) return false; // No payload
        
        const u_char* payload = packet + payload_offset;
        size_t payload_len = header->len - payload_offset;
        
        // Convert payload to std::vector<std::byte>
        std::vector<std::byte> data(payload_len);
        std::memcpy(data.data(), payload, payload_len);
        
        // Send to Grid-Watcher for analysis
        bool allowed = watcher_.processPacket(data, src_ip, dst_ip, src_port, dst_port);
        
        return allowed;
    }
};

} // namespace gw::capture