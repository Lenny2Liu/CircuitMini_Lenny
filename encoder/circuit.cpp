#include "circuit.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <queue>
#include <algorithm>

using namespace std;

unordered_map<string, GateType> gateMap = {
    {"JOIN", JOIN},
    {"BUFFER", BUFFER},
    {"XOR", XOR},
    {"CONST_ZERO", CONST_ZERO},
    {"CONST_ONE", CONST_ONE}
};

Circuit readCircuit(const string& filename) {
    ifstream infile(filename);
    if (!infile) {
        cerr << "Cannot open the file: " << filename << endl;
        exit(1);
    }

    Circuit circuit;
    string line;

    getline(infile, line);
    istringstream iss1(line);
    int numGates, numWires;
    iss1 >> numGates >> numWires;

    getline(infile, line);
    istringstream iss2(line);
    int niv;
    vector<int> numInputBitsPerValue;
    int totalInputWires = 0;
    iss2 >> niv;
    for (int i = 0; i < niv; ++i) {
        int ni;
        iss2 >> ni;
        numInputBitsPerValue.push_back(ni);
        totalInputWires += ni;
    }

    getline(infile, line);
    istringstream iss3(line);
    int nov;
    vector<int> numOutputBitsPerValue;
    int totalOutputWires = 0;
    iss3 >> nov;
    for (int i = 0; i < nov; ++i) {
        int no;
        iss3 >> no;
        numOutputBitsPerValue.push_back(no);
        totalOutputWires += no;
    }

    circuit.numInputs = totalInputWires;
    circuit.numOutputs = totalOutputWires;

    // Input wires are numbered from 0 to totalInputWires - 1
    for (int i = 0; i < totalInputWires; ++i) {
        circuit.inputWires.push_back(i);
    }

    // Read gates
    while (getline(infile, line)) {
        if (line.empty()) continue;
        if (line.substr(0, 10) == "Outputwire") {
            istringstream oss(line.substr(11)); 
            int wire;
            while (oss >> wire) {
                circuit.outputWires.push_back(wire);
            }
            continue;
        }

        istringstream gateStream(line);
        vector<string> tokens;
        string token;
        while (gateStream >> token) {
            tokens.push_back(token);
        }
        if (tokens.empty()) continue;

        int numInputs = stoi(tokens[0]);
        int numOutputs = stoi(tokens[1]);

        if (tokens.size() < 2 + numInputs + numOutputs + 1) {
            cerr << "Invalid gate line: " << line << endl;
            continue;
        }

        vector<int> inputWires;
        for (int j = 0; j < numInputs; ++j) {
            inputWires.push_back(stoi(tokens[2 + j]));
        }

        vector<int> outputWires;
        for (int j = 0; j < numOutputs; ++j) {
            int outWireID = stoi(tokens[2 + numInputs + j]);
            outputWires.push_back(outWireID);
        }

        string gateName = tokens.back();
        if (gateMap.find(gateName) == gateMap.end()) {
            cerr << "Unknown gate type: " << gateName << endl;
            continue;
        }
        GateType gateType = gateMap[gateName];

        for (int j = 0; j < numOutputs; ++j) {
            Gate gate;
            gate.type = gateType;
            gate.output = outputWires[j];
            gate.input1 = inputWires.size() > 0 ? inputWires[0] : -1;
            gate.input2 = inputWires.size() > 1 ? inputWires[1] : -1;
            circuit.gates.push_back(gate);
        }
    }

    // If output wires were not specified in the file, assign them
    if (circuit.outputWires.empty()) {
        cerr << "Output wires were not specified in the file. " << endl;
    }

    infile.close();
    return circuit;
}




vector<Circuit> partitionCircuit(const Circuit& circuit, int windowSize) {
    vector<Circuit> subcircuits;
    if (getGateCount(circuit) <= windowSize) {
        subcircuits.push_back(circuit);
        return subcircuits;
    }

    // First identify the original constant gates wire IDs
    int const_one_output = -1;
    int const_zero_output = -1;
    Gate original_const_one;
    Gate original_const_zero;
    
    for (const Gate& gate : circuit.gates) {
        if (gate.type == CONST_ONE) {
            const_one_output = gate.output;
            original_const_one = gate;
        } else if (gate.type == CONST_ZERO) {
            const_zero_output = gate.output;
            original_const_zero = gate;
        }
    }

    unordered_map<int, vector<int>> gateGraph; // Adjacency list
    for (size_t idx = 0; idx < circuit.gates.size(); ++idx) {
        if (circuit.gates[idx].type == CONST_ONE || circuit.gates[idx].type == CONST_ZERO) {
            continue;
        }
        
        const Gate& gate = circuit.gates[idx];
        int outputWire = gate.output;
        for (int consumerGateIdx = 0; consumerGateIdx < circuit.gates.size(); ++consumerGateIdx) {
            const Gate& consumerGate = circuit.gates[consumerGateIdx];
            if (consumerGate.input1 == outputWire || consumerGate.input2 == outputWire) {
                gateGraph[idx].push_back(consumerGateIdx);
            }
        }
        for (int inputWire : {gate.input1, gate.input2}) {
            if (inputWire >= 0) {
                for (int producerGateIdx = 0; producerGateIdx < circuit.gates.size(); ++producerGateIdx) {
                    if (circuit.gates[producerGateIdx].output == inputWire) {
                        gateGraph[idx].push_back(producerGateIdx);
                    }
                }
            }
        }
    }

    unordered_set<int> visited;
    for (size_t idx = 0; idx < circuit.gates.size(); ++idx) {
        if (visited.count(idx) || circuit.gates[idx].type == CONST_ONE || 
            circuit.gates[idx].type == CONST_ZERO) {
            continue;
        }
        
        vector<int> component;
        stack<int> dfsStack;
        dfsStack.push(idx);
        visited.insert(idx);
        while (!dfsStack.empty()) {
            int gateIdx = dfsStack.top();
            dfsStack.pop();
            component.push_back(gateIdx);

            for (int neighbor : gateGraph[gateIdx]) {
                if (!visited.count(neighbor)) {
                    dfsStack.push(neighbor);
                    visited.insert(neighbor);
                }
            }
        }
        
        size_t startIdx = 0;
        while (startIdx < component.size()) {
            size_t endIdx = min(startIdx + windowSize - 2, component.size());
            vector<int> subcircuitGateIndices(component.begin() + startIdx, component.begin() + endIdx);

            Circuit subcircuit;

            // Check if any gate in this subcircuit uses constants
            bool needs_constants = false;
            for (int gateIdx : subcircuitGateIndices) {
                const Gate& gate = circuit.gates[gateIdx];
                if (gate.input1 == const_one_output || gate.input1 == const_zero_output ||
                    gate.input2 == const_one_output || gate.input2 == const_zero_output) {
                    needs_constants = true;
                    break;
                }
            }

            // Always add constants to the subcircuit
            subcircuit.gates.push_back(original_const_one);
            subcircuit.gates.push_back(original_const_zero);

            // Add the non-constant gates
            for (int gateIdx : subcircuitGateIndices) {
                const Gate& gate = circuit.gates[gateIdx];
                if (gate.type != CONST_ONE && gate.type != CONST_ZERO) {
                    subcircuit.gates.push_back(gate);
                }
            }

            unordered_set<int> subcircuitGates(subcircuitGateIndices.begin(), subcircuitGateIndices.end());
            unordered_set<int> subcircuitWires;
            
            // Add constant wire outputs
            subcircuitWires.insert(const_one_output);
            subcircuitWires.insert(const_zero_output);

            for (int gateIdx : subcircuitGateIndices) {
                subcircuitWires.insert(circuit.gates[gateIdx].output);
            }

           unordered_set<int> subcircuitInputWires;
            unordered_set<int> subcircuitOutputWires;
            for (int gateIdx : subcircuitGateIndices) {
                const Gate& gate = circuit.gates[gateIdx];
                for (int inputWire : {gate.input1, gate.input2}) {
                    if (inputWire >= 0) {
                        if (!subcircuitWires.count(inputWire) && 
                            inputWire != const_one_output && 
                            inputWire != const_zero_output) {
                            subcircuitInputWires.insert(inputWire);
                        }
                    }
                }
            }

            unordered_set<int> circuitOutputWires(circuit.outputWires.begin(), circuit.outputWires.end());

            for (int gateIdx : subcircuitGateIndices) {
                const Gate& gate = circuit.gates[gateIdx];
                int outputWire = gate.output;
                
                // Skip if this gate is a constant gate or its wire comes from a constant gate
                if (gate.type == CONST_ONE || gate.type == CONST_ZERO ||
                    outputWire == const_one_output || outputWire == const_zero_output) {
                    continue;
                }

                bool isOutputWire = false;

                for (size_t consumerGateIdx = 0; consumerGateIdx < circuit.gates.size(); ++consumerGateIdx) {
                    const Gate& consumerGate = circuit.gates[consumerGateIdx];
                    if ((consumerGate.input1 == outputWire || consumerGate.input2 == outputWire) &&
                        !subcircuitGates.count(consumerGateIdx)) {
                        isOutputWire = true;
                        break;
                    }
                }

                if (circuitOutputWires.count(outputWire) > 0) {
                    isOutputWire = true;
                }

                if (isOutputWire) {
                    subcircuitOutputWires.insert(outputWire);
                }
            }

            subcircuit.numInputs = subcircuitInputWires.size();
            subcircuit.numOutputs = subcircuitOutputWires.size();
            subcircuit.inputWires.assign(subcircuitInputWires.begin(), subcircuitInputWires.end());
            subcircuit.outputWires.assign(subcircuitOutputWires.begin(), subcircuitOutputWires.end());

            subcircuits.push_back(subcircuit);

            cout << "Subcircuit " << subcircuits.size() << " has " << subcircuit.numInputs
                 << " input wires and " << subcircuit.gates.size() << " gates." << endl;
            cout << "Input wires: ";
            for (int wire : subcircuit.inputWires) {
                cout << wire << " ";
            }
            cout << "output wires: ";
            for (int wire : subcircuit.outputWires) {
                cout << wire << " ";
            }   
            cout << endl;

            startIdx = endIdx;
        }
    }

    return subcircuits;
}


int getGateCount(const Circuit& circuit) {
    return circuit.gates.size();
}
