#pragma once

enum class Protocol {
    Gossip,
    Shutdown,
    Steal,
    StealResponse,
    IncRef,
    DecRef,
    Fulfill,
    TriggerLocs,
    GetTriggers,
    RecvTriggers,
    AddTrigger,
    DeleteInfo,
    SendOwnership,
    InitiateTransfer
};
