@0xfb9f931d0da6e50e;

# Using these structures as the base classes for the objects used internally
# might be valuable because it would remove some of the serialization and all
# of the deserialization costs. This is likely only useful in a future C++
# implementation of taskloaf internals.
#
# Whether to include the shmem_ptr in the DistributedRef python object itself
# is a tough question. Should I allow it to be allow when a dref is created
# referring to a remote location that doesn't exist yet? Null pointers are
# generally an undesirable thing. But, having null shmem_ptrs is probably okay
# since the user can fall back to referring to the memory location using the
# (creator, _id) tuple.

struct ShmemPtr {
    start @0 :Int64;
    end @1 :Int64;
    blockIdx @2 :Int64;
}

struct GCRef {
    owner @0 :UInt32;
    id @1 :Int64;
    gen @2 :UInt32;
    deserialize @3 :Bool;
    ptr @4 :ShmemPtr;
}

struct DecRef {
    id @0 :Int64;
    gen @1 :Int64;
    nchildren @2 :Int64;
}

struct RemotePut {
    dref @0 :GCRef;
    val @1 :Arbitrary;
}

struct Arbitrary {
    refList @0 :List(GCRef);
    bytes @1 :Data;
}

# Unions are probably kind of slow, but...
# General decisions about protocol specifics might have some performance cost
# in both message construction cost and size of the message once serialized.
# But, both those effects are likely completely negligible compared to the cost
# of using Python.
struct Message {
    typeCode @0 :Int64;
    sourceAddr @1 :Int64;
    union {
        decRef @2 :DecRef;
        remotePut @3 :RemotePut;
        arbitrary @4 :Arbitrary;
    }
}
