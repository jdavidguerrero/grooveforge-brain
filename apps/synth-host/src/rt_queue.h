#pragma once
#include <cstddef>
#include <stdexcept>
#include <array>
#include <atomic>
#include <cstdint>
#include <type_traits>
namespace host {
/** @brief Fixed-capacity SPSC queue; the producer never overwrites unread data. */
template<class T,size_t N> class Spsc {
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
    std::array<T,N> data_{};
    alignas(64) std::atomic<uint32_t> read_{0};
    alignas(64) std::atomic<uint32_t> write_{0};
public:
    size_t free_slots() const noexcept {const auto w=write_.load(std::memory_order_relaxed),r=read_.load(std::memory_order_acquire);return (r+N-w-1)%N;}
    bool push(const T& v) noexcept {const auto w=write_.load(std::memory_order_relaxed);const auto next=(w+1)%N;if(next==read_.load(std::memory_order_acquire))return false;data_[w]=v;write_.store(uint32_t(next),std::memory_order_release);return true;}
    bool pop(T& v) noexcept {const auto r=read_.load(std::memory_order_relaxed);if(r==write_.load(std::memory_order_acquire))return false;v=data_[r];read_.store(uint32_t((r+1)%N),std::memory_order_release);return true;}
};
}
