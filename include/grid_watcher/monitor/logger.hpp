#pragma once

#define NOGDI

#include "../scada/types.hpp"
#include "../performance/lock_free.hpp"
#include <thread>
#include <atomic>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <optional>

namespace gw::monitor {

    // ========================================================================
    // Log Entry Structure
    // ========================================================================
    struct LogEntry {
        using TimePoint = std::chrono::system_clock::time_point;
        
        enum class Level : uint8_t {
            TRACE = 0,
            DEBUG = 1,
            INFO = 2,
            WARNING = 3,
            ERROR = 4,
            CRITICAL = 5
        };
        
        TimePoint timestamp;
        Level level;
        std::string source;
        std::string message;
        std::optional<scada::ThreatAlert> threat;
        
        LogEntry() noexcept 
            : timestamp(std::chrono::system_clock::now())
            , level(Level::INFO) {}
        
        LogEntry(Level lvl, std::string src, std::string msg) noexcept
            : timestamp(std::chrono::system_clock::now())
            , level(lvl)
            , source(std::move(src))
            , message(std::move(msg)) {}
        
        std::string toString() const {
            std::ostringstream oss;
            auto time_t = std::chrono::system_clock::to_time_t(timestamp);
            oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            oss << " [" << levelToString(level) << "] [" << source << "] " << message;
            
            if (threat) {
                oss << " | Attack: " << scada::to_string(threat->attack_type);
                oss << " | Severity: " << scada::to_string(threat->severity);
                oss << " | Source: " << threat->source_ip.toString();
                oss << " | Confidence: " << std::fixed << std::setprecision(2) 
                    << (threat->confidence_score * 100) << "%";
            }
            
            return oss.str();
        }
        
        static const char* levelToString(Level lvl) noexcept {
            switch (lvl) {
                case Level::TRACE: return "TRACE";
                case Level::DEBUG: return "DEBUG";
                case Level::INFO: return "INFO";
                case Level::WARNING: return "WARNING";
                case Level::ERROR: return "ERROR";
                case Level::CRITICAL: return "CRITICAL";
                default: return "UNKNOWN";
            }
        }
    };

    // ========================================================================
    // High-Performance Lock-Free Logger
    // ========================================================================
    class Logger {
    private:
        perf::LockFreeRingBuffer<LogEntry, 8192> log_queue_;
        std::thread writer_thread_;
        std::atomic<bool> running_{false};
        std::ofstream log_file_;
        std::string filename_;
        LogEntry::Level min_level_{LogEntry::Level::INFO};
        bool console_output_{true};
        
        // Statistics (cache-aligned)
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> logs_written_{0};
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> logs_dropped_{0};
        
    public:
        explicit Logger(const std::string& filename, 
                       LogEntry::Level min_level = LogEntry::Level::INFO,
                       bool console_output = true) 
            : filename_(filename)
            , min_level_(min_level)
            , console_output_(console_output)
        {
            log_file_.open(filename_, std::ios::app);
            if (!log_file_.is_open()) {
                throw std::runtime_error("Failed to open log file: " + filename_);
            }
        }
        
        ~Logger() {
            stop();
        }
        
        // Prevent copying
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        
        void start() {
            if (running_.exchange(true)) return;
            writer_thread_ = std::thread([this]() { writerLoop(); });
        }
        
        void stop() {
            if (!running_.exchange(false)) return;
            
            if (writer_thread_.joinable()) {
                writer_thread_.join();
            }
            
            // Flush remaining logs
            LogEntry entry;
            while (log_queue_.pop(entry)) {
                writeLog(entry);
            }
            
            log_file_.close();
        }
        
        // Generic log method
        void log(LogEntry::Level level, const std::string& source, 
                const std::string& message, 
                std::optional<scada::ThreatAlert> threat = std::nullopt) noexcept {
            if (level < min_level_) return;
            
            LogEntry entry(level, source, message);
            entry.threat = threat;
            
            if (!log_queue_.push(entry)) {
                logs_dropped_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        
        // Convenience methods
        void trace(const std::string& source, const std::string& message) noexcept {
            log(LogEntry::Level::TRACE, source, message);
        }
        
        void debug(const std::string& source, const std::string& message) noexcept {
            log(LogEntry::Level::DEBUG, source, message);
        }
        
        void info(const std::string& source, const std::string& message) noexcept {
            log(LogEntry::Level::INFO, source, message);
        }
        
        void warning(const std::string& source, const std::string& message) noexcept {
            log(LogEntry::Level::WARNING, source, message);
        }
        
        void error(const std::string& source, const std::string& message) noexcept {
            log(LogEntry::Level::ERROR, source, message);
        }
        
        void critical(const std::string& source, const std::string& message, 
                     const scada::ThreatAlert& threat) noexcept {
            log(LogEntry::Level::CRITICAL, source, message, threat);
        }
        
        // Configuration
        void setMinLevel(LogEntry::Level level) noexcept {
            min_level_ = level;
        }
        
        void setConsoleOutput(bool enable) noexcept {
            console_output_ = enable;
        }
        
        // Statistics
        uint64_t getLogsWritten() const noexcept {
            return logs_written_.load(std::memory_order_relaxed);
        }
        
        uint64_t getLogsDropped() const noexcept {
            return logs_dropped_.load(std::memory_order_relaxed);
        }
        
    private:
        void writerLoop() {
            LogEntry entry;
            
            while (running_.load(std::memory_order_relaxed)) {
                if (log_queue_.pop(entry)) {
                    writeLog(entry);
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }
        
        void writeLog(const LogEntry& entry) {
            auto log_str = entry.toString();
            
            // Write to file
            log_file_ << log_str << std::endl;
            log_file_.flush();
            
            // Write to console if enabled
            if (console_output_) {
                std::cout << log_str << std::endl;
            }
            
            logs_written_.fetch_add(1, std::memory_order_relaxed);
        }
    };

} // namespace gw::monitor