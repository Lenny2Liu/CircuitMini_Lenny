#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

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
    std::getline(circuitFile, line);

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
                      std::vector<TriStateGate>& triStateGates, int& nextWireId) {
    nextWireId = numWires;

    for (size_t idx = 0; idx < gates.size(); ++idx) {
        const Gate& gate = gates[idx];
        if (gate.type == "XOR") {
            TriStateGate tsGate;
            tsGate.type = "XOR";
            tsGate.inputWires = gate.inputWires;
            tsGate.outputWire = gate.outputWires[0];
            triStateGates.push_back(tsGate);
        }
        else if (gate.type == "AND") {
            if (gate.numInputs != 2 || gate.numOutputs != 1) {
                std::cerr << "AND gate with incorrect number of inputs/outputs." << std::endl;
                return false;
            }
            int x = gate.inputWires[0];
            int y = gate.inputWires[1];
            int output = gate.outputWires[0];

            int not_y_wire = nextWireId++;
            int const_one_wire = nextWireId++;
            int const_zero_wire = nextWireId++;
            int buffer1_output = nextWireId++;
            int buffer0_output = nextWireId++;

            TriStateGate constOneGate;
            constOneGate.type = "CONST_ONE";
            constOneGate.outputWire = const_one_wire;
            triStateGates.push_back(constOneGate);

            TriStateGate xorGate;
            xorGate.type = "XOR";
            xorGate.inputWires.push_back(y);
            xorGate.inputWires.push_back(const_one_wire);
            xorGate.outputWire = not_y_wire;
            triStateGates.push_back(xorGate);

            TriStateGate constZeroGate;
            constZeroGate.type = "CONST_ZERO";
            constZeroGate.outputWire = const_zero_wire;
            triStateGates.push_back(constZeroGate);

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
            int a = gate.inputWires[0];
            int constOneWire = nextWireId++;

            TriStateGate constGate;
            constGate.type = "CONST_ONE";
            constGate.outputWire = constOneWire;
            triStateGates.push_back(constGate);

            TriStateGate xorGate;
            xorGate.type = "XOR";
            xorGate.inputWires.push_back(a);
            xorGate.inputWires.push_back(constOneWire);
            xorGate.outputWire = gate.outputWires[0];
            triStateGates.push_back(xorGate);
        }
        else if (gate.type == "EQ" || gate.type == "EQW") {
            if (gate.numInputs != 1 || gate.numOutputs != 1) {
                std::cerr << "EQ/EQW gate with incorrect number of inputs/outputs." << std::endl;
                return false;
            }
            int constOneWire = nextWireId++;

            TriStateGate constGate;
            constGate.type = "CONST_ONE";
            constGate.outputWire = constOneWire;
            triStateGates.push_back(constGate);

            TriStateGate bufferGate;
            bufferGate.type = "BUFFER";
            bufferGate.inputWires.push_back(gate.inputWires[0]);
            bufferGate.inputWires.push_back(constOneWire);
            bufferGate.outputWire = gate.outputWires[0];
            triStateGates.push_back(bufferGate);
        }
        else if (gate.type == "MAND") {
            if (gate.numInputs % 2 != 0 || gate.numOutputs != (gate.numInputs / 2)) {
                std::cerr << "MAND gate with incorrect number of inputs/outputs." << std::endl;
                return false;
            }
            int n = gate.numInputs / 2;
            for (int i = 0; i < n; ++i) {
                int x = gate.inputWires[i];
                int y = gate.inputWires[i + n];
                int output = gate.outputWires[i];

                int not_y_wire = nextWireId++;
                int const_one_wire = nextWireId++;
                int const_zero_wire = nextWireId++;
                int buffer1_output = nextWireId++;
                int buffer0_output = nextWireId++;

                TriStateGate constOneGate;
                constOneGate.type = "CONST_ONE";
                constOneGate.outputWire = const_one_wire;
                triStateGates.push_back(constOneGate);

                TriStateGate xorGate;
                xorGate.type = "XOR";
                xorGate.inputWires.push_back(y);
                xorGate.inputWires.push_back(const_one_wire);
                xorGate.outputWire = not_y_wire;
                triStateGates.push_back(xorGate);

                TriStateGate constZeroGate;
                constZeroGate.type = "CONST_ZERO";
                constZeroGate.outputWire = const_zero_wire;
                triStateGates.push_back(constZeroGate);

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

    return true;
}

void outputCircuit(const std::string& outputFilename, int totalTriStateGates, int totalTriStateWires,
                   int niv, const std::vector<int>& inputWireCounts,
                   int nov, const std::vector<int>& outputWireCounts,
                   const std::vector<TriStateGate>& triStateGates) {
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
        if (tsGate.type == "XOR") {
            outFile << "2 1 " << tsGate.inputWires[0] << " " << tsGate.inputWires[1]
                    << " " << tsGate.outputWire << " XOR" << std::endl;
        }
        else if (tsGate.type == "JOIN") {
            outFile << "2 1 " << tsGate.inputWires[0] << " " << tsGate.inputWires[1]
                    << " " << tsGate.outputWire << " JOIN" << std::endl;
        }
        else if (tsGate.type == "BUFFER") {
            outFile << "2 1 " << tsGate.inputWires[0] << " " << tsGate.inputWires[1]
                    << " " << tsGate.outputWire << " BUFFER" << std::endl;
        }
        else if (tsGate.type == "CONST_ONE") {
            outFile << "0 1 " << tsGate.outputWire << " CONST_ONE" << std::endl;
        }
        else if (tsGate.type == "CONST_ZERO") {
            outFile << "0 1 " << tsGate.outputWire << " CONST_ZERO" << std::endl;
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

    std::vector<TriStateGate> triStateGates;
    int nextWireId;
    if (!transformCircuit(gates, numWires, triStateGates, nextWireId)) {
        return 1;
    }

    int totalTriStateGates = triStateGates.size();
    int totalTriStateWires = nextWireId;

    outputCircuit(argv[2], totalTriStateGates, totalTriStateWires,
                  niv, inputWireCounts, nov, outputWireCounts,
                  triStateGates);

    return 0;
}
