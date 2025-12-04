#pragma once

#include "../grid_watcher.hpp"
#include "../performance/lock_free.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <memory>
#include <barrier>

namespace gw::processing {

// ============================================================================
// Packet Processing Job
// ============================================================================
struct PacketJob {
	std::chrono::steady_clock::time_point received_at;
    std::vector<std::byte> data;
    net::ipv4 source_ip;
    net::ipv4 dest_ip;
    uint16_t source_port;
    uint16_t dest_port;
    
    // Result
    std::atomic<bool> processed{false};
    std::atomic<bool> allowed{true};
};

// ============================================================================
// Lock-Free Packet Queue (MPMC)
// ============================================================================
template<size_t Capacity>
class PacketQueue {
private:
    struct Slot {
        std::atomic<uint64_t> sequence{0};
        PacketJob job;
    };
    
    std::vector<Slot> slots_;
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> enqueue_pos_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> dequeue_pos_{0};
    
public:
    PacketQueue() : slots_(Capacity) {
        for (size_t i = 0; i < Capacity; ++i) {
            slots_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }
    
    bool enqueue(PacketJob&& job) noexcept {
        uint64_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        
        for (;;) {
            Slot& slot = slots_[pos % Capacity];
            uint64_t seq = slot.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            
            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, 
                    std::memory_order_relaxed)) {
                    slot.job = std::move(job);
                    slot.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false; // Full
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
    }
    
    bool dequeue(PacketJob& job) noexcept {
        uint64_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        
        for (;;) {
            Slot& slot = slots_[pos % Capacity];
            uint64_t seq = slot.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            
            if (diff == 0) {
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, 
                    std::memory_order_relaxed)) {
                    job = std::move(slot.job);
                    slot.sequence.store(pos + Capacity, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false; // Empty
            } else {
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
    }
};

// ============================================================================
// Multi-Threaded Packet Processor
// ============================================================================
class PacketProcessor {
private:
    scada::GridWatcher& watcher_;
    PacketQueue<32768> queue_; // 32K packet queue
    
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};
    size_t num_threads_;
    
    // Performance statistics
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> packets_queued_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> packets_processed_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> packets_dropped_queue_full_{0};
    
public:
    explicit PacketProcessor(scada::GridWatcher& watcher, 
                            size_t num_threads = std::thread::hardware_concurrency())
        : watcher_(watcher)
        , num_threads_(num_threads)
    {}
    
    ~PacketProcessor() {
        stop();
    }
    
    void start() {
        if (running_.exchange(true)) return;
        
        // Start worker threads
        for (size_t i = 0; i < num_threads_; ++i) {
            workers_.emplace_back([this, i]() {
                workerThread(i);
            });
            
            // Set thread affinity for better cache locality
            #ifdef __linux__
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i % std::thread::hardware_concurrency(), &cpuset);
            pthread_setaffinity_np(workers_.back().native_handle(), 
                                  sizeof(cpu_set_t), &cpuset);
            #endif
        }
    }
    
    void stop() {
        if (!running_.exchange(false)) return;
        
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }
    
    // Submit packet for processing (non-blocking)
    bool submitPacket(std::vector<std::byte>&& data,
                     const net::ipv4& src_ip,
                     const net::ipv4& dst_ip,
                     uint16_t src_port,
                     uint16_t dst_port) noexcept {
        PacketJob job;
        job.data = std::move(data);
        job.source_ip = src_ip;
        job.dest_ip = dst_ip;
        job.source_port = src_port;
        job.dest_port = dst_port;
        job.received_at = std::chrono::steady_clock::now();
        
        if (queue_.enqueue(std::move(job))) {
            packets_queued_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        
        packets_dropped_queue_full_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    
    struct Stats {
        uint64_t packets_queued;
        uint64_t packets_processed;
        uint64_t packets_dropped_queue_full;
        uint64_t queue_depth;
    };
    
    Stats getStats() const noexcept {
        return Stats{
            packets_queued_.load(std::memory_order_relaxed),
            packets_processed_.load(std::memory_order_relaxed),
            packets_dropped_queue_full_.load(std::memory_order_relaxed),
            0 // Queue depth would need additional tracking
        };
    }
    
private:
    void workerThread(size_t worker_id) {
        // Set thread name for debugging
        #ifdef __linux__
        std::string name = "gw-worker-" + std::to_string(worker_id);
        pthread_setname_np(pthread_self(), name.c_str());
        #endif
        
        PacketJob job;
        
        while (running_.load(std::memory_order_relaxed)) {
            if (queue_.dequeue(job)) {
                // Process packet
                bool allowed = watcher_.processPacket(
                    job.data,
                    job.source_ip,
                    job.dest_ip,
                    job.source_port,
                    job.dest_port
                );
                
                job.allowed.store(allowed, std::memory_order_release);
                job.processed.store(true, std::memory_order_release);
                
                packets_processed_.fetch_add(1, std::memory_order_relaxed);
            } else {
                // Queue empty, yield CPU
                std::this_thread::yield();
            }
        }
    }
};

// ============================================================================
// Batch Packet Processor (for even higher throughput)
// ============================================================================
class BatchPacketProcessor {
private:
    scada::GridWatcher& watcher_;
    static constexpr size_t BATCH_SIZE = 64;
    
    struct Batch {
        std::array<PacketJob, BATCH_SIZE> jobs;
        size_t count{0};
    };
    
    perf::LockFreeRingBuffer<Batch, 512> batch_queue_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};
    
public:
    explicit BatchPacketProcessor(scada::GridWatcher& watcher, size_t num_threads = 4)
        : watcher_(watcher)
    {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this]() { batchWorker(); });
        }
        running_.store(true);
    }
    
    ~BatchPacketProcessor() {
        running_.store(false);
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }
    
    bool submitBatch(std::vector<PacketJob>&& jobs) {
        if (jobs.size() > BATCH_SIZE) return false;
        
        Batch batch;
        batch.count = jobs.size();
        for (size_t i = 0; i < jobs.size(); ++i) {
            batch.jobs[i] = std::move(jobs[i]);
        }
        
        return batch_queue_.push(batch);
    }
    
private:
    void batchWorker() {
        Batch batch;
        
        while (running_.load(std::memory_order_relaxed)) {
            if (batch_queue_.pop(batch)) {
                // Process entire batch
                for (size_t i = 0; i < batch.count; ++i) {
                    auto& job = batch.jobs[i];
                    bool allowed = watcher_.processPacket(
                        job.data,
                        job.source_ip,
                        job.dest_ip,
                        job.source_port,
                        job.dest_port
                    );
                    job.allowed.store(allowed, std::memory_order_release);
                    job.processed.store(true, std::memory_order_release);
                }
            } else {
                std::this_thread::yield();
            }
        }
    }
};

} // namespace gw::processing
