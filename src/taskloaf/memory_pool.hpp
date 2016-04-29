#pragma once

#include <boost/pool/pool.hpp>

namespace taskloaf {

template <size_t AllocSize>
struct Pools {
    static boost::pool<> pool;
};

template <size_t AllocSize>
boost::pool<> Pools<AllocSize>::pool(AllocSize);

template <class Tp>
struct PoolAllocator {
    typedef Tp value_type;

    PoolAllocator() = default;

    template <class T>
    PoolAllocator(const PoolAllocator<T>&) throw() {}

    Tp* allocate(std::size_t n) {
        tlassert(n == 1);
        Tp* ptr = static_cast<Tp*>(Pools<sizeof(Tp)>::pool.malloc());
        return ptr;
    }

    void deallocate(Tp* p, std::size_t) {
        Pools<sizeof(Tp)>::pool.free(p);
    }
};

}
