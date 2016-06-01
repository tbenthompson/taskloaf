#pragma once

#define SOME_ENUM(DO) \
    DO(Gossip) \
    DO(Shutdown) \
    DO(Steal) \
    DO(StealResponse) \
    DO(IncRef) \
    DO(DecRef) \
    DO(Fulfill) \
    DO(TriggerLocs) \
    DO(GetTriggers) \
    DO(RecvTriggers) \
    DO(AddTrigger) \
    DO(DeleteInfo) \
    DO(SendOwnership) \
    DO(InitiateTransfer) \
    DO(AssignedTask)

#define MAKE_ENUM(VAR) VAR,
enum class Protocol {
    SOME_ENUM(MAKE_ENUM)
};

#define MAKE_STRINGS(VAR) #VAR,
const char* const ProtocolNames[] = {
    SOME_ENUM(MAKE_STRINGS)
};
