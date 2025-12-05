// ============================================================================
// FILE: src/main.cpp
// Grid-Watcher Demo with New Modular Structure
// ============================================================================

#include "grid_watcher/grid_watcher.hpp"
#include <iostream>
#include <iomanip>
#include <random>
#include <csignal>
#include <memory>

using namespace gw;

void setupConsole() {
    // Set output ke UTF-8 biar garis-garisnya nyambung
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Aktifkan ANSI Escape Sequences (biar bisa warna-warni & pindah kursor)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

// Global instance for signal handler
std::unique_ptr<scada::GridWatcher> g_watcher;

void signalHandler(int signum) {
    std::cout << "\n\n=== Shutting down Grid-Watcher ===\n";
    if (g_watcher) {
        g_watcher->stop();
    }
    exit(signum);
}

// ============================================================================
// Packet Generation Helpers
// ============================================================================

std::vector<std::byte> createModbusPacket(uint16_t transaction_id, 
                                          uint8_t unit_id,
                                          uint8_t function_code, 
                                          uint16_t address, 
                                          uint16_t count) {
    std::vector<std::byte> packet;
    
    // MBAP Header (7 bytes)
    packet.push_back(std::byte(transaction_id >> 8));
    packet.push_back(std::byte(transaction_id & 0xFF));
    packet.push_back(std::byte(0x00));  // Protocol ID
    packet.push_back(std::byte(0x00));
    packet.push_back(std::byte(0x00));  // Length MSB
    packet.push_back(std::byte(0x06));  // Length LSB
    packet.push_back(std::byte(unit_id));
    
    // PDU
    packet.push_back(std::byte(function_code));
    packet.push_back(std::byte(address >> 8));
    packet.push_back(std::byte(address & 0xFF));
    packet.push_back(std::byte(count >> 8));
    packet.push_back(std::byte(count & 0xFF));
    
    return packet;
}

// ============================================================================
// Attack Simulation Scenarios
// ============================================================================

void simulateNormalTraffic(scada::GridWatcher& watcher, int count = 10) {
    std::cout << "\n=== Simulating Normal SCADA Traffic ===\n";
    
    net::ipv4 plc_ip({192, 168, 1, 100});
    net::ipv4 scada_master_ip({192, 168, 1, 10});
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> addr_dist(100, 1000);
    std::uniform_int_distribution<> count_dist(1, 10);
    
    for (int i = 0; i < count; ++i) {
        auto packet = createModbusPacket(
            i + 1, 1, 0x03, addr_dist(gen), count_dist(gen)
        );
        
        watcher.processPacket(packet, scada_master_ip, plc_ip, 5000 + i, 502);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "✓ Normal traffic simulation completed\n";
}

void simulatePortScan(scada::GridWatcher& watcher) {
    std::cout << "\n=== Simulating Port Scan Attack ===\n";
    
    net::ipv4 attacker_ip({10, 0, 0, 50});
    net::ipv4 target_ip({192, 168, 1, 100});
    
    int dropped = 0;
    for (uint16_t port = 500; port < 520; ++port) {
        auto packet = createModbusPacket(1, 1, 0x03, 0, 1);
        bool allowed = watcher.processPacket(packet, attacker_ip, target_ip, 50000, port);
        if (!allowed) dropped++;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    std::cout << "✓ Port scan completed (" << dropped << " packets dropped)\n";
}

void simulateDoSAttack(scada::GridWatcher& watcher) {
    std::cout << "\n=== Simulating DoS Flood Attack ===\n";
    
    net::ipv4 attacker_ip({10, 0, 0, 66});
    net::ipv4 target_ip({192, 168, 1, 100});
    
    int dropped = 0;
    for (int i = 0; i < 2000; ++i) {
        auto packet = createModbusPacket(i, 1, 0x03, 0, 1);
        bool allowed = watcher.processPacket(packet, attacker_ip, target_ip, 60000, 502);
        if (!allowed) dropped++;
        if (i % 100 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "✓ DoS attack completed (" << dropped << " packets dropped)\n";
}

void simulateUnauthorizedWrite(scada::GridWatcher& watcher) {
    std::cout << "\n=== Simulating Unauthorized Write Attack ===\n";
    
    net::ipv4 attacker_ip({203, 0, 113, 45});
    net::ipv4 plc_ip({192, 168, 1, 100});
    
    int dropped = 0;
    for (int i = 0; i < 10; ++i) {
        auto packet = createModbusPacket(100 + i, 1, 0x10, i * 10, 1);
        bool allowed = watcher.processPacket(packet, attacker_ip, plc_ip, 40000 + i, 502);
        if (!allowed) dropped++;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    std::cout << "✓ Unauthorized write completed (" << dropped << " packets dropped)\n";
}

// ============================================================================
// Enhanced Statistics Display (with new metrics)
// ============================================================================

void printStatistics(const scada::GridWatcher& watcher) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "                  GRID-WATCHER STATISTICS\n";
    std::cout << std::string(80, '=') << "\n";
    
    auto stats = watcher.getStatistics();
    auto metrics = watcher.getMetrics();
    
    std::cout << "┌─ GENERAL ─────────────────────────────────────────────────────┐\n";
    std::cout << "│ Uptime:              " << std::setw(8) << stats.uptime.count() << " seconds\n";
    std::cout << "│ Packets Processed:   " << std::setw(10) << stats.packets_processed << "\n";
    std::cout << "│ Packets Per Second:  " << std::setw(10) << std::fixed << std::setprecision(2) 
              << stats.packets_per_second << "\n";
    std::cout << "└───────────────────────────────────────────────────────────────┘\n\n";
    
    std::cout << "┌─ TRAFFIC ─────────────────────────────────────────────────────┐\n";
    std::cout << "│ Allowed:             " << std::setw(10) << stats.packets_allowed 
              << " (" << std::fixed << std::setprecision(1) << stats.allow_rate_percent << "%)\n";
    std::cout << "│ Dropped:             " << std::setw(10) << stats.packets_dropped 
              << " (" << std::fixed << std::setprecision(1) << stats.drop_rate_percent << "%)\n";
    std::cout << "│ Throughput:          " << std::setw(10) << std::fixed << std::setprecision(2)
              << metrics.throughput.mbps << " Mbps\n";
    std::cout << "└───────────────────────────────────────────────────────────────┘\n\n";
    
    std::cout << "┌─ THREATS ─────────────────────────────────────────────────────┐\n";
    std::cout << "│ Detected:            " << std::setw(10) << stats.threats_detected << "\n";
    std::cout << "│ Rate (per min):      " << std::setw(10) << std::fixed << std::setprecision(2)
              << stats.threat_rate_per_minute << "\n";
    std::cout << "│ Active Blocks:       " << std::setw(10) << stats.active_blocks << "\n";
    std::cout << "│ Total Blocks:        " << std::setw(10) << stats.total_blocks << "\n";
    std::cout << "└───────────────────────────────────────────────────────────────┘\n\n";
    
    std::cout << "┌─ PERFORMANCE ─────────────────────────────────────────────────┐\n";
    std::cout << "│ Avg Latency:         " << std::setw(10) << std::fixed << std::setprecision(2)
              << metrics.packet_latency.avg_us << " μs\n";
    std::cout << "│ Min Latency:         " << std::setw(10) << std::fixed << std::setprecision(2)
              << metrics.packet_latency.min_ns / 1000.0 << " μs\n";
    std::cout << "│ Max Latency:         " << std::setw(10) << std::fixed << std::setprecision(2)
              << metrics.packet_latency.max_ns / 1000.0 << " μs\n";
    std::cout << "│ Memory Usage:        " << std::setw(10) << std::fixed << std::setprecision(2)
              << metrics.memory_usage_mb << " MB\n";
    std::cout << "└───────────────────────────────────────────────────────────────┘\n\n";
    
    auto blocked = watcher.getBlockedIPs();
    if (!blocked.empty()) {
        std::cout << "┌─ BLOCKED IPs ─────────────────────────────────────────────────┐\n";
        for (const auto& block : blocked) {
            std::cout << "│ " << std::left << std::setw(15) << block.ip.toString()
                      << " - " << std::setw(25) << scada::to_string(block.reason)
                      << " (" << block.violation_count << " violations)";
            if (block.permanent) std::cout << " [PERMANENT]";
            std::cout << "\n";
        }
        std::cout << "└───────────────────────────────────────────────────────────────┘\n";
    }
    
    std::cout << std::string(80, '=') << "\n\n";
}

// ============================================================================
// Main Function
// ============================================================================

int main() {
	setupConsole() ;
    std::cout << R"(
    ╔═══════════════════════════════════════════════════════════════╗
    ║                                                               ║
    ║        GRID-WATCHER v2.0: Ultra-Fast SCADA Security           ║
    ║                  (Optimized & Modular)                        ║
    ║                                                               ║
    ║  ⚡ Sub-microsecond packet processing latency                 ║
    ║  📊 Real-time performance metrics & monitoring                ║
    ║  🛡️  Automated threat detection & mitigation                  ║
    ║  🚀 Lock-free high-performance architecture                   ║
    ║                                                               ║
    ╚═══════════════════════════════════════════════════════════════╝
    )" << std::endl;
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // ====================================================================
        // Configure Detection Parameters (using new config module)
        // ====================================================================
        std::cout << "Initializing Grid-Watcher...\n";
        
        // Start with default config and customize
        auto config = scada::DetectionConfig::createDefault();
        
        // Adjust thresholds
        config.dos_packet_threshold = 500;
        config.port_scan_threshold = 10;
        config.write_read_ratio_threshold = 3.0;
        
        // Whitelist trusted SCADA master
        config.whitelisted_ips.push_back(net::ipv4({192, 168, 1, 10}));
        
        // ====================================================================
        // Initialize Grid-Watcher
        // ====================================================================
        g_watcher = std::make_unique<scada::GridWatcher>(
            config, 
            "grid_watcher_demo.log"
        );
        
        // Configure logging
        auto& logger = g_watcher->getLogger();
        logger.setMinLevel(capture::LogEntry::Level::INFO);
        
        std::cout << "Starting Grid-Watcher...\n";
        g_watcher->start();
        
        std::cout << "\n✓ Grid-Watcher is now monitoring the SCADA network\n";
        std::cout << "  Press Ctrl+C to stop\n\n";
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // ====================================================================
        // Run Attack Simulation Scenarios
        // ====================================================================
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "Starting Attack Simulation Scenarios\n";
        std::cout << std::string(80, '=') << "\n";
        
        // Scenario 1: Normal traffic (establish baseline)
        simulateNormalTraffic(*g_watcher, 50);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        printStatistics(*g_watcher);
        
        // Scenario 2: Port scan attack
        simulatePortScan(*g_watcher);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        printStatistics(*g_watcher);
        
        // Scenario 3: DoS flood attack
        simulateDoSAttack(*g_watcher);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        printStatistics(*g_watcher);
        
        // Scenario 4: Unauthorized write attempt
        simulateUnauthorizedWrite(*g_watcher);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        printStatistics(*g_watcher);
        
        // Scenario 5: Recovery with normal traffic
        std::cout << "\n=== System Recovery: Normal Traffic Resumed ===\n";
        simulateNormalTraffic(*g_watcher, 30);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // ====================================================================
        // Final Statistics
        // ====================================================================
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "              FINAL DEMONSTRATION RESULTS\n";
        std::cout << std::string(80, '=') << "\n";
        printStatistics(*g_watcher);
        
        std::cout << "\n✓ Demonstration completed successfully!\n";
        std::cout << "  Check 'grid_watcher_demo.log' for detailed logs\n";
        std::cout << "\n  Press Ctrl+C to exit or wait 10 seconds...\n";
        
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        // Graceful shutdown
        g_watcher->stop();
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ FATAL ERROR: " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "\n=== Grid-Watcher Shutdown Complete ===\n";
    return 0;
}
