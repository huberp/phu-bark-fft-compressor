# Aligned Allocators for PHU Plugins

This directory provides **aligned allocators** for STL containers (e.g., `std::vector`) to enable efficient SIMD operations (SSE, AVX, NEON).

## Usage

Replace `std::vector<T>` with `phu::memory::AlignedVector<T>` for performance-critical buffers:

```cpp
#include "memory/AlignedAllocator.h"

phu::memory::AlignedVector<float> fftBuffer;
fftBuffer.resize(2048); // Automatically 32-byte aligned
```

All standard `std::vector` operations (e.g., `resize()`, `operator[]`, iterators, `push_back`) work without modification.

## Custom Alignment

To use a different alignment (e.g., 16 bytes for SSE):

```cpp
phu::memory::AlignedVector<float, 16> sseBuffer;
sseBuffer.resize(1024); // 16-byte aligned
```

Or 64 bytes for cache-line alignment:

```cpp
phu::memory::AlignedVector<float, 64> cacheAlignedBuffer;
```

## How It Works

`AlignedVector<T, Alignment>` is a type alias for `std::vector<T, AlignedAllocator<T, Alignment>>`. The custom allocator replaces the default `std::allocator<T>` and uses platform-specific functions to guarantee alignment:

| Platform | Allocation function   | Deallocation function |
|----------|-----------------------|-----------------------|
| Windows  | `_aligned_malloc`     | `_aligned_free`       |
| POSIX    | `posix_memalign`      | `free`                |

The allocation size in bytes is always rounded up to a multiple of the alignment boundary to satisfy the requirements of aligned allocation functions.

## Default Alignment

The default alignment is **32 bytes**, which is the minimum requirement for AVX (256-bit SIMD) instructions. This covers:

- **SSE/SSE2/SSE4**: requires 16-byte alignment
- **AVX/AVX2**: requires 32-byte alignment
- **Cache lines**: typically 64 bytes (use `AlignedVector<T, 64>` if needed)

## Integration

The allocator is header-only and requires C++17. It has no external dependencies beyond the C++ standard library.
