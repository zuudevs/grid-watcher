#pragma once

#include "../core/ipv4.hpp"
#include <vector>
#include <chrono>

namespace gw::scada {

    // ========================================================================
    // Detection Configuration (Extracted & Optimized)
    // ========================================================================
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
        
        // Validation
        [[nodiscard]] bool isValid() const noexcept {
            return dos_packet_threshold > 0 
                && dos_byte_threshold > 0
                && port_scan_threshold > 0
                && max_concurrent_blocks > 0;
        }
        
        // Preset configurations
        static DetectionConfig createConservative() noexcept {
            DetectionConfig cfg;
            cfg.dos_packet_threshold = 2000;
            cfg.port_scan_threshold = 20;
            cfg.write_read_ratio_threshold = 10.0;
            cfg.auto_block_duration = std::chrono::minutes(30);
            return cfg;
        }
        
        static DetectionConfig createAggressive() noexcept {
            DetectionConfig cfg;
            cfg.dos_packet_threshold = 500;
            cfg.port_scan_threshold = 5;
            cfg.write_read_ratio_threshold = 2.0;
            cfg.auto_block_duration = std::chrono::hours(2);
            return cfg;
        }
        
        static DetectionConfig createDefault() noexcept {
            return DetectionConfig{};
        }
    };

} // namespace gw::scada
