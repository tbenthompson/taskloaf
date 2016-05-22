#pragma once

struct OwnershipTracker {
    static int& copies() { static int c; return c; }
    static int& moves() { static int m; return m; }
    static int& constructs() { static int c; return c; }
    static int& deletes() { static int d; return d; }
    static void reset() {
        copies() = 0; moves() = 0; constructs() = 0; deletes() = 0;
    }

    OwnershipTracker() { constructs()++; }
    OwnershipTracker(const OwnershipTracker& o) { *this = o; }
    OwnershipTracker(OwnershipTracker&& o) { *this = std::move(o); }
    OwnershipTracker& operator=(const OwnershipTracker&) { copies()++; return *this; }
    OwnershipTracker& operator=(OwnershipTracker&&) { moves()++; return *this; }
    ~OwnershipTracker() { deletes()++; }

    template <typename Archive>
    void serialize(Archive& ar) {
        int a = 0;
        ar(a);
    }
};
