#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <array>
#include <memory>
#include <mutex>
#include <vector>

// =============================================================================
//  Instance link (0.1.1): every EQ Anatomy instance in the same process publishes
//  its finished output (Mid / Side) to a ring that the other instances' analyzers
//  can read. The audio thread only writes its own ring (no locks); the registry
//  list is changed on the message thread. Instances in another process (or in
//  another plug-in format, which is another binary) cannot be seen.
// =============================================================================
struct LinkTap
{
    static constexpr int N = 16384;                     // power of two
    std::array<float, N> m {}, s {};
    std::atomic<juce::uint64> w { 0 };                  // samples written so far
    std::atomic<double> fs { 48000.0 };
    int uid = 0;
    juce::String name() const { const juce::SpinLock::ScopedLockType l (lock); return nm; }
    void setName (const juce::String& n) { const juce::SpinLock::ScopedLockType l (lock); nm = n; }

    // audio thread
    void push (const float* L, const float* R, int n) noexcept
    {
        auto p = w.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i, ++p)
        {
            const float l = L[i], r = R != nullptr ? R[i] : l;
            const size_t k = (size_t) (p & (juce::uint64) (N - 1));
            m[k] = 0.5f * (l + r); s[k] = 0.5f * (l - r);
        }
        w.store (p, std::memory_order_release);
    }
    // UI thread: the latest `count` samples (the writer may tear a few at the edge; fine for a display)
    void read (float* dm, float* ds, int count) const noexcept
    {
        const auto end = w.load (std::memory_order_acquire);
        for (int i = 0; i < count; ++i)
        {
            const auto p = end - (juce::uint64) (count - i);
            const size_t k = (size_t) (p & (juce::uint64) (N - 1));
            const bool ok = end >= (juce::uint64) (count - i);
            dm[i] = ok ? m[k] : 0.0f; ds[i] = ok ? s[k] : 0.0f;
        }
    }
private:
    mutable juce::SpinLock lock; juce::String nm;
};

struct LinkRegistry
{
    static std::shared_ptr<LinkTap> add()
    {
        auto& r = get(); const std::lock_guard<std::mutex> l (r.mx);
        auto t = std::make_shared<LinkTap>(); t->uid = ++r.next;
        t->setName ("EQ Anatomy #" + juce::String (t->uid));
        r.taps.push_back (t); return t;
    }
    static void remove (const std::shared_ptr<LinkTap>& t)
    {
        auto& r = get(); const std::lock_guard<std::mutex> l (r.mx);
        r.taps.erase (std::remove (r.taps.begin(), r.taps.end(), t), r.taps.end());
    }
    static std::vector<std::shared_ptr<LinkTap>> list()
    {
        auto& r = get(); const std::lock_guard<std::mutex> l (r.mx); return r.taps;
    }
private:
    std::mutex mx; std::vector<std::shared_ptr<LinkTap>> taps; int next = 0;
    static LinkRegistry& get() { static LinkRegistry r; return r; }
};
