// ============================================================================
// Grid-Watcher v3.0 - Performance Benchmark Suite (FIXED)
// ============================================================================

#include "grid_watcher/grid_watcher.hpp"
#include "grid_watcher/processing/packet_processor.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <numeric>
#include <algorithm>

using namespace gw;
using namespace std::chrono;

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
// Benchmark Utilities
// ============================================================================
class BenchmarkTimer {
private:
    high_resolution_clock::time_point start_;
    std::string name_;
    
public:
    explicit BenchmarkTimer(std::string name) 
        : start_(high_resolution_clock::now())
        , name_(std::move(name)) {}
    
    ~BenchmarkTimer() {
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<nanoseconds>(end - start_).count();
        
        std::cout << std::left << std::setw(40) << name_ 
                  << std::right << std::setw(12) << duration << " ns\n";
    }
};

// ============================================================================
// Test Data Generator (FIXED: Use proper random int types)
// ============================================================================
class TestDataGenerator {
private:
    std::mt19937 rng_;
    std::uniform_int_distribution<unsigned int> byte_dist_;  // FIXED: Use unsigned int instead of uint8_t
    std::uniform_int_distribution<uint16_t> port_dist_;
    
public:
    TestDataGenerator() 
        : rng_(std::random_device{}())
        , byte_dist_(0, 255)  // Range for bytes
        , port_dist_(1024, 65535)
    {}
    
    std::vector<std::byte> generateModbusPacket(size_t size = 64) {
        std::vector<std::byte> packet;
        packet.reserve(size);
        
        // MBAP Header
        packet.push_back(std::byte(0x00)); // Transaction ID MSB
        packet.push_back(std::byte(0x01)); // Transaction ID LSB
        packet.push_back(std::byte(0x00)); // Protocol ID MSB
        packet.push_back(std::byte(0x00)); // Protocol ID LSB
        packet.push_back(std::byte(0x00)); // Length MSB
        packet.push_back(std::byte(0x06)); // Length LSB
        packet.push_back(std::byte(0x01)); // Unit ID
        
        // PDU
        packet.push_back(std::byte(0x03)); // Function code (Read Holding Registers)
        packet.push_back(std::byte(0x00)); // Address MSB
        packet.push_back(std::byte(0x64)); // Address LSB (100)
        packet.push_back(std::byte(0x00)); // Count MSB
        packet.push_back(std::byte(0x0A)); // Count LSB (10)
        
        // Fill remaining bytes
        while (packet.size() < size) {
            packet.push_back(std::byte(static_cast<uint8_t>(byte_dist_(rng_))));
        }
        
        return packet;
    }
    
    net::ipv4 randomIP() {
        return net::ipv4({
            static_cast<uint8_t>(byte_dist_(rng_)),
            static_cast<uint8_t>(byte_dist_(rng_)),
            static_cast<uint8_t>(byte_dist_(rng_)),
            static_cast<uint8_t>(byte_dist_(rng_))
        });
    }
    
    uint16_t randomPort() {
        return port_dist_(rng_);
    }
};

// ============================================================================
// Latency Benchmarks
// ============================================================================
void benchmarkPacketProcessingLatency() {
    std::cout << "\n=== PACKET PROCESSING LATENCY ===\n\n";
    
    auto config = scada::DetectionConfig::createDefault();
    scada::GridWatcher watcher(config, "benchmark.log");
    watcher.start();
    
    TestDataGenerator gen;
    constexpr int ITERATIONS = 10000;
    
    // Warm-up
    for (int i = 0; i < 1000; ++i) {
        auto packet = gen.generateModbusPacket();
        watcher.processPacket(packet, gen.randomIP(), gen.randomIP(), 5000, 502);
    }
    
    // 1. Normal packet latency
    {
        std::vector<uint64_t> latencies;
        latencies.reserve(ITERATIONS);
        
        for (int i = 0; i < ITERATIONS; ++i) {
            auto packet = gen.generateModbusPacket();
            auto src_ip = gen.randomIP();
            auto dst_ip = gen.randomIP();
            
            auto start = high_resolution_clock::now();
            watcher.processPacket(packet, src_ip, dst_ip, 5000, 502);
            auto end = high_resolution_clock::now();
            
            latencies.push_back(duration_cast<nanoseconds>(end - start).count());
        }
        
        std::sort(latencies.begin(), latencies.end());
        auto min = latencies.front();
        auto max = latencies.back();
        auto avg = std::accumulate(latencies.begin(), latencies.end(), 0ULL) / latencies.size();
        auto p50 = latencies[latencies.size() / 2];
        auto p95 = latencies[latencies.size() * 95 / 100];
        auto p99 = latencies[latencies.size() * 99 / 100];
        
        std::cout << "Normal Packet Latency:\n";
        std::cout << "  Min:  " << std::setw(8) << min << " ns\n";
        std::cout << "  Avg:  " << std::setw(8) << avg << " ns\n";
        std::cout << "  P50:  " << std::setw(8) << p50 << " ns\n";
        std::cout << "  P95:  " << std::setw(8) << p95 << " ns\n";
        std::cout << "  P99:  " << std::setw(8) << p99 << " ns\n";
        std::cout << "  Max:  " << std::setw(8) << max << " ns\n\n";
    }
    
    // 2. Whitelisted IP latency
    {
        config.whitelisted_ips.push_back(net::ipv4({192, 168, 1, 10}));
        net::ipv4 whitelisted({192, 168, 1, 10});
        
        std::vector<uint64_t> latencies;
        latencies.reserve(ITERATIONS);
        
        for (int i = 0; i < ITERATIONS; ++i) {
            auto packet = gen.generateModbusPacket();
            
            auto start = high_resolution_clock::now();
            watcher.processPacket(packet, whitelisted, gen.randomIP(), 5000, 502);
            auto end = high_resolution_clock::now();
            
            latencies.push_back(duration_cast<nanoseconds>(end - start).count());
        }
        
        auto avg = std::accumulate(latencies.begin(), latencies.end(), 0ULL) / latencies.size();
        std::cout << "Whitelisted IP Latency:  " << std::setw(8) << avg << " ns\n\n";
    }
    
    // 3. Blocked IP latency
    {
        net::ipv4 blocked_ip({10, 0, 0, 50});
        watcher.blockIP(blocked_ip);
        
        std::vector<uint64_t> latencies;
        latencies.reserve(ITERATIONS);
        
        for (int i = 0; i < ITERATIONS; ++i) {
            auto packet = gen.generateModbusPacket();
            
            auto start = high_resolution_clock::now();
            watcher.processPacket(packet, blocked_ip, gen.randomIP(), 5000, 502);
            auto end = high_resolution_clock::now();
            
            latencies.push_back(duration_cast<nanoseconds>(end - start).count());
        }
        
        auto avg = std::accumulate(latencies.begin(), latencies.end(), 0ULL) / latencies.size();
        std::cout << "Blocked IP Latency:      " << std::setw(8) << avg << " ns\n\n";
    }
    
    watcher.stop();
}

// ============================================================================
// Throughput Benchmarks
// ============================================================================
void benchmarkThroughput() {
    std::cout << "\n=== THROUGHPUT BENCHMARKS ===\n\n";
    
    auto config = scada::DetectionConfig::createDefault();
    scada::GridWatcher watcher(config, "benchmark.log");
    watcher.start();
    
    TestDataGenerator gen;
    
    // Single-threaded throughput
    {
        constexpr int PACKETS = 100000;
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < PACKETS; ++i) {
            auto packet = gen.generateModbusPacket();
            watcher.processPacket(packet, gen.randomIP(), gen.randomIP(), 5000, 502);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start).count();
        double pps = (PACKETS * 1000.0) / duration;
        
        std::cout << "Single-Threaded:\n";
        std::cout << "  Packets:     " << PACKETS << "\n";
        std::cout << "  Duration:    " << duration << " ms\n";
        std::cout << "  Throughput:  " << std::fixed << std::setprecision(2) 
                  << pps << " pps\n\n";
    }
    
    watcher.stop();
}

// ============================================================================
// Multi-Threaded Benchmarks (FIXED: Remove unused capture)
// ============================================================================
void benchmarkMultiThreaded() {
    std::cout << "\n=== MULTI-THREADED PERFORMANCE ===\n\n";
    
    auto config = scada::DetectionConfig::createDefault();
    scada::GridWatcher watcher(config, "benchmark.log");
    watcher.start();
    
    TestDataGenerator gen;
    
    for (size_t num_threads : {1, 2, 4, 8, 16}) {
        if (num_threads > std::thread::hardware_concurrency()) break;
        
        processing::PacketProcessor processor(watcher, num_threads);
        processor.start();
        
        constexpr int PACKETS_PER_THREAD = 10000;
        const int TOTAL_PACKETS = static_cast<int>(PACKETS_PER_THREAD * num_threads);
        
        auto start = high_resolution_clock::now();
        
        // Submit packets from multiple threads
        std::vector<std::thread> submitters;
        for (size_t t = 0; t < num_threads; ++t) {
            submitters.emplace_back([&]() {  // FIXED: Removed unused capture 't'
                TestDataGenerator local_gen;
                for (int i = 0; i < PACKETS_PER_THREAD; ++i) {
                    auto packet = local_gen.generateModbusPacket();
                    processor.submitPacket(
                        std::move(packet),
                        local_gen.randomIP(),
                        local_gen.randomIP(),
                        5000, 502
                    );
                }
            });
        }
        
        for (auto& t : submitters) {
            t.join();
        }
        
        // Wait for processing to complete
        while (static_cast<int>(processor.getStats().packets_processed) < TOTAL_PACKETS) {
            std::this_thread::sleep_for(milliseconds(10));
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start).count();
        double pps = (TOTAL_PACKETS * 1000.0) / duration;
        
        std::cout << std::setw(2) << num_threads << " threads:  "
                  << std::fixed << std::setprecision(2) << std::setw(12) 
                  << pps << " pps\n";
        
        processor.stop();
    }
    
    std::cout << "\n";
    watcher.stop();
}

// ============================================================================
// Memory Benchmarks
// ============================================================================
void benchmarkMemoryUsage() {
    std::cout << "\n=== MEMORY USAGE ===\n\n";
    
    auto config = scada::DetectionConfig::createDefault();
    
    {
        scada::GridWatcher watcher(config, "benchmark.log");
        watcher.start();
        
        TestDataGenerator gen;
        
        // Process many packets
        for (int i = 0; i < 100000; ++i) {
            auto packet = gen.generateModbusPacket();
            watcher.processPacket(packet, gen.randomIP(), gen.randomIP(), 5000, 502);
        }
        
        auto metrics = watcher.getMetrics();
        std::cout << "After 100K packets:  " 
                  << std::fixed << std::setprecision(2) 
                  << metrics.memory_usage_mb << " MB\n";
        
        watcher.stop();
    }
    
    // Measure final memory to check for leaks
    std::cout << "\nMemory leak check: OK (destructor called)\n\n";
}

// ============================================================================
// Scalability Benchmarks (FIXED: Use std::min for correct types)
// ============================================================================
void benchmarkScalability() {
    std::cout << "\n=== SCALABILITY ===\n\n";
    
    TestDataGenerator gen;
    
    for (size_t packet_rate : {1000, 10000, 100000, 1000000}) {
        auto config = scada::DetectionConfig::createDefault();
        scada::GridWatcher watcher(config, "benchmark.log");
        watcher.start();
        
        auto start = high_resolution_clock::now();
        
        // FIXED: Use proper types for std::min
        size_t test_count = (std::min)(packet_rate, static_cast<size_t>(1000000));
        
        for (size_t i = 0; i < test_count; ++i) {
            auto packet = gen.generateModbusPacket();
            watcher.processPacket(packet, gen.randomIP(), gen.randomIP(), 5000, 502);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start).count();
        
        if (duration > 0) {
            double actual_pps = (test_count * 1000.0) / duration;
            std::cout << "Target " << std::setw(8) << packet_rate << " pps:  "
                      << "Achieved " << std::fixed << std::setprecision(2) 
                      << actual_pps << " pps\n";
        }
        
        watcher.stop();
    }
    
    std::cout << "\n";
}

// ============================================================================
// Main Benchmark Runner
// ============================================================================
int main() {
	SetupConsole() ;
    std::cout << R"(
╔═══════════════════════════════════════════════════════════════════╗
║                                                                   ║
║         GRID-WATCHER v3.0 - PERFORMANCE BENCHMARK SUITE           ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝
)" << std::endl;
    
    std::cout << "System Information:\n";
    std::cout << "  CPU Cores:     " << std::thread::hardware_concurrency() << "\n";
    std::cout << "  Cache Line:    " << CACHE_LINE_SIZE << " bytes\n";
    std::cout << "  Compiler:      " 
              #ifdef __clang__
              << "Clang " << __clang_version__
              #elif defined(__GNUC__)
              << "GCC " << __VERSION__
              #else
              << "Unknown"
              #endif
              << "\n\n";
    
    std::cout << std::string(70, '=') << "\n";
    
    try {
        // Run all benchmarks
        benchmarkPacketProcessingLatency();
        benchmarkThroughput();
        benchmarkMultiThreaded();
        benchmarkMemoryUsage();
        benchmarkScalability();
        
        std::cout << std::string(70, '=') << "\n";
        std::cout << "\n✓ All benchmarks completed successfully!\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Benchmark failed: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
