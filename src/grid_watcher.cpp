// ============================================================================
// Grid-Watcher CLI with Real Packet Capture Support
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX 
#define NOGDI

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

#include "grid_watcher/grid_watcher.hpp"
#include "grid_watcher/capture/packet_capture.hpp"
#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <atomic>
#include <iomanip>

// Global state
static std::atomic<bool> g_running{true};
static gw::capture::PacketCapture* g_capture = nullptr;
static gw::scada::GridWatcher* g_watcher = nullptr;

void setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}

double getCurrentMemoryUsageMB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
#endif
    return 0.0;
}

void signalHandler(int) {
    if (g_running.exchange(false)) {
        std::cout << "\n\n[SIGNAL] Shutting down gracefully...\n";
        if (g_capture) g_capture->stop();
        if (g_watcher) g_watcher->stop();
    }
}

void printBanner() {
    std::cout << R"(
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║           GRID-WATCHER v3.0 - REAL CAPTURE MODE               ║
║         Ultra-Fast SCADA Security with Live Traffic           ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
)" << "\n";
}

void clearScreen() {
    std::cout << "\033[2J\033[1;1H";
}

void updateDashboard(const gw::scada::GridWatcher& watcher, 
                    const gw::capture::PacketCapture& capture,
                    int64_t uptime_sec) {
    auto stats = watcher.getStatistics();
    auto cap_stats = capture.getStats();
    auto metrics = watcher.getMetrics();
    double mem_usage = getCurrentMemoryUsageMB();
    
    auto now = std::chrono::system_clock::now();
    std::time_t time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif

    clearScreen();
    
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          GRID-WATCHER REAL-TIME CAPTURE DASHBOARD            ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "Time: " << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") 
              << " | Uptime: " << uptime_sec << "s\n\n";
    
    // Packet Capture Stats
    std::cout << "┌─ PACKET CAPTURE (LIVE) ───────────────────────────────────────┐\n";
    std::cout << "│ Captured:      " << std::setw(12) << cap_stats.packets_captured
              << " | Processed:   " << std::setw(9) << cap_stats.packets_processed << "     │\n";
    std::cout << "│ Dropped:       " << std::setw(12) << cap_stats.packets_dropped
              << " | Rate:        " << std::setw(9) << std::fixed << std::setprecision(1) 
              << stats.packets_per_second << " pps │\n";
    std::cout << "└───────────────────────────────────────────────────────────────┘\n\n";
    
    // Traffic Analysis
    std::cout << "┌─ TRAFFIC ANALYSIS ────────────────────────────────────────────┐\n";
    std::cout << "│ Allowed:       " << std::setw(12) << stats.packets_allowed
              << " | Throughput:  " << std::setw(7) << std::setprecision(2) 
              << metrics.throughput.mbps << " Mbps│\n";
    std::cout << "│ Blocked:       " << std::setw(12) << stats.packets_dropped
              << " | Drop Rate:   " << std::setw(7) << std::setprecision(2)
              << stats.drop_rate_percent << " %   │\n";
    std::cout << "└───────────────────────────────────────────────────────────────┘\n\n";
    
    // Security Alerts
    std::cout << "┌─ SECURITY ALERTS ─────────────────────────────────────────────┐\n";
    std::cout << "│ Threats:       " << std::setw(12) << stats.threats_detected
              << " | Mitigated:   " << std::setw(9) << stats.threats_mitigated << "     │\n";
    std::cout << "│ Active Blocks: " << std::setw(12) << stats.active_blocks
              << " | Total Blocks:" << std::setw(9) << stats.total_blocks << "     │\n";
    std::cout << "└───────────────────────────────────────────────────────────────┘\n\n";
    
    // Performance
    std::cout << "┌─ PERFORMANCE ─────────────────────────────────────────────────┐\n";
    std::cout << "│ Latency:       " << std::setw(9) << std::setprecision(2)
              << metrics.packet_latency.avg_us << " μs | Memory:      " 
              << std::setw(7) << mem_usage << " MB   │\n";
    std::cout << "└───────────────────────────────────────────────────────────────┘\n\n";
    
    // Recent Blocks
    auto blocked = watcher.getBlockedIPs();
    if (!blocked.empty()) {
        std::cout << "┌─ BLOCKED IPs (Recent 5) ──────────────────────────────────────┐\n";
        size_t count = 0;
        for (const auto& block : blocked) {
            if (count++ >= 5) break;
            std::cout << "│ " << std::left << std::setw(15) << block.ip.toString()
                      << " - " << std::setw(40) << gw::scada::to_string(block.reason)
                      << "│\n";
        }
        std::cout << "└───────────────────────────────────────────────────────────────┘\n\n";
    }
    
    std::cout << "[INFO] Capturing live network traffic. Press Ctrl+C to stop.\n";
}

void printHelp() {
    std::cout << R"(
Grid-Watcher CLI - Real Packet Capture Mode

USAGE:
    grid_watcher [OPTIONS]

OPTIONS:
    --list-interfaces      List all available network interfaces
    --interface <name>     Capture on specific interface (default: any)
    --filter <bpf>        BPF filter (default: "tcp port 502")
    --help                Show this help message

EXAMPLES:
    # List interfaces
    grid_watcher --list-interfaces
    
    # Capture on specific interface
    grid_watcher --interface "Ethernet"
    
    # Capture with custom filter
    grid_watcher --filter "tcp port 502 or tcp port 20000"
    
    # Capture all TCP traffic
    grid_watcher --filter "tcp"

NOTES:
    - Requires Administrator/root privileges
    - Requires Npcap installed (Windows) or libpcap (Linux)
    - Default filter captures Modbus TCP (port 502) only

)" << std::endl;
}

int main(int argc, char* argv[]) {
    setupConsole();
    
    // Parse arguments
    std::string interface_name = "any";
    std::string bpf_filter = "tcp port 502";
    bool list_only = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        }
        else if (arg == "--list-interfaces") {
            list_only = true;
        }
        else if (arg == "--interface" && i + 1 < argc) {
            interface_name = argv[++i];
        }
        else if (arg == "--filter" && i + 1 < argc) {
            bpf_filter = argv[++i];
        }
    }
    
    // List interfaces only
    if (list_only) {
        gw::capture::PacketCapture::listInterfaces();
        return 0;
    }
    
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    printBanner();
    
    try {
        // Create configuration
        auto config = gw::scada::DetectionConfig::createDefault();
        config.dos_packet_threshold = 500;
        config.port_scan_threshold = 10;
        
        // Create Grid-Watcher
        g_watcher = new gw::scada::GridWatcher(config);
        g_watcher->start();
        
        // Create Packet Capture
        g_capture = new gw::capture::PacketCapture(*g_watcher);
        
        if (!g_capture->start(interface_name, bpf_filter)) {
            std::cerr << "\n[ERROR] Failed to start packet capture!\n";
            std::cerr << "\n💡 Troubleshooting:\n";
            std::cerr << "  1. Run as Administrator (Windows) or root (Linux)\n";
            std::cerr << "  2. Install Npcap: https://npcap.com/\n";
            std::cerr << "  3. Check interface name with: --list-interfaces\n\n";
            
            delete g_capture;
            delete g_watcher;
            return 1;
        }
        
        // Main loop
        auto start_time = std::chrono::steady_clock::now();
        
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            auto now = std::chrono::steady_clock::now();
            int64_t uptime = std::chrono::duration_cast<std::chrono::seconds>(
                now - start_time
            ).count();
            
            updateDashboard(*g_watcher, *g_capture, uptime);
        }
        
        // Cleanup
        std::cout << "\n[INFO] Cleaning up...\n";
        g_capture->stop();
        g_watcher->stop();
        
        delete g_capture;
        delete g_watcher;
        
        std::cout << "[INFO] Shutdown complete.\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL] " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}