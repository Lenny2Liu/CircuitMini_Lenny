#include "utils.h"

int getNextVarID() {
    static int varID = 0;
    return ++varID;
}
