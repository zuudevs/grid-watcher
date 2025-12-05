# Grid-Watcher API Reference

Complete API documentation for Grid-Watcher v2.0.

---

## Table of Contents

- [Core Classes](#core-classes)
  - [GridWatcher](#gridwatcher)
  - [DetectionConfig](#detectionconfig)
- [Monitoring](#monitoring)
  - [Logger](#logger)
  - [Statistics](#statistics)
  - [Metrics](#metrics)
- [Data Types](#data-types)
  - [PacketMetadata](#packetmetadata)
  - [ThreatAlert](#threatalert)
  - [BlockedIP](#blockedip)
- [Enumerations](#enumerations)
  - [AttackType](#attacktype)
  - [ThreatLevel](#threatlevel)
  - [ProtocolType](#protocoltype)

---

## Core Classes

### GridWatcher

Main engine class for packet processing and threat detection.

**Header**: `include/zuu/grid_watcher.hpp`  
**Namespace**: `gw::scada`

#### Constructor

```cpp
explicit GridWatcher(
    const DetectionConfig& config,
    const std::string& log_file = "grid_watcher.log"
)
```

Creates a new GridWatcher instance.

**Parameters**:
- `config`: Detection configuration
- `log_file`: Path to log file (optional)

**Throws**:
- `std::runtime_error`: If log file cannot be opened

**Example**:
```cpp
auto config = DetectionConfig::createDefault();
GridWatcher watcher(config, "my_log.log");
```

---

#### Methods

##### start()

```cpp
void start()
```

Starts the monitoring engine and background threads.

**Thread Safety**: Thread-safe, idempotent  
**Throws**: None (noexcept)

**Example**:
```cpp
watcher.start();
// Engine is now monitoring
```

---

##### stop()

```cpp
void stop()
```

Stops the monitoring engine gracefully.

**Thread Safety**: Thread-safe, idempotent  
**Blocks**: Until all threads are joined  
**Throws**: None (noexcept)

**Example**:
```cpp
watcher.stop();
// Engine stopped, logs flushed
```

---

##### processPacket()

```cpp
[[nodiscard]] bool processPacket(
    std::span<const std::byte> packet_data,
    const net::ipv4& source_ip,
    const net::ipv4& dest_ip,
    uint16_t source_port,
    uint16_t dest_port
) noexcept
```

Processes a packet and returns whether it should be allowed.

**Parameters**:
- `packet_data`: Raw packet bytes
- `source_ip`: Source IP address
- `dest_ip`: Destination IP address
- `source_port`: Source port number
- `dest_port`: Destination port number

**Returns**:
- `true`: Packet is allowed (pass through)
- `false`: Packet is blocked (drop)

**Performance**: ~340 ns typical, <1 µs worst case  
**Thread Safety**: Thread-safe (lock-free hot path)  
**Throws**: None (noexcept)

**Example**:
```cpp
std::vector<std::byte> packet = /* ... */;
net::ipv4 src({192, 168, 1, 100});
net::ipv4 dst({192, 168, 1, 10});

bool allowed = watcher.processPacket(packet, src, dst, 5000, 502);
if (!allowed) {
    std::cout << "Packet blocked!\n";
}
```

---

##### getStatistics()

```cpp
[[nodiscard]] Statistics getStatistics() const noexcept
```

Returns a snapshot of current statistics.

**Returns**: `Statistics` structure with all metrics

**Performance**: ~100 ns (multiple atomic reads)  
**Thread Safety**: Thread-safe  
**Throws**: None (noexcept)

**Example**:
```cpp
auto stats = watcher.getStatistics();
std::cout << "Packets: " << stats.packets_processed << "\n";
std::cout << "Threats: " << stats.threats_detected << "\n";
std::cout << "PPS: " << stats.packets_per_second << "\n";
```

**Statistics Structure**:
```cpp
struct Statistics {
    uint64_t packets_processed;
    uint64_t packets_allowed;
    uint64_t packets_dropped;
    uint64_t bytes_processed;
    uint64_t threats_detected;
    uint64_t threats_mitigated;
    uint64_t total_blocks;
    uint64_t active_blocks;
    
    // Derived metrics
    double packets_per_second;
    double bytes_per_second;
    double threat_rate_per_minute;
    double drop_rate_percent;
    double allow_rate_percent;
    std::chrono::seconds uptime;
};
```

---

##### getMetrics()

```cpp
[[nodiscard]] auto getMetrics() const noexcept
```

Returns detailed performance metrics.

**Returns**: Metrics structure with latency, throughput, and resource usage

**Performance**: ~200 ns  
**Thread Safety**: Thread-safe  
**Throws**: None (noexcept)

**Example**:
```cpp
auto metrics = watcher.getMetrics();

// Latency
std::cout << "Min: " << metrics.packet_latency.min_ns << " ns\n";
std::cout << "Avg: " << metrics.packet_latency.avg_us << " μs\n";
std::cout << "Max: " << metrics.packet_latency.max_ns << " ns\n";

// Throughput
std::cout << "PPS: " << metrics.throughput.packets_per_sec << "\n";
std::cout << "Mbps: " << metrics.throughput.mbps << "\n";

// Resources
std::cout << "Memory: " << metrics.memory_usage_mb << " MB\n";
```

**Metrics Structure**:
```cpp
struct Metrics {
    struct {
        uint64_t samples;
        uint64_t min_ns;
        uint64_t max_ns;
        double avg_ns;
        double avg_us;
        double avg_ms;
    } packet_latency;
    
    struct {
        uint64_t samples;
        uint64_t min_ns;
        uint64_t max_ns;
        double avg_ns;
        double avg_us;
        double avg_ms;
    } threat_latency;
    
    struct {
        double packets_per_sec;
        double bytes_per_sec;
        double mbps;
    } throughput;
    
    double memory_usage_mb;
};
```

---

##### getBlockedIPs()

```cpp
[[nodiscard]] std::vector<BlockedIP> getBlockedIPs() const
```

Returns list of currently blocked IPs.

**Returns**: Vector of `BlockedIP` structures

**Thread Safety**: Thread-safe (acquires read lock)  
**Throws**: May throw on allocation failure

**Example**:
```cpp
auto blocked = watcher.getBlockedIPs();
for (const auto& block : blocked) {
    std::cout << block.ip.toString() 
              << " - " << to_string(block.reason)
              << " (" << block.violation_count << " violations)\n";
}
```

---

##### blockIP()

```cpp
void blockIP(
    const net::ipv4& ip,
    AttackType reason = AttackType::NONE
)
```

Manually blocks an IP address.

**Parameters**:
- `ip`: IP address to block
- `reason`: Reason for blocking (optional)

**Thread Safety**: Thread-safe  
**Throws**: May throw on mitigation engine errors

**Example**:
```cpp
net::ipv4 attacker({10, 0, 0, 50});
watcher.blockIP(attacker, AttackType::DOS_FLOOD);
```

---

##### unblockIP()

```cpp
void unblockIP(const net::ipv4& ip)
```

Manually unblocks an IP address.

**Parameters**:
- `ip`: IP address to unblock

**Returns**: (implicitly) whether IP was blocked

**Thread Safety**: Thread-safe  
**Throws**: None

**Example**:
```cpp
net::ipv4 ip({10, 0, 0, 50});
watcher.unblockIP(ip);
```

---

##### addWhitelist()

```cpp
void addWhitelist(const net::ipv4& ip)
```

Adds IP to whitelist (never block).

**Parameters**:
- `ip`: IP address to whitelist

**Thread Safety**: Thread-safe  
**Throws**: None

**Example**:
```cpp
net::ipv4 trusted({192, 168, 1, 10});
watcher.addWhitelist(trusted);
```

---

##### removeWhitelist()

```cpp
void removeWhitelist(const net::ipv4& ip)
```

Removes IP from whitelist.

**Parameters**:
- `ip`: IP address to remove

**Thread Safety**: Thread-safe  
**Throws**: None

**Example**:
```cpp
net::ipv4 ip({192, 168, 1, 10});
watcher.removeWhitelist(ip);
```

---

##### getLogger()

```cpp
monitoring::Logger& getLogger() noexcept
```

Returns reference to logger instance.

**Returns**: Reference to logger

**Thread Safety**: Thread-safe  
**Throws**: None (noexcept)

**Example**:
```cpp
auto& logger = watcher.getLogger();
logger.setMinLevel(monitoring::LogEntry::Level::DEBUG);
```

---

## DetectionConfig

Configuration structure for detection thresholds and behavior.

**Header**: `include/zuu/scada/config.hpp`  
**Namespace**: `gw::scada`

### Fields

```cpp
struct DetectionConfig {
    // Port scan detection
    uint32_t port_scan_threshold = 10;
    std::chrono::seconds port_scan_window{10};
    
    // DoS detection
    uint32_t dos_packet_threshold = 1000;
    uint64_t dos_byte_threshold = 10'000'000;
    std::chrono::seconds dos_window{5};
    
    // Behavioral anomaly
    double write_read_ratio_threshold = 5.0;
    uint32_t exception_rate_threshold = 10;
    double packet_size_deviation_threshold = 3.0;
    
    // Network lists
    std::vector<net::ipv4> whitelisted_ips;
    std::vector<net::ipv4> blacklisted_ips;
    std::vector<uint16_t> monitored_ports{502, 20000};
    
    // Auto-mitigation
    bool auto_block_enabled = true;
    std::chrono::minutes auto_block_duration{60};
    uint32_t max_concurrent_blocks = 1000;
    
    // Performance tuning
    size_t packet_buffer_size = 4096;
    size_t log_queue_size = 8192;
    uint32_t worker_threads = 4;
};
```

### Static Methods

#### createDefault()

```cpp
static DetectionConfig createDefault() noexcept
```

Creates default configuration (balanced).

**Returns**: Default configuration

**Example**:
```cpp
auto config = DetectionConfig::createDefault();
```

---

#### createAggressive()

```cpp
static DetectionConfig createAggressive() noexcept
```

Creates aggressive configuration (high security).

**Returns**: Aggressive configuration

**Characteristics**:
- Lower thresholds (more sensitive)
- Faster blocking
- Longer block durations

**Example**:
```cpp
auto config = DetectionConfig::createAggressive();
```

---

#### createConservative()

```cpp
static DetectionConfig createConservative() noexcept
```

Creates conservative configuration (low false positives).

**Returns**: Conservative configuration

**Characteristics**:
- Higher thresholds (less sensitive)
- Slower blocking
- Shorter block durations

**Example**:
```cpp
auto config = DetectionConfig::createConservative();
```

---

### Methods

#### isValid()

```cpp
[[nodiscard]] bool isValid() const noexcept
```

Validates configuration parameters.

**Returns**:
- `true`: Configuration is valid
- `false`: Configuration has invalid parameters

**Example**:
```cpp
DetectionConfig config;
config.dos_packet_threshold = 0;  // Invalid!
if (!config.isValid()) {
    std::cerr << "Invalid configuration!\n";
}
```

---

## Monitoring

### Logger

High-performance async logger.

**Header**: `include/zuu/captureing/logger.hpp`  
**Namespace**: `gw::captureing`

#### Constructor

```cpp
explicit Logger(
    const std::string& filename,
    LogEntry::Level min_level = LogEntry::Level::INFO,
    bool console_output = true
)
```

**Parameters**:
- `filename`: Log file path
- `min_level`: Minimum log level (optional)
- `console_output`: Enable console output (optional)

**Throws**:
- `std::runtime_error`: If file cannot be opened

---

#### Methods

##### start()

```cpp
void start()
```

Starts async logging thread.

---

##### stop()

```cpp
void stop()
```

Stops logging and flushes all entries.

---

##### Log Methods

```cpp
void trace(const std::string& source, const std::string& message) noexcept
void debug(const std::string& source, const std::string& message) noexcept
void info(const std::string& source, const std::string& message) noexcept
void warning(const std::string& source, const std::string& message) noexcept
void error(const std::string& source, const std::string& message) noexcept
void critical(const std::string& source, const std::string& message, 
             const ThreatAlert& threat) noexcept
```

**Parameters**:
- `source`: Component name
- `message`: Log message
- `threat`: Threat details (critical only)

**Performance**: ~50 ns (hot path)  
**Thread Safety**: Thread-safe (lock-free)  
**Throws**: None (noexcept)

---

##### Configuration Methods

```cpp
void setMinLevel(LogEntry::Level level) noexcept
void setConsoleOutput(bool enable) noexcept
```

---

##### Statistics Methods

```cpp
uint64_t getLogsWritten() const noexcept
uint64_t getLogsDropped() const noexcept
```

---

### Statistics

Performance statistics tracker.

**Header**: `include/zuu/captureing/statistics.hpp`  
**Namespace**: `gw::captureing`

#### Methods

##### Increment Methods

```cpp
void incrementPacketsProcessed() noexcept
void incrementPacketsAllowed() noexcept
void incrementPacketsDropped() noexcept
void addBytesProcessed(uint64_t bytes) noexcept
void incrementThreatsDetected() noexcept
void incrementThreatsMitigated() noexcept
void incrementTotalBlocks() noexcept
void decrementActiveBlocks() noexcept
```

**Performance**: ~5 ns each (single atomic operation)  
**Thread Safety**: Thread-safe (atomic)  
**Throws**: None (noexcept)

---

##### getSnapshot()

```cpp
Snapshot getSnapshot() const noexcept
```

Returns consistent snapshot of all statistics.

**Returns**: `Snapshot` structure

**Performance**: ~100 ns  
**Thread Safety**: Thread-safe  
**Throws**: None (noexcept)

---

##### reset()

```cpp
void reset() noexcept
```

Resets all statistics to zero.

---

### Metrics

Advanced performance metrics.

**Header**: `include/zuu/captureing/metrics.hpp`  
**Namespace**: `gw::captureing`

#### LatencyTracker

Tracks latency with histogram.

##### record()

```cpp
void record(std::chrono::nanoseconds latency) noexcept
```

Records a latency sample.

**Performance**: ~20 ns  
**Thread Safety**: Thread-safe (atomic)

---

##### getStats()

```cpp
Stats getStats() const noexcept
```

Returns latency statistics.

---

#### ThroughputTracker

Tracks throughput with sliding window.

##### record()

```cpp
void record(uint64_t bytes) noexcept
```

Records throughput sample.

**Performance**: ~30 ns  
**Thread Safety**: Thread-safe (atomic)

---

##### getStats()

```cpp
Stats getStats(size_t window_sec = 10) const noexcept
```

Returns throughput statistics.

**Parameters**:
- `window_sec`: Window size in seconds (default: 10)

---

## Data Types

### PacketMetadata

Metadata extracted from packets.

**Header**: `include/zuu/scada/types.hpp`  
**Namespace**: `gw::scada`

```cpp
struct PacketMetadata {
    TimePoint timestamp;
    net::ipv4 source_ip;
    net::ipv4 dest_ip;
    uint16_t source_port;
    uint16_t dest_port;
    ProtocolType protocol;
    size_t packet_size;
    
    // Modbus specific
    uint16_t transaction_id;
    uint8_t unit_id;
    ModbusFunctionCode function_code;
    uint16_t register_address;
    uint16_t register_count;
    
    // Flags
    bool is_response;
    bool has_exception;
    bool is_malformed;
};
```

---

### ThreatAlert

Threat detection result.

**Header**: `include/zuu/scada/types.hpp`  
**Namespace**: `gw::scada`

```cpp
struct ThreatAlert {
    TimePoint timestamp;
    net::ipv4 source_ip;
    net::ipv4 dest_ip;
    AttackType attack_type;
    ThreatLevel severity;
    std::string description;
    double confidence_score;  // 0.0 - 1.0
    bool auto_mitigated;
};
```

---

### BlockedIP

Blocked IP information.

**Header**: `include/zuu/scada/mitigation_engine.hpp`  
**Namespace**: `gw::scada`

```cpp
struct BlockedIP {
    net::ipv4 ip;
    TimePoint blocked_at;
    TimePoint expires_at;
    AttackType reason;
    uint32_t violation_count;
    bool permanent;
    
    bool isExpired() const noexcept;
    void extend(std::chrono::minutes duration) noexcept;
};
```

---

## Enumerations

### AttackType

Types of detected attacks.

**Header**: `include/zuu/scada/types.hpp`  
**Namespace**: `gw::scada`

```cpp
enum class AttackType : uint8_t {
    NONE = 0,
    PORT_SCAN = 1,
    DOS_FLOOD = 2,
    MODBUS_COMMAND_INJECTION = 3,
    UNAUTHORIZED_WRITE = 4,
    ABNORMAL_TRAFFIC_PATTERN = 5,
    SUSPICIOUS_FUNCTION_CODE = 6,
    MALFORMED_PACKET = 7,
    REPLAY_ATTACK = 8,
    MAN_IN_THE_MIDDLE = 9,
    BRUTE_FORCE = 10
};
```

**String Conversion**:
```cpp
const char* to_string(AttackType type) noexcept;
```

---

### ThreatLevel

Severity levels for threats.

**Header**: `include/zuu/scada/types.hpp`  
**Namespace**: `gw::scada`

```cpp
enum class ThreatLevel : uint8_t {
    INFO = 0,
    LOW = 1,
    MEDIUM = 2,
    HIGH = 3,
    CRITICAL = 4
};
```

**String Conversion**:
```cpp
const char* to_string(ThreatLevel level) noexcept;
```

---

### ProtocolType

Supported SCADA protocols.

**Header**: `include/zuu/scada/types.hpp`  
**Namespace**: `gw::scada`

```cpp
enum class ProtocolType : uint8_t {
    MODBUS_TCP = 1,
    DNP3 = 2,
    IEC_104 = 3,
    OPC_UA = 4,
    UNKNOWN = 0xFF
};
```

**String Conversion**:
```cpp
const char* to_string(ProtocolType type) noexcept;
```

---

## Examples

### Complete Example

```cpp
#include "zuu/grid_watcher.hpp"
#include <iostream>

int main() {
    // Create configuration
    auto config = gw::scada::DetectionConfig::createAggressive();
    config.whitelisted_ips.push_back(gw::net::ipv4({192, 168, 1, 10}));
    
    // Initialize Grid-Watcher
    gw::scada::GridWatcher watcher(config, "grid_watcher.log");
    
    // Configure logger
    auto& logger = watcher.getLogger();
    logger.setMinLevel(gw::captureing::LogEntry::Level::INFO);
    
    // Start monitoring
    watcher.start();
    
    // Process packets
    std::vector<std::byte> packet = /* ... */;
    gw::net::ipv4 src({192, 168, 1, 100});
    gw::net::ipv4 dst({192, 168, 1, 10});
    
    bool allowed = watcher.processPacket(packet, src, dst, 5000, 502);
    
    // Get statistics
    auto stats = watcher.getStatistics();
    std::cout << "Packets: " << stats.packets_processed << "\n";
    std::cout << "Threats: " << stats.threats_detected << "\n";
    
    // Get metrics
    auto metrics = watcher.getMetrics();
    std::cout << "Latency: " << metrics.packet_latency.avg_us << " μs\n";
    std::cout << "Throughput: " << metrics.throughput.mbps << " Mbps\n";
    
    // Manual IP management
    watcher.blockIP(gw::net::ipv4({10, 0, 0, 50}));
    
    // Stop monitoring
    watcher.stop();
    
    return 0;
}
```

---

## Performance Considerations

### Hot Path Operations (<100 ns)

- `processPacket()` for whitelisted IPs: ~120 ns
- `processPacket()` for blocked IPs (cached): ~60 ns
- Statistics increments: ~5 ns each
- Bloom filter checks: ~10 ns each

### Warm Path Operations (100-1000 ns)

- `processPacket()` with full analysis: ~340 ns
- Logger writes: ~50 ns (async, no blocking)
- Metrics recording: ~20-30 ns

### Cold Path Operations (>1 µs)

- `getStatistics()`: ~100 ns (many atomic reads)
- `getMetrics()`: ~200 ns (computation)
- `getBlockedIPs()`: Variable (depends on count)

---

## Thread Safety

### Thread-Safe Operations

- All public methods are thread-safe
- Lock-free hot path (`processPacket`)
- Atomic counters with appropriate ordering
- Async logging (no blocking)

### Locking Strategy

- **Hot path**: Lock-free (bloom filters, atomics)
- **Mitigation engine**: Read-write locks (for consistency)
- **Logger**: Lock-free ring buffer + async thread
- **Statistics**: Atomic counters (relaxed ordering)

---

## Error Handling

### Exceptions

Most operations are `noexcept`. Exceptions may be thrown:

- Constructor failures (file I/O)
- Memory allocation failures (rare)
- Configuration validation errors

### Return Values

- `processPacket()`: Returns `bool` (allow/block)
- Most methods return by value or reference
- `nullptr` never returned (use `std::optional` where needed)

---

## Best Practices

### Performance

1. **Use presets** for quick configuration
2. **Enable LTO** in production builds
3. **Profile** before optimizing
4. **Batch operations** when possible

### Security

1. **Always whitelist** trusted systems
2. **Monitor statistics** regularly
3. **Adjust thresholds** based on your network
4. **Test in non-production** first

### Maintenance

1. **Check logs** regularly for threats
2. **Update configuration** as needed
3. **Monitor resource usage**
4. **Review blocked IPs** periodically

---

## See Also

- [README.md](README.md) - Project overview
- [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) - Architecture
- [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - Quick reference
- [CHANGELOG.md](CHANGELOG.md) - Version history