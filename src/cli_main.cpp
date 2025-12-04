// ============================================================================
// Grid-Watcher v3.0 - Production CLI Application
// Ultra-Fast Multi-Threaded SCADA Security Monitor
// ============================================================================

#include "grid_watcher/grid_watcher.hpp"
#include "grid_watcher/processing/packet_processor.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <csignal>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

using namespace gw;

void SetupConsole() {
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

// ============================================================================
// Global State
// ============================================================================
struct GlobalState {
    std::unique_ptr<scada::GridWatcher> watcher;
    std::unique_ptr<processing::PacketProcessor> processor;
    std::atomic<bool> should_exit{false};
    std::string config_file;
} g_state;

// ============================================================================
// Configuration Management
// ============================================================================
struct AppConfig {
    scada::DetectionConfig detection;
    size_t worker_threads = std::thread::hardware_concurrency();
    std::string log_file = "grid_watcher.log";
    std::string interface = "any";
    uint16_t api_port = 8080;
    bool enable_api = true;
    bool daemon_mode = false;
    
    static AppConfig loadFromFile(const std::string& filename) {
        AppConfig cfg;
        
        // Simple config file parser (in production, use JSON/YAML)
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open config file: " + filename);
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') continue;
            
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            // Parse configuration
            if (key == "dos_threshold") {
                cfg.detection.dos_packet_threshold = std::stoi(value);
            } else if (key == "port_scan_threshold") {
                cfg.detection.port_scan_threshold = std::stoi(value);
            } else if (key == "worker_threads") {
                cfg.worker_threads = std::stoi(value);
            } else if (key == "log_file") {
                cfg.log_file = value;
            } else if (key == "interface") {
                cfg.interface = value;
            } else if (key == "api_port") {
                cfg.api_port = std::stoi(value);
            } else if (key == "enable_api") {
                cfg.enable_api = (value == "true" || value == "1");
            }
        }
        
        return cfg;
    }
    
    void saveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot write config file: " + filename);
        }
        
        file << "# Grid-Watcher Configuration\n\n";
        file << "# Detection Settings\n";
        file << "dos_threshold=" << detection.dos_packet_threshold << "\n";
        file << "port_scan_threshold=" << detection.port_scan_threshold << "\n\n";
        
        file << "# Performance Settings\n";
        file << "worker_threads=" << worker_threads << "\n\n";
        
        file << "# Logging\n";
        file << "log_file=" << log_file << "\n\n";
        
        file << "# Network\n";
        file << "interface=" << interface << "\n\n";
        
        file << "# API\n";
        file << "enable_api=" << (enable_api ? "true" : "false") << "\n";
        file << "api_port=" << api_port << "\n";
    }
};

// ============================================================================
// Signal Handlers
// ============================================================================
void signalHandler(int signum) {
    static std::atomic<int> signal_count{0};
    
    if (signal_count++ == 0) {
        std::cout << "\n\n[INFO] Received signal " << signum 
                  << ", shutting down gracefully...\n";
        g_state.should_exit.store(true);
    } else {
        std::cout << "\n[WARN] Received signal again, forcing exit...\n";
        exit(signum);
    }
}

void setupSignalHandlers() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    #ifndef _WIN32
    signal(SIGHUP, signalHandler);
    #endif
}

// ============================================================================
// Console UI
// ============================================================================
class ConsoleUI {
public:
    static void printBanner() {
        std::cout << R"(
╔═══════════════════════════════════════════════════════════════════════╗
║                                                                       ║
║              GRID-WATCHER v3.0 - Production Release                   ║
║        Ultra-Fast Multi-Threaded SCADA Security Monitor               ║
║                                                                       ║
║  ⚡ Multi-threaded packet processing                                  ║
║  🛡️  Real-time threat detection & mitigation                          ║
║  📊 Advanced performance monitoring                                   ║
║  🚀 Lock-free high-performance architecture                           ║
║  🌐 Web dashboard & REST API                                          ║
║                                                                       ║
╚═══════════════════════════════════════════════════════════════════════╝
)" << std::endl;
    }
    
    static void printHelp(const char* program_name) {
        std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
        std::cout << "Options:\n";
        std::cout << "  -c, --config FILE       Configuration file (default: grid_watcher.conf)\n";
        std::cout << "  -i, --interface IFACE   Network interface to monitor (default: any)\n";
        std::cout << "  -t, --threads N         Number of worker threads (default: CPU count)\n";
        std::cout << "  -l, --log FILE          Log file path (default: grid_watcher.log)\n";
        std::cout << "  -p, --port PORT         API server port (default: 8080)\n";
        std::cout << "  -d, --daemon            Run as daemon\n";
        std::cout << "  --no-api                Disable REST API\n";
        std::cout << "  -v, --verbose           Verbose output\n";
        std::cout << "  -h, --help              Show this help\n";
        std::cout << "  --version               Show version\n\n";
        
        std::cout << "Examples:\n";
        std::cout << "  " << program_name << " -i eth0 -t 8\n";
        std::cout << "  " << program_name << " -c /etc/grid-watcher.conf -d\n";
        std::cout << "  " << program_name << " --config production.conf --threads 16\n\n";
    }
    
    static void printVersion() {
        std::cout << "Grid-Watcher v3.0.0\n";
        std::cout << "Build: " << __DATE__ << " " << __TIME__ << "\n";
        std::cout << "Compiler: " << 
        #ifdef __clang__
            "Clang " << __clang_version__
        #elif defined(__GNUC__)
            "GCC " << __VERSION__
        #elif defined(_MSC_VER)
            "MSVC " << _MSC_VER
        #else
            "Unknown"
        #endif
        << "\n";
    }
    
    static void printStats(const scada::GridWatcher& watcher,
                          const processing::PacketProcessor& processor) {
        auto stats = watcher.getStatistics();
        auto metrics = watcher.getMetrics();
        auto proc_stats = processor.getStats();
        
        // Clear screen and move cursor to top
        #ifdef _WIN32
        system("cls");
        #else
        std::cout << "\033[2J\033[H";
        #endif
        
        std::cout << "╔═══════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║              GRID-WATCHER REAL-TIME DASHBOARD                     ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════════════╝\n\n";
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::cout << "Time: " << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        std::cout << " | Uptime: " << stats.uptime.count() << "s\n\n";
        
        std::cout << "┌─ PACKET PROCESSING ───────────────────────────────────────────────┐\n";
        std::cout << "│ Queued:           " << std::setw(12) << proc_stats.packets_queued << "    ";
        std::cout << "│ Processed:      " << std::setw(12) << proc_stats.packets_processed << " │\n";
        std::cout << "│ Queue Drops:      " << std::setw(12) << proc_stats.packets_dropped_queue_full << "    ";
        std::cout << "│ Rate:           " << std::setw(10) << std::fixed << std::setprecision(2) 
                  << stats.packets_per_second << " pps │\n";
        std::cout << "└───────────────────────────────────────────────────────────────────┘\n\n";
        
        std::cout << "┌─ TRAFFIC ANALYSIS ────────────────────────────────────────────────┐\n";
        std::cout << "│ Total:            " << std::setw(12) << stats.packets_processed << "    ";
        std::cout << "│ Throughput:     " << std::setw(8) << std::fixed << std::setprecision(2)
                  << metrics.throughput.mbps << " Mbps │\n";
        std::cout << "│ Allowed:          " << std::setw(12) << stats.packets_allowed 
                  << "    │ Drop Rate:      " << std::setw(8) << std::fixed << std::setprecision(2)
                  << stats.drop_rate_percent << " %   │\n";
        std::cout << "│ Dropped:          " << std::setw(12) << stats.packets_dropped << " │\n";
        std::cout << "└───────────────────────────────────────────────────────────────────┘\n\n";
        
        std::cout << "┌─ THREAT DETECTION ────────────────────────────────────────────────┐\n";
        std::cout << "│ Threats:          " << std::setw(12) << stats.threats_detected << "    ";
        std::cout << "│ Rate:           " << std::setw(8) << std::fixed << std::setprecision(2)
                  << stats.threat_rate_per_minute << "/min │\n";
        std::cout << "│ Active Blocks:    " << std::setw(12) << stats.active_blocks << "    ";
        std::cout << "│ Total Blocks:   " << std::setw(12) << stats.total_blocks << " │\n";
        std::cout << "└───────────────────────────────────────────────────────────────────┘\n\n";
        
        std::cout << "┌─ PERFORMANCE METRICS ─────────────────────────────────────────────┐\n";
        std::cout << "│ Latency (μs):     Min=" << std::setw(8) << std::fixed << std::setprecision(2)
                  << metrics.packet_latency.min_ns / 1000.0;
        std::cout << "  Avg=" << std::setw(8) << metrics.packet_latency.avg_us;
        std::cout << "  Max=" << std::setw(8) << metrics.packet_latency.max_ns / 1000.0 << " │\n";
        std::cout << "│ Memory Usage:     " << std::setw(8) << std::fixed << std::setprecision(2)
                  << metrics.memory_usage_mb << " MB                                   │\n";
        std::cout << "└───────────────────────────────────────────────────────────────────┘\n\n";
        
        std::cout << "[Press Ctrl+C to stop]\n";
    }
};

// ============================================================================
// System Tuning (Linux)
// ============================================================================
void optimizeSystemSettings() {
    #ifdef __linux__
    // Increase file descriptor limit
    struct rlimit rlim;
    rlim.rlim_cur = 65536;
    rlim.rlim_max = 65536;
    if (setrlimit(RLIMIT_NOFILE, &rlim) == 0) {
        std::cout << "[INFO] Increased file descriptor limit to 65536\n";
    }
    
    // Set process priority
    if (nice(-10) == -1) {
        std::cout << "[WARN] Could not increase process priority (requires root)\n";
    }
    #endif
}

// ============================================================================
// Main Application
// ============================================================================
int main(int argc, char* argv[]) {
	SetupConsole() ;

    try {
        // Parse command-line arguments
        AppConfig config;
        bool verbose = false;
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "-h" || arg == "--help") {
                ConsoleUI::printHelp(argv[0]);
                return 0;
            } else if (arg == "--version") {
                ConsoleUI::printVersion();
                return 0;
            } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
                g_state.config_file = argv[++i];
                config = AppConfig::loadFromFile(g_state.config_file);
            } else if ((arg == "-t" || arg == "--threads") && i + 1 < argc) {
                config.worker_threads = std::stoi(argv[++i]);
            } else if ((arg == "-l" || arg == "--log") && i + 1 < argc) {
                config.log_file = argv[++i];
            } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
                config.api_port = std::stoi(argv[++i]);
            } else if ((arg == "-i" || arg == "--interface") && i + 1 < argc) {
                config.interface = argv[++i];
            } else if (arg == "-d" || arg == "--daemon") {
                config.daemon_mode = true;
            } else if (arg == "--no-api") {
                config.enable_api = false;
            } else if (arg == "-v" || arg == "--verbose") {
                verbose = true;
            } else {
                std::cerr << "Unknown option: " << arg << "\n";
                ConsoleUI::printHelp(argv[0]);
                return 1;
            }
        }
        
        // Print banner
        if (!config.daemon_mode) {
            ConsoleUI::printBanner();
        }
        
        // Setup signal handlers
        setupSignalHandlers();
        
        // Optimize system settings
        optimizeSystemSettings();
        
        // Initialize Grid-Watcher
        std::cout << "[INFO] Initializing Grid-Watcher...\n";
        std::cout << "[INFO] Worker threads: " << config.worker_threads << "\n";
        std::cout << "[INFO] Log file: " << config.log_file << "\n";
        std::cout << "[INFO] Interface: " << config.interface << "\n";
        
        g_state.watcher = std::make_unique<scada::GridWatcher>(
            config.detection,
            config.log_file
        );
        
        // Configure logging
        auto& logger = g_state.watcher->getLogger();
        if (verbose) {
            logger.setMinLevel(monitor::LogEntry::Level::DEBUG);
        }
        
        // Start Grid-Watcher
        g_state.watcher->start();
        std::cout << "[INFO] Grid-Watcher started successfully\n";
        
        // Initialize packet processor
        g_state.processor = std::make_unique<processing::PacketProcessor>(
            *g_state.watcher,
            config.worker_threads
        );
        g_state.processor->start();
        std::cout << "[INFO] Packet processor started with " 
                  << config.worker_threads << " worker threads\n";
        
        // TODO: Initialize network capture (libpcap integration)
        // TODO: Start REST API server if enabled
        
        std::cout << "[INFO] Grid-Watcher is now monitoring network traffic\n";
        std::cout << "[INFO] Press Ctrl+C to stop\n\n";
        
        // Main monitoring loop
        while (!g_state.should_exit.load()) {
            if (!config.daemon_mode) {
                ConsoleUI::printStats(*g_state.watcher, *g_state.processor);
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Graceful shutdown
        std::cout << "\n[INFO] Shutting down...\n";
        
        if (g_state.processor) {
            g_state.processor->stop();
            std::cout << "[INFO] Packet processor stopped\n";
        }
        
        if (g_state.watcher) {
            g_state.watcher->stop();
            std::cout << "[INFO] Grid-Watcher stopped\n";
        }
        
        // Print final statistics
        auto final_stats = g_state.watcher->getStatistics();
        std::cout << "\n=== FINAL STATISTICS ===\n";
        std::cout << "Total packets processed: " << final_stats.packets_processed << "\n";
        std::cout << "Total threats detected: " << final_stats.threats_detected << "\n";
        std::cout << "Total uptime: " << final_stats.uptime.count() << " seconds\n";
        
        std::cout << "\n[INFO] Grid-Watcher shutdown complete\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL] Error: " << e.what() << "\n";
        return 1;
    }
}