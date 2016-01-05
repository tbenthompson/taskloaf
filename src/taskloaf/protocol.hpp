#pragma once

enum class Protocol {
    Gossip,
    Shutdown,
    Steal,
    StealResponse,
    IncRef,
    DecRef,
    NewIVar,
    Fulfill,
    TriggerLocs,
    GetTriggers,
    RecvTriggers,
    AddTrigger,
    DeleteTriggers,
    DeleteVals
};
