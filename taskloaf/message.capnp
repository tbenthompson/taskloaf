@0xfb9f931d0da6e50e;

# Using these structures as the base classes for the objects used internally
# might be valuable because it would remove some of the serialization and all
# of the deserialization costs. This is likely only useful in a future C++
# implementation of taskloaf internals.

struct DecRef {
    id @0 :Int64;
    gen @1 :Int64;
    nchildren @2 :Int64;
}

struct ShmemPtr {
    start @0 :Int64;
    end @1 :Int64;
    blockIdx @2 :Int64;
}

struct Ref {
    owner @0 :UInt32;
    id @1 :Int64;
    gen @2 :UInt32;
    refList @3 :List(Ref);
}

struct ObjectRef {
    ref @0 :Ref;
    deserialize @1 :Bool;
    ptr @2 :ShmemPtr;
}

struct Object {
    objref @0 :ObjectRef;
    val @1 :Data;
}

struct Promise {
    ref @0 :Ref; 
    runningOn @1 :UInt32;
}

struct Task {
    promise @0 :Promise; 
    objrefs @1 :List(ObjectRef);
}

# Unions are probably kind of slow, but...
# General decisions about protocol specifics might have some performance cost
# in both message construction cost and size of the message once serialized.
# But, both those effects are likely completely negligible compared to the cost
# of using Python.
#
# TODO: A multi-part message design would allow getting rid of the union and
# using a separate message part for the typeCode and sourceAddr and then a
# user-defined message for the remainder, to be decoded by the deserializer
# specified by typeCode
# TODO: This seems especially cool in the context of a system where Capnp
# objects underlie most of the system so that no serialization needs to happen
# for a lot of messaging requirements.
# TODO: is there any kind of capnp <--> zmq interface? A buffer interface?
struct Message {
    typeCode @0 :Int64;
    sourceAddr @1 :Int64;
    union {
        decRef @2 :DecRef;
        object @3 :Object;
        task @4 :Task;
        arbitrary @5 :Data;
    }
}
