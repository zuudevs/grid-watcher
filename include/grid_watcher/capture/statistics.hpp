#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

namespace gw::capture {

    // ========================================================================
    // Performance Statistics (Cache-Aligned for Speed)
    // ========================================================================
    class Statistics {
    private:
        // Packet counters (cache-aligned to prevent false sharing)
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> packets_processed_{0};
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> packets_allowed_{0};
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> packets_dropped_{0};
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> bytes_processed_{0};
        
        // Threat counters
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> threats_detected_{0};
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> threats_mitigated_{0};
        
        // Block counters
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> total_blocks_{0};
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> active_blocks_{0};
        
        // Timing
        std::chrono::system_clock::time_point start_time_;
        
    public:
        Statistics() noexcept 
            : start_time_(std::chrono::system_clock::now()) {}
        
        // Reset statistics
        void reset() noexcept {
            packets_processed_.store(0, std::memory_order_relaxed);
            packets_allowed_.store(0, std::memory_order_relaxed);
            packets_dropped_.store(0, std::memory_order_relaxed);
            bytes_processed_.store(0, std::memory_order_relaxed);
            threats_detected_.store(0, std::memory_order_relaxed);
            threats_mitigated_.store(0, std::memory_order_relaxed);
            total_blocks_.store(0, std::memory_order_relaxed);
            active_blocks_.store(0, std::memory_order_relaxed);
            start_time_ = std::chrono::system_clock::now();
        }
        
        // Increment operations (hot path - optimized)
        void incrementPacketsProcessed() noexcept {
            packets_processed_.fetch_add(1, std::memory_order_relaxed);
        }
        
        void incrementPacketsAllowed() noexcept {
            packets_allowed_.fetch_add(1, std::memory_order_relaxed);
        }
        
        void incrementPacketsDropped() noexcept {
            packets_dropped_.fetch_add(1, std::memory_order_relaxed);
        }
        
        void addBytesProcessed(uint64_t bytes) noexcept {
            bytes_processed_.fetch_add(bytes, std::memory_order_relaxed);
        }
        
        void incrementThreatsDetected() noexcept {
            threats_detected_.fetch_add(1, std::memory_order_relaxed);
        }
        
        void incrementThreatsMitigated() noexcept {
            threats_mitigated_.fetch_add(1, std::memory_order_relaxed);
        }
        
        void incrementTotalBlocks() noexcept {
            total_blocks_.fetch_add(1, std::memory_order_relaxed);
            active_blocks_.fetch_add(1, std::memory_order_relaxed);
        }
        
        void decrementActiveBlocks() noexcept {
            active_blocks_.fetch_sub(1, std::memory_order_relaxed);
        }
        
        // Snapshot for reporting
        struct Snapshot {
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
            
            Snapshot() noexcept 
                : packets_processed(0)
                , packets_allowed(0)
                , packets_dropped(0)
                , bytes_processed(0)
                , threats_detected(0)
                , threats_mitigated(0)
                , total_blocks(0)
                , active_blocks(0)
                , packets_per_second(0.0)
                , bytes_per_second(0.0)
                , threat_rate_per_minute(0.0)
                , drop_rate_percent(0.0)
                , allow_rate_percent(0.0)
                , uptime(0) {}
        };
        
        Snapshot getSnapshot() const noexcept {
            Snapshot snap;
            
            // Atomic reads
            snap.packets_processed = packets_processed_.load(std::memory_order_relaxed);
            snap.packets_allowed = packets_allowed_.load(std::memory_order_relaxed);
            snap.packets_dropped = packets_dropped_.load(std::memory_order_relaxed);
            snap.bytes_processed = bytes_processed_.load(std::memory_order_relaxed);
            snap.threats_detected = threats_detected_.load(std::memory_order_relaxed);
            snap.threats_mitigated = threats_mitigated_.load(std::memory_order_relaxed);
            snap.total_blocks = total_blocks_.load(std::memory_order_relaxed);
            snap.active_blocks = active_blocks_.load(std::memory_order_relaxed);
            
            // Calculate derived metrics
            auto now = std::chrono::system_clock::now();
            snap.uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
            
            double uptime_seconds = snap.uptime.count();
            if (uptime_seconds > 0.0) {
                snap.packets_per_second = snap.packets_processed / uptime_seconds;
                snap.bytes_per_second = snap.bytes_processed / uptime_seconds;
                snap.threat_rate_per_minute = (snap.threats_detected / uptime_seconds) * 60.0;
            }
            
            if (snap.packets_processed > 0) {
                snap.drop_rate_percent = (snap.packets_dropped * 100.0) / snap.packets_processed;
                snap.allow_rate_percent = (snap.packets_allowed * 100.0) / snap.packets_processed;
            }
            
            return snap;
        }
        
        // Individual getters (for specific metrics)
        uint64_t getPacketsProcessed() const noexcept {
            return packets_processed_.load(std::memory_order_relaxed);
        }
        
        uint64_t getThreatsDetected() const noexcept {
            return threats_detected_.load(std::memory_order_relaxed);
        }
        
        uint64_t getActiveBlocks() const noexcept {
            return active_blocks_.load(std::memory_order_relaxed);
        }
        
        std::chrono::seconds getUptime() const noexcept {
            auto now = std::chrono::system_clock::now();
            return std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
        }
    };

} // namespace gw::capture
