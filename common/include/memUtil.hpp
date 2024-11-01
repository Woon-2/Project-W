#ifndef __AlignMem_HPP
#define __AlignMem_HPP

#include <memory_resource>
#include <algorithm>

#include <cstdlib>
#include <cassert>

template <class T, std::size_t Alignment>
class AlignAllocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    // template <class U>
    // struct rebind {
    //     using other = AlignAllocator<U, Alignment>;
    // };

    AlignAllocator() noexcept = default;

    template <class U>
    AlignAllocator(const AlignAllocator<U, Alignment>&) noexcept {}

    T* allocate(size_type n) {
        return static_cast<T*>( aligned_alloc(Alignment, n * sizeof(T)) );
    }

    void deallocate(T* p, size_type) noexcept {
        free(p);
    }
};

class AlignedResource : public std::pmr::memory_resource {
public:
    AlignedResource()
        : pUpstream_(std::pmr::get_default_resource()), alignment_(sizeof(std::max_align_t)) {}

    explicit AlignedResource(std::pmr::memory_resource* pUpstream)
        : pUpstream_(pUpstream), alignment_(sizeof(std::max_align_t)) {}

    AlignedResource(std::size_t alignment)
        : pUpstream_(std::pmr::get_default_resource()), alignment_(alignment) {}

    AlignedResource(std::pmr::memory_resource* pUpstream, std::size_t alignment)
        : pUpstream_(pUpstream), alignment_(alignment) {}

    std::pmr::memory_resource* upstream_resource() const {
        return pUpstream_;
    }

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        assert(pUpstream_ != nullptr);
        return pUpstream_->allocate( bytes, std::max(alignment, alignment_) );
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        assert(pUpstream_ != nullptr);
        pUpstream_->deallocate(p, bytes, std::max(alignment, alignment_));
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

    std::pmr::memory_resource* pUpstream_;
    std::size_t alignment_;
};

template <std::size_t N>
struct RawMemory {
	std::uint8_t data[N];
};

#endif // __AlignMem_HPP