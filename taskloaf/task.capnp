@0x8c96edc6897b9e10;

struct Ref {
    owner @0 :Int64;
    creator @1 :Int64;
    id @2 :Int64;
    gen @3 :Int64;
}

struct MaybeBytes {
    wasbytes @0 :Bool;
    bytes @1 :Data;
}

struct Task {
    ref @0 :Ref;
    f @1 :MaybeBytes;
    args @2 :MaybeBytes;
}

struct SetResult {
    ref @0 :Ref;
    v @1 :MaybeBytes;
}

struct Await {
    ref @0 :Ref;
    reqaddr @1 :UInt32;
}

struct DecRef {
    creator @0 :Int64;
    id @1 :Int64;
    gen @2 :Int64;
    nchildren @3 :Int64;
}

struct Message {
    union {
        task @0 :Task;
        setresult @1 :SetResult;
        await @2 :Await;
        decref @3 :DecRef;
    }
}
