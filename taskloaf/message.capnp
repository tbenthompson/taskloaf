@0xfb9f931d0da6e50e;

# Using these structures as the base classes for the objects used internally
# might be valuable because it would remove some of the serialization and all
# of the deserialization costs. This is likely only useful in a future C++ 
# implementation of taskloaf internals.

struct DecRef {
    creator @0 :UInt32;
    id @1 :Int64;
    gen @2 :UInt32;
    nchildren @3 :UInt32;
}


struct DistributedRef {
    owner @0 :UInt32;
    creator @1 :UInt32;
    id @2 :Int64;
    gen @3 :UInt32;
}

struct TaskRef {
    dref @0 :DistributedRef;
}

struct MemoryRef {
    dref @0 :DistributedRef;
    shmemPtr @1 :UInt64;
    valIncluded @2 :Bool;
    val @3 :Data;
}

# This decision to use a union and a List to make the message very general
# might have some performance cost in both message construction cost and
# size of the message once serialized. But, both those effects are likely
# completely negligible compared to the cost of using Python.

struct Ref {
    union {
        memoryRef @0 :MemoryRef;
        taskRef @1 :TaskRef;
    }
}

struct Message {
    chunks @0 :List(Ref);
}
