#ifndef QBFENCODER_H
#define QBFENCODER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "circuit.h"
#include "utils.h"


struct WireVars {
    int v1;
    int v2;
};


int addExactlyOneConstraint(const std::vector<int>& vars, std::vector<std::string>& clauses);
int addConstGateCompatibilityConstraints(
    int funcVar,
    GateType funcType,
    int gateOutputVar_v1, int gateOutputVar_v2,
    std::vector<std::string>& clauses
);
int addBUFFERCompatibilityConstraints(
    int funcVar, 
    int selVar1, int selVar2,  
    int controlVar_v1, int controlVar_v2, 
    int dataVar_v1, int dataVar_v2, 
    int gateOutputVar_v1, int gateOutputVar_v2, 
    std::vector<std::string>& clauses
);
int addJOINCompatibilityConstraints(
    int funcVar, 
    int selVar1, int selVar2, 
    int inputVar1_v1, int inputVar1_v2, 
    int inputVar2_v1, int inputVar2_v2, 
    int gateOutputVar_v1, int gateOutputVar_v2, 
    std::vector<std::string>& clauses
);
int addXORCompatibilityConstraints(
    int funcVar, 
    int selVar1, int selVar2, 
    int inputVar1_v1, int inputVar1_v2, 
    int inputVar2_v1, int inputVar2_v2, 
    int gateOutputVar_v1, int gateOutputVar_v2, 
    std::vector<std::string>& clauses
);
void encodeSubcircuitAsQBF(const Circuit& subcircuit, const int numGates, const std::string& filename);

#endif 
