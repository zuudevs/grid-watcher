#pragma once

#include <atomic>
#include <chrono>
#include <algorithm>
#include <array>
#include <cstdint>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

namespace gw::capture {

    // ========================================================================
    // Latency Tracker (for hot-path performance monitoring)
    // ========================================================================
    class LatencyTracker {
    private:
        static constexpr size_t HISTOGRAM_SIZE = 32;
        
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> total_samples_{0};
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> total_latency_ns_{0};
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> min_latency_ns_{UINT64_MAX};
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> max_latency_ns_{0};
        
        // Simple histogram for percentile calculation
        std::array<std::atomic<uint64_t>, HISTOGRAM_SIZE> histogram_{};
        
    public:
        LatencyTracker() {
            for (auto& bucket : histogram_) {
                bucket.store(0, std::memory_order_relaxed);
            }
        }
        
        void record(std::chrono::nanoseconds latency) noexcept {
            uint64_t ns = latency.count();
            
            total_samples_.fetch_add(1, std::memory_order_relaxed);
            total_latency_ns_.fetch_add(ns, std::memory_order_relaxed);
            
            // Update min/max
            uint64_t current_min = min_latency_ns_.load(std::memory_order_relaxed);
            while (ns < current_min && 
                   !min_latency_ns_.compare_exchange_weak(current_min, ns, 
                                                          std::memory_order_relaxed)) {}
            
            uint64_t current_max = max_latency_ns_.load(std::memory_order_relaxed);
            while (ns > current_max && 
                   !max_latency_ns_.compare_exchange_weak(current_max, ns, 
                                                          std::memory_order_relaxed)) {}
            
            // Update histogram (logarithmic buckets)
            size_t bucket = std::min(static_cast<size_t>(63 - __builtin_clzll(ns | 1)) / 2, 
                                    HISTOGRAM_SIZE - 1);
            histogram_[bucket].fetch_add(1, std::memory_order_relaxed);
        }
        
        struct Stats {
            uint64_t samples;
            uint64_t min_ns;
            uint64_t max_ns;
            double avg_ns;
            double avg_us;
            double avg_ms;
        };
        
        Stats getStats() const noexcept {
            Stats stats{};
            stats.samples = total_samples_.load(std::memory_order_relaxed);
            
            if (stats.samples > 0) {
                stats.min_ns = min_latency_ns_.load(std::memory_order_relaxed);
                stats.max_ns = max_latency_ns_.load(std::memory_order_relaxed);
                uint64_t total = total_latency_ns_.load(std::memory_order_relaxed);
                stats.avg_ns = static_cast<double>(total) / stats.samples;
                stats.avg_us = stats.avg_ns / 1000.0;
                stats.avg_ms = stats.avg_us / 1000.0;
            } else {
                stats.min_ns = 0;
                stats.max_ns = 0;
                stats.avg_ns = 0.0;
                stats.avg_us = 0.0;
                stats.avg_ms = 0.0;
            }
            
            return stats;
        }
        
        void reset() noexcept {
            total_samples_.store(0, std::memory_order_relaxed);
            total_latency_ns_.store(0, std::memory_order_relaxed);
            min_latency_ns_.store(UINT64_MAX, std::memory_order_relaxed);
            max_latency_ns_.store(0, std::memory_order_relaxed);
            for (auto& bucket : histogram_) {
                bucket.store(0, std::memory_order_relaxed);
            }
        }
    };

    // ========================================================================
    // Throughput Tracker (packets/sec, bytes/sec)
    // ========================================================================
    class ThroughputTracker {
    private:
        static constexpr size_t WINDOW_SIZE = 60; // 60 seconds window
        
        struct Window {
            std::atomic<uint64_t> packets{0};
            std::atomic<uint64_t> bytes{0};
            std::atomic<uint64_t> timestamp_sec{0};
        };
        
        std::array<Window, WINDOW_SIZE> windows_;
        alignas(CACHE_LINE_SIZE) std::atomic<size_t> current_idx_{0};
        
    public:
        void record(uint64_t bytes) noexcept {
            auto now_sec = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000ULL;
            size_t idx = now_sec % WINDOW_SIZE;
            
            auto& window = windows_[idx];
            
            // Reset window if timestamp changed
            uint64_t expected_ts = window.timestamp_sec.load(std::memory_order_relaxed);
            if (expected_ts != now_sec) {
                if (window.timestamp_sec.compare_exchange_strong(expected_ts, now_sec, 
                                                                 std::memory_order_relaxed)) {
                    window.packets.store(0, std::memory_order_relaxed);
                    window.bytes.store(0, std::memory_order_relaxed);
                }
            }
            
            window.packets.fetch_add(1, std::memory_order_relaxed);
            window.bytes.fetch_add(bytes, std::memory_order_relaxed);
        }
        
        struct Stats {
            double packets_per_sec;
            double bytes_per_sec;
            double mbps; // Megabits per second
        };
        
        Stats getStats(size_t window_sec = 10) const noexcept {
            auto now_sec = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000ULL;
            
            uint64_t total_packets = 0;
            uint64_t total_bytes = 0;
            size_t valid_windows = 0;
            
            for (size_t i = 0; i < std::min(window_sec, WINDOW_SIZE); ++i) {
                size_t idx = (now_sec - i) % WINDOW_SIZE;
                const auto& window = windows_[idx];
                
                uint64_t ts = window.timestamp_sec.load(std::memory_order_relaxed);
                if (ts >= now_sec - window_sec) {
                    total_packets += window.packets.load(std::memory_order_relaxed);
                    total_bytes += window.bytes.load(std::memory_order_relaxed);
                    valid_windows++;
                }
            }
            
            Stats stats{};
            if (valid_windows > 0) {
                stats.packets_per_sec = static_cast<double>(total_packets) / valid_windows;
                stats.bytes_per_sec = static_cast<double>(total_bytes) / valid_windows;
                stats.mbps = (stats.bytes_per_sec * 8) / 1000000.0; // Convert to Mbps
            }
            
            return stats;
        }
    };

    // ========================================================================
    // Resource Monitor (CPU, Memory usage estimation)
    // ========================================================================
    class ResourceMonitor {
    private:
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> memory_allocated_{0};
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> memory_freed_{0};
        
    public:
        void recordAllocation(size_t bytes) noexcept {
            memory_allocated_.fetch_add(bytes, std::memory_order_relaxed);
        }
        
        void recordDeallocation(size_t bytes) noexcept {
            memory_freed_.fetch_add(bytes, std::memory_order_relaxed);
        }
        
        uint64_t getCurrentMemoryUsage() const noexcept {
            uint64_t allocated = memory_allocated_.load(std::memory_order_relaxed);
            uint64_t freed = memory_freed_.load(std::memory_order_relaxed);
            return (allocated > freed) ? (allocated - freed) : 0;
        }
        
        double getMemoryUsageMB() const noexcept {
            return getCurrentMemoryUsage() / (1024.0 * 1024.0);
        }
    };

    // ========================================================================
    // Combined Metrics Manager
    // ========================================================================
    class MetricsManager {
    private:
        LatencyTracker packet_processing_latency_;
        LatencyTracker threat_detection_latency_;
        ThroughputTracker throughput_;
        ResourceMonitor resources_;
        
    public:
        LatencyTracker& packetProcessingLatency() noexcept { return packet_processing_latency_; }
		const LatencyTracker& packetProcessingLatency() const noexcept { return packet_processing_latency_; }
        
        LatencyTracker& threatDetectionLatency() noexcept { return threat_detection_latency_; }
		const LatencyTracker& threatDetectionLatency() const noexcept { return threat_detection_latency_; }
        
        ThroughputTracker& throughput() noexcept { return throughput_; }
		const ThroughputTracker& throughput() const noexcept { return throughput_; }
        
        ResourceMonitor& resources() noexcept { return resources_; }
		const ResourceMonitor& resources() const noexcept { return resources_; }
        
        void reset() noexcept {
            packet_processing_latency_.reset();
            threat_detection_latency_.reset();
            // Note: throughput and resources auto-decay
        }
    };

} // namespace gw::capture