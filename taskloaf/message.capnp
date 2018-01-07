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
    needsDeserialize @0 :Bool;
    start @1 :Int64;
    end @2 :Int64;
}

struct DistributedRef {
    owner @0 :UInt32;
    creator @1 :UInt32;
    id @2 :Int64;
    gen @3 :UInt32;
    shmemPtr @4 :ShmemPtr;
}

struct DecRef {
    creator @0 :Int64;
    id @1 :Int64;
    gen @2 :Int64;
    nchildren @3 :Int64;
}

struct RemotePut {
    dref @0 :DistributedRef;
    val @1 :Data;
}

struct Arbitrary {
    bytes @0 :Data;
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
        drefList @2 :List(DistributedRef);
        remotePut @3 :RemotePut;
        decRef @4 :DecRef;
        arbitrary @5 :Arbitrary;
    }
}
