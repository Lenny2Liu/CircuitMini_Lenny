#include "gate.h"

bool operator==(const Gate& lhs, const Gate& rhs) {
    return lhs.type == rhs.type &&
           lhs.input1 == rhs.input1 &&
           lhs.input2 == rhs.input2 &&
           lhs.output == rhs.output;
}
