#pragma once
#include <memory>
#include <vector>
#include <cstddef>
#include <limits>
#include <cstdlib> // For aligned allocation

namespace phu {
namespace memory {

/**
 * AlignedAllocator - A C++17 allocator for STL containers that ensures
 * memory alignment for SIMD operations (e.g., SSE, AVX, NEON).
 *
 * Default alignment is 32 bytes for AVX compatibility. Custom alignment
 * (e.g., 16 bytes for SSE, 64 bytes for cache lines) can be specified
 * via the Alignment template parameter.
 *
 * Platform fallbacks:
 *   - Windows:      _aligned_malloc / _aligned_free
 *   - POSIX:        posix_memalign / free
 *
 * @tparam T         The type of elements in the container.
 * @tparam Alignment The alignment boundary in bytes (default: 32 for AVX).
 */
template <typename T, std::size_t Alignment = 32>
class AlignedAllocator {
public:
    // Minimal C++17 allocator requirements
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    AlignedAllocator() noexcept = default;

    template <typename U>
    constexpr AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    // rebind is required when the allocator has multiple template parameters,
    // so that std::allocator_traits can construct a rebound allocator type.
    template <typename U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };    [[nodiscard]] T* allocate(size_type n) {
        if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
            throw std::bad_alloc();
        }

        // Round up allocation size to a multiple of Alignment as required by
        // std::aligned_alloc (and as good practice for aligned allocations).
        const size_type bytes = ((n * sizeof(T) + Alignment - 1) / Alignment) * Alignment;

        void* ptr = nullptr;

#if defined(_WIN32)
        ptr = _aligned_malloc(bytes, Alignment);
#else
        if (posix_memalign(&ptr, Alignment, bytes) != 0) {
            ptr = nullptr;
        }
#endif

        if (!ptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, size_type) noexcept {
#if defined(_WIN32)
        _aligned_free(p);
#else
        free(p);
#endif
    }

    template <typename U>
    constexpr bool operator==(const AlignedAllocator<U, Alignment>&) const noexcept {
        return true;
    }

    template <typename U>
    constexpr bool operator!=(const AlignedAllocator<U, Alignment>&) const noexcept {
        return false;
    }
};

/**
 * AlignedVector - A std::vector with aligned memory allocation.
 * Drop-in replacement for std::vector with guaranteed alignment.
 *
 * Example usage:
 * @code
 *   phu::memory::AlignedVector<float> fftBuffer;
 *   fftBuffer.resize(2048); // Automatically 32-byte aligned
 * @endcode
 *
 * @tparam T         The type of elements in the vector.
 * @tparam Alignment The alignment boundary in bytes (default: 32 for AVX).
 */
template <typename T, std::size_t Alignment = 32>
using AlignedVector = std::vector<T, AlignedAllocator<T, Alignment>>;

} // namespace memory
} // namespace phu
