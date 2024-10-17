#ifndef GATE_H
#define GATE_H

#include <string>

// Enumeration for gate types
enum GateType { JOIN, BUFFER, XOR, CONST_ZERO, CONST_ONE };

// Enumeration for wire states
enum State { ZERO = 0, ONE = 1, Z = 2, X = 3 };

// Gate structure
struct Gate {
    GateType type;
    int input1;
    int input2;
    int output;
};

// Equality operator for Gate
bool operator==(const Gate& lhs, const Gate& rhs);

#endif // GATE_H
