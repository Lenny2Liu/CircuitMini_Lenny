#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "gate.h"
#include "utils.h"


struct Circuit {
    int numInputs;
    int numOutputs;
    std::vector<Gate> gates;
    std::vector<int> inputWires;
    std::vector<int> outputWires;
};


int getGateCount(const Circuit& circuit);
Circuit readCircuit(const std::string& filename);
std::vector<Circuit> partitionCircuit(const Circuit& circuit, int windowSize);
int getJOINCount(const Circuit& circuit);
extern std::unordered_map<std::string, GateType> gateMap;

#endif
