#pragma once

#include <esp_heap_caps.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <vector>

bool TextGlyphStorageUsesPsram();

template <typename T>
class TextGlyphAllocator {
public:
    using value_type = T;
    using is_always_equal = std::true_type;

    TextGlyphAllocator() noexcept = default;

    template <typename U>
    TextGlyphAllocator(const TextGlyphAllocator<U>&) noexcept {}

    T* allocate(size_t count) {
        if (count == 0) {
            return nullptr;
        }
        if (count > std::numeric_limits<size_t>::max() / sizeof(T)) {
            std::abort();
        }
        uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
        if (TextGlyphStorageUsesPsram()) {
            caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
        }
        auto ptr = static_cast<T*>(heap_caps_malloc(count * sizeof(T), caps));
        if (ptr == nullptr) {
            std::abort();
        }
        return ptr;
    }

    void deallocate(T* ptr, size_t) noexcept { heap_caps_free(ptr); }
};

template <typename T, typename U>
bool operator==(const TextGlyphAllocator<T>&, const TextGlyphAllocator<U>&) {
    return true;
}

template <typename T, typename U>
bool operator!=(const TextGlyphAllocator<T>&, const TextGlyphAllocator<U>&) {
    return false;
}

template <typename T>
using TextGlyphVector = std::vector<T, TextGlyphAllocator<T>>;

struct TextGlyph {
    uint32_t codepoint = 0;
    uint32_t adv_w = 0;
    uint16_t box_w = 0;
    uint16_t box_h = 0;
    int16_t ofs_x = 0;
    int16_t ofs_y = 0;
    TextGlyphVector<uint8_t> bitmap;
};
