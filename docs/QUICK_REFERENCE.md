# Grid-Watcher Quick Reference Guide

## 🚀 Quick Start

### 1. Build & Run
```bash
# Clone & build
git clone <your-repo>
cd grid_watcher
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Run demo
./bin/grid_watcher
```

### 2. Basic Usage
```cpp
#include "zuu/grid_watcher.hpp"

// Create & start
auto config = gw::scada::DetectionConfig::createDefault();
gw::scada::GridWatcher watcher(config);
watcher.start();

// Process packets
bool allowed = watcher.processPacket(
    packet_data, src_ip, dst_ip, src_port, dst_port
);

// Get stats
auto stats = watcher.getStatistics();
```

## 📁 File Organization Map

### When to Edit Which File?

| Task | File to Edit | Location |
|------|-------------|----------|
| Change detection thresholds | `config.hpp` | `include/zuu/scada/` |
| Add new attack types | `types.hpp` | `include/zuu/scada/` |
| Modify Modbus parsing | `modbus_parser.hpp` | `include/zuu/scada/` |
| Add detection algorithm | `behavioral_analyzer.hpp` | `include/zuu/scada/` |
| Change mitigation logic | `mitigation_engine.hpp` | `include/zuu/scada/` |
| Adjust logging behavior | `logger.hpp` | `include/zuu/captureing/` |
| Add performance counters | `statistics.hpp` | `include/zuu/captureing/` |
| Create new metrics | `metrics.hpp` | `include/zuu/captureing/` |
| Modify main engine logic | `grid_watcher.hpp` | `include/zuu/` |
| Update demo scenarios | `main.cpp` | `src/` |

## 🔧 Common Tasks

### Add New Detection Algorithm

**File:** `include/zuu/scada/behavioral_analyzer.hpp`

```cpp
// In BehavioralAnalyzer::analyze() method
std::vector<ThreatAlert> analyze(const PacketMetadata& pkt) noexcept {
    std::vector<ThreatAlert> alerts;
    
    // YOUR NEW DETECTION HERE
    if (detectNewThreat(pkt)) {
        alerts.emplace_back(
            pkt.source_ip, pkt.dest_ip,
            AttackType::YOUR_NEW_TYPE,  // Add to types.hpp first
            ThreatLevel::HIGH,
            "Your threat description",
            0.85  // Confidence score
        );
    }
    
    return alerts;
}
```

### Add New Attack Type

**File:** `include/zuu/scada/types.hpp`

```cpp
enum class AttackType : uint8_t {
    // ... existing types ...
    YOUR_NEW_ATTACK = 11,  // Add here
};

// Add string conversion
inline const char* to_string(AttackType type) noexcept {
    switch (type) {
        // ... existing cases ...
        case AttackType::YOUR_NEW_ATTACK: return "Your New Attack";
        default: return "Unknown";
    }
}
```

### Adjust Detection Thresholds

**File:** `src/main.cpp` or your application code

```cpp
auto config = gw::scada::DetectionConfig::createDefault();

// Adjust thresholds
config.dos_packet_threshold = 1000;      // Packets/sec
config.port_scan_threshold = 15;         // Unique ports
config.write_read_ratio_threshold = 5.0; // Write/read ratio

gw::scada::GridWatcher watcher(config);
```

### Create Custom Configuration Preset

**File:** `include/zuu/scada/config.hpp`

```cpp
struct DetectionConfig {
    // ... existing fields ...
    
    static DetectionConfig createYourPreset() noexcept {
        DetectionConfig cfg;
        cfg.dos_packet_threshold = 1500;
        cfg.port_scan_threshold = 8;
        // ... customize ...
        return cfg;
    }
};
```

### Add Custom Metrics

**File:** `include/zuu/captureing/metrics.hpp`

```cpp
class YourMetricTracker {
private:
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> counter_{0};
    
public:
    void record(uint64_t value) noexcept {
        counter_.fetch_add(value, std::memory_order_relaxed);
    }
    
    uint64_t get() const noexcept {
        return counter_.load(std::memory_order_relaxed);
    }
};

// Add to MetricsManager
class MetricsManager {
    YourMetricTracker your_metric_;
    
public:
    YourMetricTracker& yourMetric() noexcept { return your_metric_; }
};
```

### Whitelist/Blacklist IPs

```cpp
// Add to configuration
config.whitelisted_ips.push_back(gw::net::ipv4({192, 168, 1, 10}));

// Or dynamically at runtime
watcher.addWhitelist(gw::net::ipv4({10, 0, 0, 5}));
watcher.blockIP(gw::net::ipv4({203, 0, 113, 45}));
```

## 🔍 Debugging Checklist

### Problem: High Packet Drop Rate

1. Check configuration thresholds
```cpp
auto config = watcher.getConfiguration(); // Add this method
std::cout << "DoS threshold: " << config.dos_packet_threshold << "\n";
```

2. Check statistics
```cpp
auto stats = watcher.getStatistics();
std::cout << "Drop rate: " << stats.drop_rate_percent << "%\n";
```

3. Enable debug logging
```cpp
watcher.getLogger().setMinLevel(monitoring::LogEntry::Level::DEBUG);
```

### Problem: False Positives

1. Check behavioral analyzer thresholds in `config.hpp`
2. Add suspicious IPs to whitelist
3. Adjust confidence scores in detection algorithms

### Problem: High Latency

1. Check performance metrics
```cpp
auto metrics = watcher.getMetrics();
std::cout << "Avg latency: " << metrics.packet_latency.avg_us << " μs\n";
```

2. Verify compiler optimizations are enabled
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=ON ..
```

3. Profile with perf
```bash
make perf
```

## 📊 Monitoring Guide

### Real-time Statistics

```cpp
// Get snapshot
auto stats = watcher.getStatistics();

// Key metrics to monitor
std::cout << "PPS: " << stats.packets_per_second << "\n";
std::cout << "Threats/min: " << stats.threat_rate_per_minute << "\n";
std::cout << "Drop rate: " << stats.drop_rate_percent << "%\n";
```

### Performance Metrics

```cpp
auto metrics = watcher.getMetrics();

// Latency
std::cout << "Min: " << metrics.packet_latency.min_ns / 1000.0 << " μs\n";
std::cout << "Avg: " << metrics.packet_latency.avg_us << " μs\n";
std::cout << "Max: " << metrics.packet_latency.max_ns / 1000.0 << " μs\n";

// Throughput
std::cout << "PPS: " << metrics.throughput.packets_per_sec << "\n";
std::cout << "Mbps: " << metrics.throughput.mbps << "\n";

// Resources
std::cout << "Memory: " << metrics.memory_usage_mb << " MB\n";
```

### Blocked IPs

```cpp
auto blocked = watcher.getBlockedIPs();
for (const auto& block : blocked) {
    std::cout << block.ip.toString() 
              << " - " << scada::to_string(block.reason)
              << " (" << block.violation_count << " violations)\n";
}
```

## ⚡ Performance Tuning

### Compiler Flags (CMakeLists.txt)

```cmake
# Maximum optimization
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto")

# Profiling (when needed)
set(CMAKE_CXX_FLAGS_PROFILING "-O2 -g -fno-omit-frame-pointer")
```

### Configuration Tuning

```cpp
DetectionConfig config;

// For high-traffic environments
config.dos_packet_threshold = 2000;  // Increase threshold
config.packet_buffer_size = 8192;    // Larger buffer
config.log_queue_size = 16384;       // More log capacity

// For low-latency requirements
config.worker_threads = 1;           // Single thread
config.auto_block_enabled = true;    // Immediate blocking
```

### Memory Tuning

Bloom filters size (`performance/bloom_filter.hpp`):
```cpp
// Current: 8192 bits
perf::BloomFilter<16384, 3> bigger_filter;  // Less false positives

// Current: 3 hashes
perf::BloomFilter<8192, 5> more_accurate;   // More accurate
```

## 🧪 Testing Guide

### Unit Test Structure (to be implemented)

```cpp
// tests/test_behavioral_analyzer.cpp
TEST(BehavioralAnalyzer, DetectsDoS) {
    auto config = DetectionConfig::createDefault();
    BehavioralAnalyzer analyzer(config);
    
    // Simulate DoS
    PacketMetadata pkt;
    pkt.source_ip = net::ipv4({10, 0, 0, 1});
    
    for (int i = 0; i < 1000; ++i) {
        auto threats = analyzer.analyze(pkt);
        // Assert detection
    }
}
```

### Integration Test

```cpp
// tests/test_grid_watcher.cpp
TEST(GridWatcher, BlocksAttacker) {
    auto config = DetectionConfig::createAggressive();
    GridWatcher watcher(config);
    watcher.start();
    
    // Simulate attack
    auto attacker = net::ipv4({10, 0, 0, 66});
    for (int i = 0; i < 2000; ++i) {
        bool allowed = watcher.processPacket(...);
        if (i > 1000) {
            EXPECT_FALSE(allowed);  // Should be blocked
        }
    }
}
```

## 📝 Logging Best Practices

### Log Levels

```cpp
// Use appropriate levels
logger.trace("Component", "Detailed debug info");     // Development only
logger.debug("Component", "Debug information");       // Troubleshooting
logger.info("Component", "Normal operation");         // Production
logger.warning("Component", "Potential issue");       // Attention needed
logger.error("Component", "Error occurred");          // Action required
logger.critical("Component", "Critical threat", threat); // Immediate action
```

### Performance Impact

- `TRACE/DEBUG`: High overhead, disable in production
- `INFO/WARNING`: Minimal overhead (<1%)
- `ERROR/CRITICAL`: Negligible overhead

## 🔐 Security Considerations

1. **Whitelist Management**: Always whitelist critical infrastructure IPs
2. **False Positive Handling**: Monitor and adjust thresholds regularly
3. **Log Security**: Protect log files (contain IP addresses)
4. **Resource Limits**: Set `max_concurrent_blocks` to prevent memory exhaustion

## 📈 Typical Performance Benchmarks

| Metric | Target | Typical | Excellent |
|--------|--------|---------|-----------|
| Packet latency | <1 μs | 340 ns | 200 ns |
| Throughput | 100K pps | 500K pps | 1M+ pps |
| Memory usage | <1 MB | 450 KB | 300 KB |
| False positive rate | <1% | 0.1% | 0.01% |

## 🚨 Common Pitfalls

1. **Forgetting to call `start()`**: Watcher won't process packets
2. **Too aggressive thresholds**: High false positive rate
3. **Not whitelisting trusted IPs**: Legitimate traffic blocked
4. **Ignoring statistics**: Missing performance issues
5. **Debug logging in production**: Significant overhead

## 📚 Additional Resources

- **CMakeLists.txt**: Build configuration and compiler flags
- **PROJECT_STRUCTURE.md**: Detailed architecture documentation
- **README.md**: Project overview and objectives
- **include/zuu/**: Header-only implementation (no .cpp files needed)

## 🤝 Contributing

When adding new features:

1. ✅ Keep hot-path code in headers (template/constexpr)
2. ✅ Use cache-aligned atomics for counters
3. ✅ Add appropriate statistics tracking
4. ✅ Update documentation
5. ✅ Write unit tests
6. ✅ Profile performance impact