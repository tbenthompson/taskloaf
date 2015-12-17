#pragma once
#include "future_visitor.hpp"
#include "future.hpp"

#include <iostream>

namespace taskloaf {

struct PrintFuture: public FutureVisitor {
    int offset = 0;

    virtual void visit(Then* t) override {
        std::cout << "Then " << t->fnc_name << std::endl;
        offset += 1;
        t->fut->accept(this);
        offset -= 1;
    }

    virtual void visit(Unwrap* t) override {
        (void)t;
        std::cout << "Unwrap" << std::endl;
        offset += 1;
        t->fut->accept(this);
        offset -= 1
    }

    virtual void visit(Async* t) override {
        (void)t;
        std::cout << "Async" << std::endl;
    }

    virtual void visit(Ready* t) override {
        (void)t;
        std::cout << "Ready" << std::endl;
    }

    virtual void visit(WhenAll* t) override {
        (void)t;
        std::cout << "WhenAll" << std::endl;
        for (auto& d: t->data) {
            offset += 1;
            d->accept(this);
            offset -= 1;
        }
    }
};

template <typename T>
void print(Future<T> f) {
    PrintFuture printer; 
    f.data->accept(&printer);
}

}
