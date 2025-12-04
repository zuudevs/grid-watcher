// ============================================================================
// FILE: include/zuu/grid_watcher.hpp
// Grid-Watcher: Ultra-Fast SCADA IDS/IPS Engine (OPTIMIZED & MODULAR)
// ============================================================================

#pragma once

#include "scada/types.hpp"
#include "scada/config.hpp"
#include "scada/modbus_parser.hpp"
#include "scada/behavioral_analyzer.hpp"
#include "scada/mitigation_engine.hpp"
#include "monitor/logger.hpp"
#include "monitor/statistics.hpp"
#include "monitor/metrics.hpp"
#include "performance/bloom_filter.hpp"
#include "core/ipv4.hpp"
#include <thread>
#include <atomic>
#include <memory>
#include <span>

namespace gw::scada {

    // ========================================================================
    // Main Grid-Watcher Engine (Streamlined & Optimized)
    // ========================================================================
    class GridWatcher {
    private:
        // Configuration
        DetectionConfig config_;
        
        // Core components
        BehavioralAnalyzer analyzer_;
        MitigationEngine mitigation_;
        
        // Monitoring components
        std::unique_ptr<monitor::Logger> logger_;
        monitor::Statistics stats_;
        monitor::MetricsManager metrics_;
        
        // Fast lookup structures (bloom filters for O(1) checks)
        perf::BloomFilter<8192, 3> blocked_ips_cache_;
        perf::BloomFilter<8192, 3> whitelisted_ips_cache_;
        
        // Background threads
        std::atomic<bool> running_{false};
        std::thread monitor_thread_;
        std::thread cleanup_thread_;
        
    public:
        explicit GridWatcher(const DetectionConfig& config, 
                            const std::string& log_file = "grid_watcher.log")
            : config_(config)
            , analyzer_(config)
            , mitigation_(config)
            , logger_(std::make_unique<monitor::Logger>(log_file))
        {
            // Initialize bloom filters with whitelisted IPs
            for (const auto& ip : config_.whitelisted_ips) {
                whitelisted_ips_cache_.add(ip.to_uint32());
            }
            
            // Register mitigation callback
            mitigation_.registerCallback([this](const ThreatAlert& alert, MitigationAction action) {
                handleMitigationAction(alert, action);
            });
            
            logger_->start();
            logger_->info("GridWatcher", "Grid-Watcher initialized successfully");
        }
        
        ~GridWatcher() {
            stop();
        }
        
        // No copying
        GridWatcher(const GridWatcher&) = delete;
        GridWatcher& operator=(const GridWatcher&) = delete;
        
        // ====================================================================
        // Control Methods
        // ====================================================================
        
        void start() {
            if (running_.exchange(true)) {
                logger_->warning("GridWatcher", "Already running");
                return;
            }
            
            // Start background threads
            cleanup_thread_ = std::thread([this]() { cleanupLoop(); });
            monitor_thread_ = std::thread([this]() { monitorLoop(); });
            
            logger_->info("GridWatcher", "Grid-Watcher started - Monitoring SCADA network");
        }
        
        void stop() {
            if (!running_.exchange(false)) return;
            
            logger_->info("GridWatcher", "Stopping Grid-Watcher...");
            
            if (cleanup_thread_.joinable()) cleanup_thread_.join();
            if (monitor_thread_.joinable()) monitor_thread_.join();
            
            logger_->stop();
        }
        
        // ====================================================================
        // HOT PATH - Packet Processing (Ultra-Optimized)
        // ====================================================================
        
        [[nodiscard]] bool processPacket(
            std::span<const std::byte> packet_data,
            const net::ipv4& source_ip,
            const net::ipv4& dest_ip,
            uint16_t source_port,
            uint16_t dest_port) noexcept 
        {
            // Start latency measurement
            auto start = std::chrono::steady_clock::now();
            
            // Update statistics
            stats_.incrementPacketsProcessed();
            
            uint32_t src_ip_int = source_ip.to_uint32();
            
            // FAST PATH 1: Check whitelist (bloom filter - O(1))
            if (UNLIKELY(whitelisted_ips_cache_.contains(src_ip_int))) {
                stats_.incrementPacketsAllowed();
                metrics_.throughput().record(packet_data.size());
                return true; // ALLOW
            }
            
            // FAST PATH 2: Check blocked IPs (bloom filter - O(1))
            if (UNLIKELY(blocked_ips_cache_.contains(src_ip_int))) {
                // Double-check with mitigation engine (bloom filter can have false positives)
                if (mitigation_.isBlocked(source_ip)) {
                    stats_.incrementPacketsDropped();
                    recordLatency(start);
                    return false; // DROP
                }
            }
            
            // Parse packet metadata (minimal parsing for speed)
            PacketMetadata meta = buildPacketMetadata(
                packet_data, source_ip, dest_ip, source_port, dest_port
            );
            
            // Check if should drop before analysis (rate limiting)
            if (mitigation_.shouldDropPacket(meta)) {
                stats_.incrementPacketsDropped();
                recordLatency(start);
                return false; // DROP
            }
            
            // Behavioral analysis (detect threats)
            auto threat_start = std::chrono::steady_clock::now();
            auto threats = analyzer_.analyze(meta);
            metrics_.threatDetectionLatency().record(
                std::chrono::steady_clock::now() - threat_start
            );
            
            // Process threats
            bool should_drop = false;
            for (const auto& threat : threats) {
                stats_.incrementThreatsDetected();
                
                // Log threat
                logger_->critical("ThreatDetector", threat.description, threat);
                
                // Trigger mitigation
                auto action = mitigation_.mitigate(threat);
                
                // Update bloom filter cache if IP was blocked
                if (action == MitigationAction::BLOCK_IP) {
                    blocked_ips_cache_.add(src_ip_int);
                }
                
                // Check if packet should be dropped
                if (action == MitigationAction::DROP_PACKET || 
                    action == MitigationAction::BLOCK_IP) {
                    should_drop = true;
                }
            }
            
            // Update final statistics
            if (should_drop) {
                stats_.incrementPacketsDropped();
            } else {
                stats_.incrementPacketsAllowed();
                metrics_.throughput().record(packet_data.size());
            }
            
            recordLatency(start);
            return !should_drop; // ALLOW if not dropped
        }
        
        // ====================================================================
        // Statistics & Management
        // ====================================================================
        
        using Statistics = monitor::Statistics::Snapshot;
        
        [[nodiscard]] Statistics getStatistics() const noexcept {
            return stats_.getSnapshot();
        }
        
        [[nodiscard]] auto getMetrics() const noexcept {
            struct Metrics {
                monitor::LatencyTracker::Stats packet_latency;
                monitor::LatencyTracker::Stats threat_latency;
                monitor::ThroughputTracker::Stats throughput;
                double memory_usage_mb;
            };
            
            Metrics m;
            m.packet_latency = metrics_.packetProcessingLatency().getStats();
            m.threat_latency = metrics_.threatDetectionLatency().getStats();
            m.throughput = metrics_.throughput().getStats();
            m.memory_usage_mb = metrics_.resources().getMemoryUsageMB();
            return m;
        }
        
        [[nodiscard]] std::vector<BlockedIP> getBlockedIPs() const {
            return mitigation_.getBlockedIPs();
        }
        
        // Manual IP management
        void blockIP(const net::ipv4& ip, AttackType reason = AttackType::NONE) {
            mitigation_.blockIP(ip, reason, config_.auto_block_duration);
            blocked_ips_cache_.add(ip.to_uint32());
            logger_->warning("ManualControl", "IP manually blocked: " + ip.toString());
        }
        
        void unblockIP(const net::ipv4& ip) {
            if (mitigation_.unblockIP(ip)) {
                logger_->info("ManualControl", "IP manually unblocked: " + ip.toString());
            }
        }
        
        void addWhitelist(const net::ipv4& ip) {
            mitigation_.addWhitelist(ip);
            whitelisted_ips_cache_.add(ip.to_uint32());
            logger_->info("ManualControl", "IP added to whitelist: " + ip.toString());
        }
        
        void removeWhitelist(const net::ipv4& ip) {
            mitigation_.removeWhitelist(ip);
            logger_->info("ManualControl", "IP removed from whitelist: " + ip.toString());
        }
        
        // Logger access
        monitor::Logger& getLogger() noexcept { return *logger_; }
        
    private:
        // ====================================================================
        // Helper Methods
        // ====================================================================
        
        PacketMetadata buildPacketMetadata(
            std::span<const std::byte> packet_data,
            const net::ipv4& source_ip,
            const net::ipv4& dest_ip,
            uint16_t source_port,
            uint16_t dest_port) noexcept 
        {
            PacketMetadata meta;
            meta.source_ip = source_ip;
            meta.dest_ip = dest_ip;
            meta.source_port = source_port;
            meta.dest_port = dest_port;
            meta.packet_size = packet_data.size();
            meta.timestamp = std::chrono::system_clock::now();
            
            // Protocol detection and parsing
            if (dest_port == 502 || source_port == 502) {
                // Modbus TCP
                if (isModbusTCP(packet_data)) {
                    auto parsed = ModbusParser::parse(packet_data);
                    if (parsed) {
                        meta = *parsed;
                        meta.source_ip = source_ip;
                        meta.dest_ip = dest_ip;
                        meta.source_port = source_port;
                        meta.dest_port = dest_port;
                    } else {
                        meta.is_malformed = true;
                    }
                } else {
                    meta.is_malformed = true;
                }
                meta.protocol = ProtocolType::MODBUS_TCP;
            }
            // Add more protocol parsers here (DNP3, IEC-104, etc.)
            
            return meta;
        }
        
        void recordLatency(std::chrono::steady_clock::time_point start) noexcept {
            auto end = std::chrono::steady_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            metrics_.packetProcessingLatency().record(latency);
        }
        
        void handleMitigationAction(const ThreatAlert& alert, MitigationAction action) {
            std::ostringstream msg;
            msg << "Mitigation: " << to_string(action)
                << " for " << alert.source_ip.toString()
                << " due to " << to_string(alert.attack_type);
            
            if (action == MitigationAction::BLOCK_IP) {
                logger_->warning("Mitigation", msg.str());
            } else {
                logger_->info("Mitigation", msg.str());
            }
        }
        
        // ====================================================================
        // Background Threads
        // ====================================================================
        
        void cleanupLoop() {
            while (running_.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::minutes(1));
                
                if (!running_.load(std::memory_order_relaxed)) break;
                
                // Cleanup expired blocks
                mitigation_.cleanup();
                
                logger_->info("Cleanup", "Periodic cleanup completed");
            }
        }
        
        void monitorLoop() {
            while (running_.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::seconds(30));
                
                if (!running_.load(std::memory_order_relaxed)) break;
                
                auto stats = getStatistics();
                auto metrics = getMetrics();
                
                std::ostringstream msg;
                msg << "Stats: "
                    << stats.packets_processed << " pkts ("
                    << std::fixed << std::setprecision(1) << stats.packets_per_second << " pps) | "
                    << stats.threats_detected << " threats ("
                    << std::fixed << std::setprecision(2) << stats.threat_rate_per_minute << "/min) | "
                    << "Latency: " << std::fixed << std::setprecision(2) 
                    << metrics.packet_latency.avg_us << " μs | "
                    << stats.active_blocks << " active blocks";
                
                logger_->info("Monitor", msg.str());
            }
        }
    };

} // namespace gw::scada
