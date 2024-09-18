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
    std::string type; // XOR, AND, INV, EQ, EQW, MAND
};

struct TriStateGate {
    std::string type; // XOR, JOIN, BUFFER, CONST_ONE, CONST_ZERO
    std::vector<int> inputWires;
    int outputWire;
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./transformer <input_circuit_file>" << std::endl;
        return 1;
    }

    std::ifstream circuitFile(argv[1]);
    if (!circuitFile.is_open()) {
        std::cerr << "Failed to open the circuit file." << std::endl;
        return 1;
    }

    std::string line;
    int numGates, numWires;
    if (!(circuitFile >> numGates >> numWires)) {
        std::cerr << "Error reading number of gates and wires." << std::endl;
        return 1;
    }
    int niv; 
    if (!(circuitFile >> niv)) {
        std::cerr << "Error reading number of input values." << std::endl;
        return 1;
    }
    std::vector<int> inputWireCounts(niv);
    for (int i = 0; i < niv; ++i) {
        if (!(circuitFile >> inputWireCounts[i])) {
            std::cerr << "Error reading input wire counts." << std::endl;
            return 1;
        }
    }

    // Read the number of output values and their wire counts
    int nov;
    if (!(circuitFile >> nov)) {
        std::cerr << "Error reading number of output values." << std::endl;
        return 1;
    }
    std::vector<int> outputWireCounts(nov);
    for (int i = 0; i < nov; ++i) {
        if (!(circuitFile >> outputWireCounts[i])) {
            std::cerr << "Error reading output wire counts." << std::endl;
            return 1;
        }
    }
    std::getline(circuitFile, line);
    std::vector<Gate> gates;
    for (int i = 0; i < numGates; ++i) {
        std::getline(circuitFile, line);
        if (line.empty()) {
            --i; // Adjust index if line is empty
            continue;
        }
        std::istringstream iss(line);
        Gate gate;
        if (!(iss >> gate.numInputs >> gate.numOutputs)) {
            std::cerr << "Error reading gate inputs and outputs." << std::endl;
            return 1;
        }
        gate.inputWires.resize(gate.numInputs);
        for (int j = 0; j < gate.numInputs; ++j) {
            if (!(iss >> gate.inputWires[j])) {
                std::cerr << "Error reading gate input wires." << std::endl;
                return 1;
            }
        }
        gate.outputWires.resize(gate.numOutputs);
        for (int j = 0; j < gate.numOutputs; ++j) {
            if (!(iss >> gate.outputWires[j])) {
                std::cerr << "Error reading gate output wires." << std::endl;
                return 1;
            }
        }
        if (!(iss >> gate.type)) {
            std::cerr << "Error reading gate type." << std::endl;
            return 1;
        }
        gates.push_back(gate);
    }

    // Transform the boolean circuit into a tri-state circuit
    std::vector<TriStateGate> triStateGates;
    int nextWireId = numWires;

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
            // AND(x, y) = (x / y) JOIN (0 / (y XOR 1))
            if (gate.numInputs != 2 || gate.numOutputs != 1) {
                std::cerr << "AND gate with incorrect number of inputs/outputs." << std::endl;
                return 1;
            }
            int x = gate.inputWires[0];
            int y = gate.inputWires[1];
            int output = gate.outputWires[0];

            int not_y_wire = nextWireId++;
            int const_one_wire = nextWireId++;
            int const_zero_wire = nextWireId++;
            int buffer1_output = nextWireId++;
            int buffer0_output = nextWireId++;

            // Create CONST_ONE gate for constant 1
            TriStateGate constOneGate;
            constOneGate.type = "CONST_ONE";
            constOneGate.outputWire = const_one_wire;
            triStateGates.push_back(constOneGate);

            // Compute not_y = y XOR const_one_wire
            TriStateGate xorGate;
            xorGate.type = "XOR";
            xorGate.inputWires.push_back(y);
            xorGate.inputWires.push_back(const_one_wire);
            xorGate.outputWire = not_y_wire;
            triStateGates.push_back(xorGate);

            // Create CONST_ZERO gate for constant 0
            TriStateGate constZeroGate;
            constZeroGate.type = "CONST_ZERO";
            constZeroGate.outputWire = const_zero_wire;
            triStateGates.push_back(constZeroGate);

            // Buffer x controlled by y (buffer1_output = BUFFER(x, y))
            TriStateGate buffer1Gate;
            buffer1Gate.type = "BUFFER";
            buffer1Gate.inputWires.push_back(x);
            buffer1Gate.inputWires.push_back(y); // Control signal
            buffer1Gate.outputWire = buffer1_output;
            triStateGates.push_back(buffer1Gate);

            // Buffer 0 controlled by not_y (buffer0_output = BUFFER(0, not_y))
            TriStateGate buffer0Gate;
            buffer0Gate.type = "BUFFER";
            buffer0Gate.inputWires.push_back(const_zero_wire);
            buffer0Gate.inputWires.push_back(not_y_wire); // Control signal
            buffer0Gate.outputWire = buffer0_output;
            triStateGates.push_back(buffer0Gate);

            // Join the two buffers (output = JOIN(buffer1_output, buffer0_output))
            TriStateGate joinGate;
            joinGate.type = "JOIN";
            joinGate.inputWires.push_back(buffer1_output);
            joinGate.inputWires.push_back(buffer0_output);
            joinGate.outputWire = output;
            triStateGates.push_back(joinGate);
        }
        else if (gate.type == "INV") {
            // Transform INV gate using XOR with constant 1
            if (gate.numInputs != 1 || gate.numOutputs != 1) {
                std::cerr << "INV gate with incorrect number of inputs/outputs." << std::endl;
                return 1;
            }
            int a = gate.inputWires[0];
            int constOneWire = nextWireId++;

            // Create a CONST_ONE gate to set constOneWire to 1
            TriStateGate constGate;
            constGate.type = "CONST_ONE";
            constGate.outputWire = constOneWire;
            triStateGates.push_back(constGate);

            // output = a XOR constOneWire
            TriStateGate xorGate;
            xorGate.type = "XOR";
            xorGate.inputWires.push_back(a);
            xorGate.inputWires.push_back(constOneWire);
            xorGate.outputWire = gate.outputWires[0];
            triStateGates.push_back(xorGate);
        }
        else if (gate.type == "EQ" || gate.type == "EQW") {
            // Use BUFFER gate for wire assignments with control signal set to 1
            if (gate.numInputs != 1 || gate.numOutputs != 1) {
                std::cerr << "EQ/EQW gate with incorrect number of inputs/outputs." << std::endl;
                return 1;
            }
            int constOneWire = nextWireId++;

            // Create a CONST_ONE gate
            TriStateGate constGate;
            constGate.type = "CONST_ONE";
            constGate.outputWire = constOneWire;
            triStateGates.push_back(constGate);

            // BUFFER(input, 1)
            TriStateGate bufferGate;
            bufferGate.type = "BUFFER";
            bufferGate.inputWires.push_back(gate.inputWires[0]);
            bufferGate.inputWires.push_back(constOneWire); // Control signal is 1
            bufferGate.outputWire = gate.outputWires[0];
            triStateGates.push_back(bufferGate);
        }
        else if (gate.type == "MAND") {
            // Decompose MAND into individual AND transformations
            if (gate.numInputs % 2 != 0 || gate.numOutputs != (gate.numInputs / 2)) {
                std::cerr << "MAND gate with incorrect number of inputs/outputs." << std::endl;
                return 1;
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

                // Compute NOT y (not_y = y XOR 1)
                TriStateGate constOneGate;
                constOneGate.type = "CONST_ONE";
                constOneGate.outputWire = const_one_wire;
                triStateGates.push_back(constOneGate);

                // y XOR const_one_wire
                TriStateGate xorGate;
                xorGate.type = "XOR";
                xorGate.inputWires.push_back(y);
                xorGate.inputWires.push_back(const_one_wire);
                xorGate.outputWire = not_y_wire;
                triStateGates.push_back(xorGate);

                // constant 0
                TriStateGate constZeroGate;
                constZeroGate.type = "CONST_ZERO";
                constZeroGate.outputWire = const_zero_wire;
                triStateGates.push_back(constZeroGate);

                // Buffer x controlled by y
                TriStateGate buffer1Gate;
                buffer1Gate.type = "BUFFER";
                buffer1Gate.inputWires.push_back(x);
                buffer1Gate.inputWires.push_back(y);
                buffer1Gate.outputWire = buffer1_output;
                triStateGates.push_back(buffer1Gate);

                // Buffer 0 controlled by not_y
                TriStateGate buffer0Gate;
                buffer0Gate.type = "BUFFER";
                buffer0Gate.inputWires.push_back(const_zero_wire);
                buffer0Gate.inputWires.push_back(not_y_wire);
                buffer0Gate.outputWire = buffer0_output;
                triStateGates.push_back(buffer0Gate);

                // Join the two buffers
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
            return 1;
        }
    }

    // OUTPUT

    int totalTriStateGates = triStateGates.size();
    int totalTriStateWires = nextWireId;

    std::cout << totalTriStateGates << " " << totalTriStateWires << std::endl;
    std::cout << niv;
    for (int count : inputWireCounts) {
        std::cout << " " << count;
    }
    std::cout << std::endl;
    std::cout << nov;
    for (int count : outputWireCounts) {
        std::cout << " " << count;
    }
    std::cout << std::endl;

    for (size_t i = 0; i < triStateGates.size(); ++i) {
        const TriStateGate& tsGate = triStateGates[i];
        if (tsGate.type == "XOR") {
            // Format: 2 1 <input1> <input2> <output> XOR
            std::cout << "2 1 " << tsGate.inputWires[0] << " " << tsGate.inputWires[1] << " "
                      << tsGate.outputWire << " XOR" << std::endl;
        }
        else if (tsGate.type == "JOIN") {
            // Format: 2 1 <input1> <input2> <output> JOIN
            std::cout << "2 1 " << tsGate.inputWires[0] << " " << tsGate.inputWires[1] << " "
                      << tsGate.outputWire << " JOIN" << std::endl;
        }
        else if (tsGate.type == "BUFFER") {
            // Format: 2 1 <input> <control> <output> BUFFER
            std::cout << "2 1 " << tsGate.inputWires[0] << " " << tsGate.inputWires[1] << " "
                      << tsGate.outputWire << " BUFFER" << std::endl;
        }
        else if (tsGate.type == "CONST_ONE") {
            // Format: 0 1 <output> CONST_ONE
            std::cout << "0 1 " << tsGate.outputWire << " CONST_ONE" << std::endl;
        }
        else if (tsGate.type == "CONST_ZERO") {
            // Format: 0 1 <output> CONST_ZERO
            std::cout << "0 1 " << tsGate.outputWire << " CONST_ZERO" << std::endl;
        }
        else {
            // Unsupported tri-state gate type
            std::cerr << "Unsupported tri-state gate type: " << tsGate.type << std::endl;
            return 1;
        }
    }

    return 0;
}
