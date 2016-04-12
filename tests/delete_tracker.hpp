#pragma once

struct DeleteTracker {
    static int deletes;

    ~DeleteTracker() {
        deletes++;
    }

    template <typename Archive>
    void serialize(Archive& ar) {
        int a = 0;
        ar(a);
    }
};
