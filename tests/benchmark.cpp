#include "taskloaf.hpp"
#include "taskloaf/timing.hpp"


using namespace taskloaf;

void run_many_times(int n, void (*fnc)()) {
    Timer time;
    time.start();
    for (int i = 0; i < n; i++) {
        fnc();
    }
    time.stop();
    std::cout << time.get_time() << "us" << std::endl;
}

#define BENCH(n, fnc_name)\
    std::cout << "Running " << n << " iterations of " #fnc_name << " took ";\
    run_many_times(n, &fnc_name);

void bench_closure() {
    TaskT t(
        [] (std::vector<Data>&) {}, {make_data(10)}
    );
}
void bench_typed_closure() {
    TaskT t([] () {});
}

void bench_make_data() {
    auto d = make_data(10);
}

template <typename T>
struct UniqueTypedData {
    T data;

    UniqueTypedData(T&& v): data(std::move(v)) {}
};

void bench_typed_data() {
    UniqueTypedData<int> d(10); 
}

#include <stack>

template <typename T>
struct MemoryPool {
    std::vector<uint8_t> data;
    std::vector<size_t> next;

    template <typename... Args>
    T* alloc(Args&&... args) {
        if (next.size() == 0) {
            auto orig_size = data.size();
            data.resize(orig_size + sizeof(T));
            return new (&data[orig_size]) T(std::forward<Args>(args)...);
        } else {
            auto out_idx = next.back();
            next.pop_back();
            return new (&data[out_idx]) T(std::forward<Args>(args)...);
        }
    }

    void free(T* v) {
        v->~T();
        auto offset = reinterpret_cast<uint8_t*>(v) - &data[0];
        next.push_back(offset);
    }
};

template <typename T>
struct TypedDataPtr {
    T* data;

    static auto& get_pool() {
        static MemoryPool<T> pool;
        return pool;
    }

    TypedDataPtr(T&& v) {
        data = get_pool().alloc();        
        *data = std::move(v);
    }

    ~TypedDataPtr() {
        get_pool().free(data); 
    }
};

void bench_typed_data_ptr() {
    {
        TypedDataPtr<int> d(10);
    } 
    {
        TypedDataPtr<int> d(10);
    }
}

void bench_allocation() {
    auto* ptr = new int;
    delete ptr;
}

void bench_100mb_allocation() {
    int n = 100000000 / 4;
    auto* ptr = new int[n];
    for (int i = 0; i < n; i++) {
        auto* ptr2 = ptr + i;
        *ptr2 = i;
    }
    delete[] ptr;
}

int main() {
    BENCH(1000000, bench_closure);
    BENCH(1000000, bench_typed_closure);
    BENCH(1000000, bench_make_data);
    BENCH(1000000, bench_typed_data);
    BENCH(1000000, bench_typed_data_ptr);
    BENCH(1000000, bench_allocation);
    BENCH(1, bench_100mb_allocation);
}

// struct TaskBase {
//     typedef std::unique_ptr<TaskBase> Ptr;
//     virtual void operator()() = 0;
//     virtual Closure<void()> to_serializable() = 0;
// };
// 
// template <typename F>
// struct LambdaTask {
//     F f;
//     void operator()() {
//         f();
//     }
//     void to_serializable() {
//         return Closure<void()>{f, {}};
//     }
// };
// 
// struct Task {
//     std::unique_ptr<TaskBase> ptr = nullptr;
//     Closure<void()> serializable;
// 
//     void promote() {
//         if (ptr == nullptr) {
//             return;
//         }
//         serializable = ptr->to_serializable();
//         ptr = nullptr;
//     }
// 
//     void operator()() {
//         if (ptr != nullptr) {
//             ptr->operator()();
//         } else {
//             serializable();
//         }
//     }
// 
//     void save(cereal::BinaryOutputArchive& ar) const {
//         const_cast<Task*>(this)->promote();
//         ar(serializable);
//     }
// 
//     void load(cereal::BinaryInputArchive& ar) {
//         ptr = nullptr; 
//         ar(serializable);
//     }
// };
