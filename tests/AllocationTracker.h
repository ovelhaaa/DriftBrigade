#pragma once
#include <atomic>
#include <cstdlib>
#include <new>
// Counts the callback thread only; JUCE's GUI/timer threads allocate independently.
// Each executable has one translation unit; catches ordinary C++ heap use.
inline thread_local std::atomic<std::size_t> allocations {0};
void* operator new(std::size_t n) { allocations.fetch_add(1, std::memory_order_relaxed); if (auto* p=std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
