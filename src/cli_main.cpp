// [FIX] WAJIB PALING ATAS: Matikan macro min/max Windows
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX 
#define NOGDI

// [FIX] URUTAN INCLUDE KRUSIAL UNTUK WINDOWS
#ifdef _WIN32
#include <windows.h> 
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>

#include "grid_watcher/grid_watcher.hpp"
#include "grid_watcher/scada/config.hpp"

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

namespace {
    // Gunakan pointer untuk menghindari masalah destruktor statis
    struct GlobalState {
        std::atomic<bool> running{true};
        bool verbose = false;
    };

    static GlobalState* g_state = nullptr;
    static gw::scada::GridWatcher* g_watcher = nullptr;
}

struct AppConfig {
    uint16_t port = 502;
    std::string interface_ip = "0.0.0.0"; 
    bool dry_run = false;
};

// Helper: Ambil Real RAM Usage dari Windows Kernel
static double getCurrentMemoryUsageMB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
#endif
    return 0.0;
}

static void printBanner() {
    std::cout << R"(
   ______     _     __ _       __      __       __            
  / ____/____(_)___/ /| |     / /___ _/ /______/ /_  ___  _____
 / / __/ ___/ / __  / | | /| / / __ `/ __/ ___/ __ \/ _ \/ ___/
/ /_/ / /  / / /_/ /  | |/ |/ / /_/ / /_/ /__/ / / /  __/ /    
\____/_/  /_/\__,_/   |__/|__/\__,_/\__/\___/_/ /_/\___/_/     
                                                               
    SCADA Network Security Monitor | v3.0.0 (Real-Time Mode)
    )" << "\n\n";
}

static void signalHandler(int signum) {
    (void)signum;
    if (g_state && g_state->running) {
        std::cout << "\r\033[K"; 
        std::cout << "\n[CLI] Signal received. Initiating graceful shutdown...\n";
        g_state->running = false;
    }
}

static void clearScreen() {
    std::cout << "\033[2J\033[1;1H";
}

// Update Dashboard dengan Data ASLI dari Statistics.hpp
static void updateDashboard(const gw::scada::GridWatcher& watcher, int64_t uptime_sec) {
    // Ambil metrics snapshot
    auto stats = watcher.getStatistics();
    
    // Ambil data OS
    double mem_usage = getCurrentMemoryUsageMB();
    
    // Hitung throughput dalam Mbps (Megabits per second)
    // stats.bytes_per_second sudah dihitung di statistics.hpp
    double throughput_mbps = (stats.bytes_per_second * 8.0) / 1'000'000.0;

    // Format Waktu
    auto now = std::chrono::system_clock::now();
    std::time_t time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    #ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
    #else
        localtime_r(&time_t, &tm_buf);
    #endif

    clearScreen();
    std::cout << "╔═══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║               GRID-WATCHER REAL-TIME DASHBOARD                    ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "Time: " << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") 
              << " | Uptime: " << uptime_sec << "s\n\n";

    std::cout << "┌─ SYSTEM HEALTH ───────────────────────────────────────────────────┐\n";
    std::cout << "│ RAM Usage:   " << std::setw(10) << std::fixed << std::setprecision(2) << mem_usage << " MB   "
              << " | Status:      " << std::setw(10) << "ONLINE" << "     │\n"; 
    std::cout << "└───────────────────────────────────────────────────────────────────┘\n\n";

    std::cout << "┌─ TRAFFIC MONITOR (REAL) ──────────────────────────────────────────┐\n";
    // Gunakan field yang BENAR-BENAR ada di statistics.hpp
    std::cout << "│ Processed:     " << std::setw(12) << stats.packets_processed
              << " | Rate:        " << std::setw(9) << std::setprecision(1) << stats.packets_per_second << " pps │\n";
    std::cout << "│ Dropped:       " << std::setw(12) << stats.packets_dropped
              << " | Throughput:  " << std::setw(9) << std::setprecision(2) << throughput_mbps << " Mbps│\n";
    std::cout << "└───────────────────────────────────────────────────────────────────┘\n\n";

    std::cout << "┌─ SECURITY ALERTS ─────────────────────────────────────────────────┐\n";
    std::cout << "│ Threats:       " << std::setw(12) << stats.threats_detected
              << " | Mitigated:   " << std::setw(9) << stats.threats_mitigated << "     │\n";
    std::cout << "│ Active Blocks: " << std::setw(12) << stats.active_blocks
              << " | Total Blocks:" << std::setw(9) << stats.total_blocks << "     │\n";
    std::cout << "└───────────────────────────────────────────────────────────────────┘\n\n";

    std::cout << "[INFO] Listening on Raw Socket. Use 'scripts/traffic_gen.py' to generate load.\n";
    std::cout << "[INFO] Press Ctrl+C to stop.\n";
}

int main(int argc, char* argv[]) {
	SetupConsole() ;
    // Inisialisasi di heap
    g_state = new GlobalState();

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    AppConfig user_config;
    
    // Parse Arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            g_state->verbose = true;
        }
        else if (arg == "-p" && i + 1 < argc) {
            try {
                user_config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
            } catch (...) {}
        }
    }

    if(g_state->verbose) printBanner();

    try {
        // Setup Config
        auto scada_config = gw::scada::DetectionConfig::createDefault();
        scada_config.monitored_ports = { user_config.port };

        // Start Engine
        g_watcher = new gw::scada::GridWatcher(scada_config);
        g_watcher->start();

        if(!g_state->verbose) clearScreen();

        auto start_time = std::chrono::steady_clock::now();

        while (g_state->running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250)); // Update lebih cepat (4fps)
            
            auto now = std::chrono::steady_clock::now();
            int64_t uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

            if (!g_state->verbose) {
                updateDashboard(*g_watcher, uptime);
            }
        }

        g_watcher->stop();
        
        if (g_watcher) delete g_watcher;
        if (g_state) delete g_state;

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}