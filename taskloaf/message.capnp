@0xfb9f931d0da6e50e;

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

struct Ref {
    union {
        memoryRef @0 :MemoryRef;
        taskRef @1 :TaskRef;
    }
}

struct Message {
    chunks @0 :List(Ref);
}
