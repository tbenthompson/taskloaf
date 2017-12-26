@0xfb9f931d0da6e50e;

struct DecRef {
    creator @0 :UInt32;
    id @1 :Int64;
    gen @2 :UInt32;
    nchildren @3 :UInt32;
}

struct Memory {
    owner @0 :UInt32;
    creator @1 :UInt32;
    id @2 :Int64;
    gen @3 :UInt32;
    shmem_ptr @4 :UInt64;
}

struct Message {
    chunks @0 :List(Memory);
}
