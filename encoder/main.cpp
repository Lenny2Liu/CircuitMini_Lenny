#include <iostream>
#include <vector>
#include <string>
#include "circuit.h"
#include "qbfencoder.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: <input_circuit_file>" << endl;
        return 1;
    }
    Circuit circuit = readCircuit(argv[1]);

    int windowSize = 5; // Define the window size as needed
    vector<Circuit> subcircuits = partitionCircuit(circuit, windowSize);
    cout << "Circuit has been partitioned into " << subcircuits.size() << " subcircuits" << endl;
    for (size_t i = 0; i < subcircuits.size(); ++i) {
        string qbfFilename = "../qbf/subcircuit_" + to_string(i + 1) + ".qdimacs";
        encodeSubcircuitAsQBF(subcircuits[i], qbfFilename);
        cout << "Subcircuit " << i + 1 << " has been written to " << qbfFilename << endl;
    }

    return 0;
}
