#ifndef GATE_H
#define GATE_H

#include <string>


enum GateType { JOIN, BUFFER, XOR, CONST_ZERO, CONST_ONE };


enum State { ZERO = 0, ONE = 1, Z = 2, X = 3 };


struct Gate {
    GateType type;
    int input1;
    int input2;
    int output;
};

bool operator==(const Gate& lhs, const Gate& rhs);

#endif
