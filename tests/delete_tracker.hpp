#pragma once

struct DeleteTracker {
    static int& get_deletes() {
        static int deletes;
        return deletes; 
    }

    ~DeleteTracker() {
        get_deletes()++;
    }

    template <typename Archive>
    void serialize(Archive& ar) {
        int a = 0;
        ar(a);
    }
};
