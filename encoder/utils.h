#ifndef UTILITY_H
#define UTILITY_H

#include <functional>
#include <utility>

// Helper struct for pair hashing, used in unordered_map
struct pair_hash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ h2;
    }
};

// Function to get the next unique variable ID
int getNextVarID();

#endif // UTILITY_H
