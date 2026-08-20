// SPDX-License-Identifier: MIT
//
// Vendored from pyvista-algorithms (https://github.com/banesullivan/pyvista-algorithms)
//   src/cpp/parallel_radix_sort.h  — MIT licensed (Bane Sullivan and contributors).
//
// Support header (parallel radix sort) for the vendored point-dedup kernel pvaClean.h.
// Unmodified upstream source below this banner (local #includes renamed to the
// pva* vendored filenames).
//
// Lightweight parallel radix sort for uint64_t keys.
//
// Standard 8-pass LSD radix sort with 256-bin buckets, parallelised across
// threads on the histogram phase and the scatter phase. For int64 keys
// representing packed (smaller_endpoint, larger_endpoint) edge IDs this
// runs ~5-10× faster than std::sort or __gnu_parallel::sort because it's
// O(N · 8) instead of O(N log N) and the inner loop is branch-free.
//
// Optimizations vs naive LSD:
//  * Histograms and offsets buffers are hoisted out of the pass loop so
//    we don't pay an 8× allocator round-trip on every sort.
//  * Each pass first computes the histogram, then early-exits if the
//    byte is degenerate (all keys share the same byte value). For our
//    packed (hash<<32 | orig_id) keys with orig_id<<2^24 this skips
//    several high-byte passes for free.
//
// We also expose a "sort + unique" helper that fuses the dedup pass.

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include <omp.h>

namespace pvu {

inline void parallel_radix_sort_u64(uint64_t *data, size_t n) {
    if (n < 1024) {
        std::sort(data, data + n);
        return;
    }
    constexpr int RADIX = 256;
    constexpr int PASSES = 8;
    std::vector<uint64_t> tmp(n);
    int n_threads = omp_get_max_threads();

    // Per-thread histograms and per-thread scatter offsets, hoisted out
    // of the pass loop to avoid repeated allocations.
    std::vector<size_t> histos((size_t)n_threads * RADIX);
    std::vector<size_t> offsets((size_t)n_threads * RADIX);

    uint64_t *src = data;
    uint64_t *dst = tmp.data();
    int passes_done = 0;

    for (int pass = 0; pass < PASSES; ++pass) {
        std::fill(histos.begin(), histos.end(), 0);
        const int shift = pass * 8;

#pragma omp parallel
        {
            int tid = omp_get_thread_num();
            size_t *h = histos.data() + (size_t)tid * RADIX;
#pragma omp for schedule(static)
            for (long long i = 0; i < (long long)n; ++i) {
                int b = (src[i] >> shift) & 0xFF;
                ++h[b];
            }
        }

        // Prefix sum across threads.
        size_t global[RADIX];
        std::fill(global, global + RADIX, 0);
        for (int t = 0; t < n_threads; ++t) {
            for (int b = 0; b < RADIX; ++b) {
                global[b] += histos[(size_t)t * RADIX + b];
            }
        }

        // Degenerate-byte fast path: if every key has the same byte
        // value in this pass, the scatter is just a copy. Skip it; this
        // preserves src and saves a full O(N) write.
        int nonzero_buckets = 0;
        for (int b = 0; b < RADIX; ++b) {
            if (global[b] != 0) {
                if (++nonzero_buckets > 1)
                    break;
            }
        }
        if (nonzero_buckets <= 1) {
            // No reorder needed; src is already correctly partitioned.
            continue;
        }

        size_t cum[RADIX + 1];
        cum[0] = 0;
        for (int b = 0; b < RADIX; ++b)
            cum[b + 1] = cum[b] + global[b];

        // Per-thread base offsets so threads can scatter without atomics.
        for (int b = 0; b < RADIX; ++b) {
            size_t off = cum[b];
            for (int t = 0; t < n_threads; ++t) {
                offsets[(size_t)t * RADIX + b] = off;
                off += histos[(size_t)t * RADIX + b];
            }
        }

#pragma omp parallel
        {
            int tid = omp_get_thread_num();
            size_t *o = offsets.data() + (size_t)tid * RADIX;
#pragma omp for schedule(static)
            for (long long i = 0; i < (long long)n; ++i) {
                int b = (src[i] >> shift) & 0xFF;
                dst[o[b]++] = src[i];
            }
        }
        std::swap(src, dst);
        ++passes_done;
    }

    // After an even number of effective passes, data ends up back in
    // the original buffer. Odd → mirror back.
    if (src != data) {
        std::memcpy(data, src, n * sizeof(uint64_t));
    }
}

// Sort + unique in place. Returns the new logical size.
inline size_t parallel_sort_unique_u64(uint64_t *data, size_t n) {
    parallel_radix_sort_u64(data, n);
    if (n == 0)
        return 0;
    auto last = std::unique(data, data + n);
    return (size_t)(last - data);
}

} // namespace pvu
