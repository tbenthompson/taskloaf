#pragma once

namespace taskloaf {

struct Then;
struct Unwrap;
struct Async;
struct Ready;
struct WhenAll;

struct FutureVisitor {
    virtual void visit(Then* t) = 0;
    virtual void visit(Unwrap* t) = 0;
    virtual void visit(Async* t) = 0;
    virtual void visit(Ready* t) = 0;
    virtual void visit(WhenAll* t) = 0;
};

}
