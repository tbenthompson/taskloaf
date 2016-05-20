#pragma once
#include <ucontext.h>
#include <thread>
#include <csetjmp>

namespace taskloaf {

struct Stack {
    const static size_t size = 2*SIGSTKSZ;
    char data[size];
};

struct Thread {
    typedef std::unique_ptr<Thread> Ptr;
    std::function<void()> main;
    std::unique_ptr<Stack> stack;
    ucontext_t ctx;
    std::jmp_buf jmpbuf;
    std::jmp_buf prev;
    bool launched = false;

    Thread(Thread&&) = delete;
    Thread(const Thread&) = delete;

    Thread(std::function<void()> main):
        main(main),
        stack(std::make_unique<Stack>())
    {
        getcontext(&ctx);
        ctx.uc_link = nullptr;
        ctx.uc_stack.ss_sp = stack->data;
        ctx.uc_stack.ss_size = stack->size;

        auto this_ptr = uint64_t(reinterpret_cast<uintptr_t>(this));
        auto fnc = reinterpret_cast<void(*)()>(call_member_starter);
        makecontext(&ctx, fnc, 2, int(this_ptr), int(this_ptr >> 32));
    }

    static void call_member_starter(unsigned int lo, unsigned int hi) {
        uintptr_t this_ptr = lo | (uint64_t(hi) << 32);
        reinterpret_cast<Thread*>(this_ptr)->starter();
    }

    void starter() {
        main(); 
        std::longjmp(prev, 1);
    }

    void switch_in() {
        if (setjmp(prev) == 0) {
            if (launched) {
                longjmp(jmpbuf, 1);
            } else {
                launched = true;
                setcontext(&ctx);
            }
        }
    }

    void switch_out() {
        if (setjmp(jmpbuf) == 0) {
            std::longjmp(prev, 1);
        }
    }
};

}
