#pragma once

#include "../grid_watcher.hpp"
#include "../processing/packet_processor.hpp"
#include <string>
#include <sstream>
#include <thread>
#include <atomic>
#include <vector>
#include <unordered_map>

// Simplified HTTP server (for production, use a proper library like Crow/Drogon)
namespace gw::web {

// ============================================================================
// JSON Helper (minimal implementation)
// ============================================================================
class JSON {
public:
    static std::string escape(const std::string& str) {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '\"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c;
            }
        }
        return result;
    }
    
    template<typename T>
    static std::string number(T value) {
        return std::to_string(value);
    }
    
    static std::string string(const std::string& value) {
        return "\"" + escape(value) + "\"";
    }
    
    static std::string boolean(bool value) {
        return value ? "true" : "false";
    }
};

// ============================================================================
// API Response Builder
// ============================================================================
class APIResponse {
private:
    int status_code_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
    
public:
    APIResponse(int status = 200) : status_code_(status) {
        headers_["Content-Type"] = "application/json";
        headers_["Access-Control-Allow-Origin"] = "*";
    }
    
    APIResponse& setStatus(int code) {
        status_code_ = code;
        return *this;
    }
    
    APIResponse& setHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
        return *this;
    }
    
    APIResponse& setBody(const std::string& body) {
        body_ = body;
        headers_["Content-Length"] = std::to_string(body.size());
        return *this;
    }
    
    std::string build() const {
        std::ostringstream ss;
        
        // Status line
        ss << "HTTP/1.1 " << status_code_ << " ";
        ss << getStatusText(status_code_) << "\r\n";
        
        // Headers
        for (const auto& [key, value] : headers_) {
            ss << key << ": " << value << "\r\n";
        }
        ss << "\r\n";
        
        // Body
        ss << body_;
        
        return ss.str();
    }
    
private:
    static const char* getStatusText(int code) {
        switch (code) {
            case 200: return "OK";
            case 201: return "Created";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            default: return "Unknown";
        }
    }
};

// ============================================================================
// API Endpoints
// ============================================================================
class GridWatcherAPI {
private:
    scada::GridWatcher& watcher_;
    processing::PacketProcessor* processor_;
    
public:
    GridWatcherAPI(scada::GridWatcher& watcher, 
                   processing::PacketProcessor* processor = nullptr)
        : watcher_(watcher), processor_(processor) {}
    
    // GET /api/status
    std::string getStatus() {
        std::ostringstream json;
        json << "{\n";
        json << "  \"status\": \"running\",\n";
        json << "  \"version\": \"3.0.0\",\n";
        json << "  \"uptime\": " << watcher_.getStatistics().uptime.count() << "\n";
        json << "}";
        return json.str();
    }
    
    // GET /api/statistics
    std::string getStatistics() {
        auto stats = watcher_.getStatistics();
        
        std::ostringstream json;
        json << std::fixed << std::setprecision(2);
        json << "{\n";
        json << "  \"packets\": {\n";
        json << "    \"processed\": " << stats.packets_processed << ",\n";
        json << "    \"allowed\": " << stats.packets_allowed << ",\n";
        json << "    \"dropped\": " << stats.packets_dropped << ",\n";
        json << "    \"per_second\": " << stats.packets_per_second << ",\n";
        json << "    \"drop_rate_percent\": " << stats.drop_rate_percent << "\n";
        json << "  },\n";
        json << "  \"threats\": {\n";
        json << "    \"detected\": " << stats.threats_detected << ",\n";
        json << "    \"rate_per_minute\": " << stats.threat_rate_per_minute << ",\n";
        json << "    \"active_blocks\": " << stats.active_blocks << ",\n";
        json << "    \"total_blocks\": " << stats.total_blocks << "\n";
        json << "  },\n";
        json << "  \"uptime_seconds\": " << stats.uptime.count() << "\n";
        json << "}";
        
        return json.str();
    }
    
    // GET /api/metrics
    std::string getMetrics() {
        auto metrics = watcher_.getMetrics();
        
        std::ostringstream json;
        json << std::fixed << std::setprecision(2);
        json << "{\n";
        json << "  \"latency\": {\n";
        json << "    \"min_ns\": " << metrics.packet_latency.min_ns << ",\n";
        json << "    \"max_ns\": " << metrics.packet_latency.max_ns << ",\n";
        json << "    \"avg_ns\": " << metrics.packet_latency.avg_ns << ",\n";
        json << "    \"avg_us\": " << metrics.packet_latency.avg_us << ",\n";
        json << "    \"samples\": " << metrics.packet_latency.samples << "\n";
        json << "  },\n";
        json << "  \"throughput\": {\n";
        json << "    \"packets_per_sec\": " << metrics.throughput.packets_per_sec << ",\n";
        json << "    \"bytes_per_sec\": " << metrics.throughput.bytes_per_sec << ",\n";
        json << "    \"mbps\": " << metrics.throughput.mbps << "\n";
        json << "  },\n";
        json << "  \"memory_usage_mb\": " << metrics.memory_usage_mb << "\n";
        json << "}";
        
        return json.str();
    }
    
    // GET /api/blocks
    std::string getBlockedIPs() {
        auto blocked = watcher_.getBlockedIPs();
        
        std::ostringstream json;
        json << "{\n";
        json << "  \"total\": " << blocked.size() << ",\n";
        json << "  \"blocks\": [\n";
        
        for (size_t i = 0; i < blocked.size(); ++i) {
            const auto& block = blocked[i];
            
            json << "    {\n";
            json << "      \"ip\": \"" << block.ip.toString() << "\",\n";
            json << "      \"reason\": \"" << scada::to_string(block.reason) << "\",\n";
            json << "      \"violations\": " << block.violation_count << ",\n";
            json << "      \"permanent\": " << (block.permanent ? "true" : "false") << "\n";
            json << "    }";
            
            if (i < blocked.size() - 1) json << ",";
            json << "\n";
        }
        
        json << "  ]\n";
        json << "}";
        
        return json.str();
    }
    
    // POST /api/block
    std::string blockIP(const std::string& ip) {
        try {
            // Parse IP address
            unsigned int a, b, c, d;
            if (sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
                return "{\"error\": \"Invalid IP address format\"}";
            }
            
            net::ipv4 addr({static_cast<uint8_t>(a), static_cast<uint8_t>(b),
                           static_cast<uint8_t>(c), static_cast<uint8_t>(d)});
            
            watcher_.blockIP(addr, scada::AttackType::NONE);
            
            return "{\"success\": true, \"message\": \"IP blocked\"}";
        } catch (const std::exception& e) {
            return "{\"error\": \"" + JSON::escape(e.what()) + "\"}";
        }
    }
    
    // POST /api/unblock
    std::string unblockIP(const std::string& ip) {
        try {
            unsigned int a, b, c, d;
            if (sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
                return "{\"error\": \"Invalid IP address format\"}";
            }
            
            net::ipv4 addr({static_cast<uint8_t>(a), static_cast<uint8_t>(b),
                           static_cast<uint8_t>(c), static_cast<uint8_t>(d)});
            
            watcher_.unblockIP(addr);
            
            return "{\"success\": true, \"message\": \"IP unblocked\"}";
        } catch (const std::exception& e) {
            return "{\"error\": \"" + JSON::escape(e.what()) + "\"}";
        }
    }
    
    // GET /api/processor/stats (if processor is available)
    std::string getProcessorStats() {
        if (!processor_) {
            return "{\"error\": \"Processor not available\"}";
        }
        
        auto stats = processor_->getStats();
        
        std::ostringstream json;
        json << "{\n";
        json << "  \"packets_queued\": " << stats.packets_queued << ",\n";
        json << "  \"packets_processed\": " << stats.packets_processed << ",\n";
        json << "  \"packets_dropped_queue_full\": " << stats.packets_dropped_queue_full << "\n";
        json << "}";
        
        return json.str();
    }
};

// ============================================================================
// Simple HTTP Server
// ============================================================================
class SimpleHTTPServer {
private:
    GridWatcherAPI api_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    uint16_t port_;
    
public:
    SimpleHTTPServer(scada::GridWatcher& watcher,
                    processing::PacketProcessor* processor,
                    uint16_t port)
        : api_(watcher, processor), port_(port) {}
    
    ~SimpleHTTPServer() {
        stop();
    }
    
    void start() {
        if (running_.exchange(true)) return;
        
        server_thread_ = std::thread([this]() {
            // In production, implement proper HTTP server
            // This is just a placeholder
            std::cout << "[INFO] API server would start on port " << port_ << "\n";
            std::cout << "[INFO] Available endpoints:\n";
            std::cout << "  GET  /api/status\n";
            std::cout << "  GET  /api/statistics\n";
            std::cout << "  GET  /api/metrics\n";
            std::cout << "  GET  /api/blocks\n";
            std::cout << "  POST /api/block\n";
            std::cout << "  POST /api/unblock\n";
            std::cout << "  GET  /api/processor/stats\n";
            
            while (running_.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
    }
    
    void stop() {
        if (!running_.exchange(false)) return;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
};

// ============================================================================
// Prometheus Metrics Exporter
// ============================================================================
class PrometheusExporter {
private:
    scada::GridWatcher& watcher_;
    
public:
    explicit PrometheusExporter(scada::GridWatcher& watcher)
        : watcher_(watcher) {}
    
    std::string exportMetrics() {
        auto stats = watcher_.getStatistics();
        auto metrics = watcher_.getMetrics();
        
        std::ostringstream out;
        
        // Packet metrics
        out << "# HELP grid_watcher_packets_processed Total packets processed\n";
        out << "# TYPE grid_watcher_packets_processed counter\n";
        out << "grid_watcher_packets_processed " << stats.packets_processed << "\n\n";
        
        out << "# HELP grid_watcher_packets_allowed Total packets allowed\n";
        out << "# TYPE grid_watcher_packets_allowed counter\n";
        out << "grid_watcher_packets_allowed " << stats.packets_allowed << "\n\n";
        
        out << "# HELP grid_watcher_packets_dropped Total packets dropped\n";
        out << "# TYPE grid_watcher_packets_dropped counter\n";
        out << "grid_watcher_packets_dropped " << stats.packets_dropped << "\n\n";
        
        // Threat metrics
        out << "# HELP grid_watcher_threats_detected Total threats detected\n";
        out << "# TYPE grid_watcher_threats_detected counter\n";
        out << "grid_watcher_threats_detected " << stats.threats_detected << "\n\n";
        
        out << "# HELP grid_watcher_active_blocks Current active IP blocks\n";
        out << "# TYPE grid_watcher_active_blocks gauge\n";
        out << "grid_watcher_active_blocks " << stats.active_blocks << "\n\n";
        
        // Performance metrics
        out << "# HELP grid_watcher_latency_microseconds Packet processing latency\n";
        out << "# TYPE grid_watcher_latency_microseconds summary\n";
        out << "grid_watcher_latency_microseconds{quantile=\"0.0\"} " 
            << metrics.packet_latency.min_ns / 1000.0 << "\n";
        out << "grid_watcher_latency_microseconds{quantile=\"0.5\"} " 
            << metrics.packet_latency.avg_us << "\n";
        out << "grid_watcher_latency_microseconds{quantile=\"1.0\"} " 
            << metrics.packet_latency.max_ns / 1000.0 << "\n\n";
        
        // Throughput
        out << "# HELP grid_watcher_throughput_mbps Current throughput in Mbps\n";
        out << "# TYPE grid_watcher_throughput_mbps gauge\n";
        out << "grid_watcher_throughput_mbps " << metrics.throughput.mbps << "\n\n";
        
        return out.str();
    }
};

} // namespace gw::web
