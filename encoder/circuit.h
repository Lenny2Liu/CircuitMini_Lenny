#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "gate.h"
#include "utils.h"

// Circuit structure
struct Circuit {
    int numInputs;
    int numOutputs;
    std::vector<Gate> gates;
    std::vector<int> inputWires;
    std::vector<int> outputWires;
};

// Function declarations
Circuit readCircuit(const std::string& filename);
std::vector<int> topologicalSort(const Circuit& circuit);
std::vector<Circuit> partitionCircuit(const Circuit& circuit, int windowSize);

extern std::unordered_map<std::string, GateType> gateMap;

#endif // CIRCUIT_H
