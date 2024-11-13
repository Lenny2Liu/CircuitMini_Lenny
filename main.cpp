#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>

// Hash function for pair<int, int> if needed in future
struct pair_hash {
    template <class T1, class T2>
    std::size_t operator () (const std::pair<T1,T2> &pair) const {
        return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
    }
};

struct Gate {
    int numInputs;
    int numOutputs;
    std::vector<int> inputWires;
    std::vector<int> outputWires;
    std::string type;
};

struct TriStateGate {
    std::string type;
    std::vector<int> inputWires;
    int outputWire;
};

bool readCircuit(const std::string& filename, int& numGates, int& numWires,
                 int& niv, std::vector<int>& inputWireCounts,
                 int& nov, std::vector<int>& outputWireCounts,
                 std::vector<Gate>& gates) {
    std::ifstream circuitFile(filename.c_str());
    if (!circuitFile.is_open()) {
        std::cerr << "Failed to open the circuit file." << std::endl;
        return false;
    }

    if (!(circuitFile >> numGates >> numWires)) {
        std::cerr << "Error reading number of gates and wires." << std::endl;
        return false;
    }

    if (!(circuitFile >> niv)) {
        std::cerr << "Error reading number of input values." << std::endl;
        return false;
    }
    inputWireCounts.resize(niv);
    for (int i = 0; i < niv; ++i) {
        if (!(circuitFile >> inputWireCounts[i])) {
            std::cerr << "Error reading input wire counts." << std::endl;
            return false;
        }
    }

    if (!(circuitFile >> nov)) {
        std::cerr << "Error reading number of output values." << std::endl;
        return false;
    }
    outputWireCounts.resize(nov);
    for (int i = 0; i < nov; ++i) {
        if (!(circuitFile >> outputWireCounts[i])) {
            std::cerr << "Error reading output wire counts." << std::endl;
            return false;
        }
    }

    std::string line;
    std::getline(circuitFile, line); // Consume the remaining newline

    for (int i = 0; i < numGates; ++i) {
        std::getline(circuitFile, line);
        if (line.empty()) {
            --i;
            continue;
        }
        std::istringstream iss(line);
        Gate gate;
        if (!(iss >> gate.numInputs >> gate.numOutputs)) {
            std::cerr << "Error reading gate inputs and outputs." << std::endl;
            return false;
        }
        gate.inputWires.resize(gate.numInputs);
        for (int j = 0; j < gate.numInputs; ++j) {
            if (!(iss >> gate.inputWires[j])) {
                std::cerr << "Error reading gate input wires." << std::endl;
                return false;
            }
        }
        gate.outputWires.resize(gate.numOutputs);
        for (int j = 0; j < gate.numOutputs; ++j) {
            if (!(iss >> gate.outputWires[j])) {
                std::cerr << "Error reading gate output wires." << std::endl;
                return false;
            }
        }
        if (!(iss >> gate.type)) {
            std::cerr << "Error reading gate type." << std::endl;
            return false;
        }
        gates.push_back(gate);
    }
    return true;
}

bool transformCircuit(const std::vector<Gate>& gates, int numWires,
                      std::vector<TriStateGate>& triStateGates, int& nextWireId,
                      const std::vector<int>& originalOutputWires,
                      std::vector<int>& transformedOutputWires) {
    nextWireId = numWires;

    // Predefine constant wires to reuse across the circuit
    int const_one_wire = nextWireId++;
    int const_zero_wire = nextWireId++;

    // Create CONST_ONE and CONST_ZERO gates once
    TriStateGate constOneGate;
    constOneGate.type = "CONST_ONE";
    constOneGate.inputWires = {}; // No inputs
    constOneGate.outputWire = const_one_wire;
    triStateGates.push_back(constOneGate);

    TriStateGate constZeroGate;
    constZeroGate.type = "CONST_ZERO";
    constZeroGate.inputWires = {}; // No inputs
    constZeroGate.outputWire = const_zero_wire;
    triStateGates.push_back(constZeroGate);

    // Map to track wire IDs from the original circuit to the transformed circuit
    std::unordered_map<int, int> wireMapping;
    for (int i = 0; i < numWires; ++i) {
        wireMapping[i] = i; // Initialize mapping (assuming input wires remain the same)
    }

    for (size_t idx = 0; idx < gates.size(); ++idx) {
        const Gate& gate = gates[idx];

        // Map input wires using wireMapping
        std::vector<int> mappedInputWires;
        for (int wire : gate.inputWires) {
            if (wireMapping.find(wire) == wireMapping.end()) {
                // Wire not yet mapped, use the same wire ID
                wireMapping[wire] = wire;
            }
            mappedInputWires.push_back(wireMapping[wire]);
        }

        // Map output wires
        std::vector<int> mappedOutputWires;
        for (int wire : gate.outputWires) {
            if (wireMapping.find(wire) == wireMapping.end()) {
                // Assign new wire ID for output wires
                wireMapping[wire] = wire;
            }
            mappedOutputWires.push_back(wireMapping[wire]);
        }

        if (gate.type == "XOR") {
            TriStateGate tsGate;
            tsGate.type = "XOR";
            tsGate.inputWires = mappedInputWires;
            tsGate.outputWire = mappedOutputWires[0];
            triStateGates.push_back(tsGate);
        }
        else if (gate.type == "AND") {
            if (gate.numInputs != 2 || gate.numOutputs != 1) {
                std::cerr << "AND gate with incorrect number of inputs/outputs." << std::endl;
                return false;
            }
            int x = mappedInputWires[0];
            int y = mappedInputWires[1];
            int output = mappedOutputWires[0];

            int not_y_wire = nextWireId++;
            int buffer1_output = nextWireId++;
            int buffer0_output = nextWireId++;

            TriStateGate xorGate;
            xorGate.type = "XOR";
            xorGate.inputWires.push_back(y);
            xorGate.inputWires.push_back(const_one_wire);
            xorGate.outputWire = not_y_wire;
            triStateGates.push_back(xorGate);

            TriStateGate buffer1Gate;
            buffer1Gate.type = "BUFFER";
            buffer1Gate.inputWires.push_back(x);
            buffer1Gate.inputWires.push_back(y);
            buffer1Gate.outputWire = buffer1_output;
            triStateGates.push_back(buffer1Gate);

            TriStateGate buffer0Gate;
            buffer0Gate.type = "BUFFER";
            buffer0Gate.inputWires.push_back(const_zero_wire);
            buffer0Gate.inputWires.push_back(not_y_wire);
            buffer0Gate.outputWire = buffer0_output;
            triStateGates.push_back(buffer0Gate);

            TriStateGate joinGate;
            joinGate.type = "JOIN";
            joinGate.inputWires.push_back(buffer1_output);
            joinGate.inputWires.push_back(buffer0_output);
            joinGate.outputWire = output;
            triStateGates.push_back(joinGate);
        }
        else if (gate.type == "INV") {
            if (gate.numInputs != 1 || gate.numOutputs != 1) {
                std::cerr << "INV gate with incorrect number of inputs/outputs." << std::endl;
                return false;
            }
            int a = mappedInputWires[0];
            int output = mappedOutputWires[0];

            TriStateGate xorGate;
            xorGate.type = "XOR";
            xorGate.inputWires.push_back(a);
            xorGate.inputWires.push_back(const_one_wire);
            xorGate.outputWire = output;
            triStateGates.push_back(xorGate);
        }
        else if (gate.type == "EQ" || gate.type == "EQW") {
            if (gate.numInputs != 1 || gate.numOutputs != 1) {
                std::cerr << "EQ/EQW gate with incorrect number of inputs/outputs." << std::endl;
                return false;
            }
            int a = mappedInputWires[0];
            int output = mappedOutputWires[0];

            TriStateGate bufferGate;
            bufferGate.type = "BUFFER";
            bufferGate.inputWires.push_back(a);
            bufferGate.inputWires.push_back(const_one_wire);
            bufferGate.outputWire = output;
            triStateGates.push_back(bufferGate);
        }
        else if (gate.type == "MAND") {
            if (gate.numInputs % 2 != 0 || gate.numOutputs != (gate.numInputs / 2)) {
                std::cerr << "MAND gate with incorrect number of inputs/outputs." << std::endl;
                return false;
            }
            int n = gate.numInputs / 2;
            for (int i = 0; i < n; ++i) {
                int x = mappedInputWires[i];
                int y = mappedInputWires[i + n];
                int output = mappedOutputWires[i];

                int not_y_wire = nextWireId++;
                int buffer1_output = nextWireId++;
                int buffer0_output = nextWireId++;

                TriStateGate xorGate;
                xorGate.type = "XOR";
                xorGate.inputWires.push_back(y);
                xorGate.inputWires.push_back(const_one_wire);
                xorGate.outputWire = not_y_wire;
                triStateGates.push_back(xorGate);

                TriStateGate buffer1Gate;
                buffer1Gate.type = "BUFFER";
                buffer1Gate.inputWires.push_back(x);
                buffer1Gate.inputWires.push_back(y);
                buffer1Gate.outputWire = buffer1_output;
                triStateGates.push_back(buffer1Gate);

                TriStateGate buffer0Gate;
                buffer0Gate.type = "BUFFER";
                buffer0Gate.inputWires.push_back(const_zero_wire);
                buffer0Gate.inputWires.push_back(not_y_wire);
                buffer0Gate.outputWire = buffer0_output;
                triStateGates.push_back(buffer0Gate);

                TriStateGate joinGate;
                joinGate.type = "JOIN";
                joinGate.inputWires.push_back(buffer1_output);
                joinGate.inputWires.push_back(buffer0_output);
                joinGate.outputWire = output;
                triStateGates.push_back(joinGate);
            }
        }
        else {
            std::cerr << "Unsupported gate type: " << gate.type << std::endl;
            return false;
        }
    }

    // Collect transformed output wires
    for (int originalOutputWire : originalOutputWires) {
        if (wireMapping.find(originalOutputWire) != wireMapping.end()) {
            transformedOutputWires.push_back(wireMapping[originalOutputWire]);
        } else {
            std::cerr << "Original output wire " << originalOutputWire << " not found in mapping." << std::endl;
            return false;
        }
    }

    return true;
}

// Function to output the transformed circuit to a file
void outputCircuit(const std::string& outputFilename, int totalTriStateGates, int totalTriStateWires,
                   int niv, const std::vector<int>& inputWireCounts,
                   int nov, const std::vector<int>& outputWireCounts,
                   const std::vector<TriStateGate>& triStateGates,
                   const std::vector<int>& transformedOutputWires) {
    std::ofstream outFile(outputFilename);
    if (!outFile) {
        std::cerr << "Failed to open output file: " << outputFilename << std::endl;
        exit(1);
    }

    outFile << totalTriStateGates << " " << totalTriStateWires << std::endl;
    outFile << niv;
    for (int count : inputWireCounts) {
        outFile << " " << count;
    }
    outFile << std::endl;
    outFile << nov;
    for (int count : outputWireCounts) {
        outFile << " " << count;
    }
    outFile << std::endl;

    for (const TriStateGate& tsGate : triStateGates) {
        if (tsGate.type == "XOR" || tsGate.type == "JOIN" || tsGate.type == "BUFFER") {
            int numInputs = tsGate.inputWires.size();
            int numOutputs = 1;
            outFile << numInputs << " " << numOutputs << " ";
            for (int wire : tsGate.inputWires) {
                outFile << wire << " ";
            }
            outFile << tsGate.outputWire << " " << tsGate.type << std::endl;
        }
        else if (tsGate.type == "CONST_ONE" || tsGate.type == "CONST_ZERO") {
            int numInputs = 0;
            int numOutputs = 1;
            outFile << numInputs << " " << numOutputs << " " << tsGate.outputWire << " " << tsGate.type << std::endl;
        }
        else {
            std::cerr << "Unsupported tri-state gate type: " << tsGate.type << std::endl;
            exit(1);
        }
    }


    outFile.close();
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./transformer <input_circuit_file> <output_file>" << std::endl;
        return 1;
    }

    int numGates, numWires;
    int niv, nov;
    std::vector<int> inputWireCounts, outputWireCounts;
    std::vector<Gate> gates;

    if (!readCircuit(argv[1], numGates, numWires, niv, inputWireCounts, nov, outputWireCounts, gates)) {
        return 1;
    }

    // Collect original output wires
    std::vector<int> originalOutputWires;
    int totalOutputWireCount = 0;
    for (int count : outputWireCounts) {
        totalOutputWireCount += count;
    }
    originalOutputWires.resize(totalOutputWireCount);

    // Assuming output wires are the last wires (adjust as needed)
    for (int i = 0; i < totalOutputWireCount; ++i) {
        originalOutputWires[i] = numWires - totalOutputWireCount + i;
    }

    std::vector<TriStateGate> triStateGates;
    int nextWireId;
    std::vector<int> transformedOutputWires;
    if (!transformCircuit(gates, numWires, triStateGates, nextWireId, originalOutputWires, transformedOutputWires)) {
        return 1;
    }

    int totalTriStateGates = triStateGates.size();
    int totalTriStateWires = nextWireId;

    outputCircuit(argv[2], totalTriStateGates, totalTriStateWires,
                  niv, inputWireCounts, nov, outputWireCounts,
                  triStateGates, transformedOutputWires);

    return 0;
}
