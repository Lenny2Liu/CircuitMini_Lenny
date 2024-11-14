#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include "circuit.h"
#include "qbfencoder.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: <input_circuit_file>" << endl;
        return 1;
    }
    Circuit circuit = readCircuit(argv[1]);

    // minimize join gates, BUFFER is not free

    int MinimizedCount = 0;
    int minimizedGateCount = 0;
    int windowSize = 5; // By default, can change
    vector<Circuit> subcircuits = partitionCircuit(circuit, windowSize);
    cout << "Circuit has been partitioned into " << subcircuits.size() << " subcircuits" << endl;

    for (size_t i = 0; i < subcircuits.size(); ++i) {
        cout << "Subcircuit " << i + 1 << ":" << endl;
        for (const auto& gate : subcircuits[i].gates) {
            cout << "Gate: " << gate.type << ", Input1: " << gate.input1 << ", Input2: " << gate.input2 << ", Output: " << gate.output << endl;
        }
        cout << endl;
    }
    int maxEll = windowSize;
    QDPLLResult result;
    for (size_t i = 0; i < subcircuits.size(); ++i) { 
        bool found = false;
        for (int ell = 3; ell <= getGateCount(subcircuits[i]); ell++) {
            string qbfFilename = "../qbf/subcircuit_" + to_string(i + 1) + "_ell_" + to_string(ell) + ".qdimacs";
            result = encodeSubcircuitAsQBF(subcircuits[i], ell, qbfFilename);
            if (result == QDPLL_RESULT_SAT) {
                cout << "Found a solution with ell = " << ell << " gates for subcircuit " << i + 1 << endl;
                found = true;
                MinimizedCount ++;
                minimizedGateCount += windowSize - ell;
                break;
            }
            cout << "Subcircuit " << i + 1 << " with ell = " << ell << " has been written to " << qbfFilename << endl;

            // string solverCommand = "./depqbf " + qbfFilename + " > solver_output.txt";
            // int result = system(solverCommand.c_str());

            // ifstream solverOutput("solver_output.txt");
            // string firstLine;
            // getline(solverOutput, firstLine);
            // if (firstLine == "SAT") {
            //     cout << "Found a solution with ell = " << ell << " gates for subcircuit " << i + 1 << endl;
            //     found = true;
            //     MinimizedCount ++;
            //     minimizedGateCount += windowSize - ell;
            //     break;
            // } else {
            //     cout << "No solution with ell = " << ell << " gates for subcircuit " << i + 1 << endl;
            // }
        }
        if (!found) {
            cout << "Could not synthesize subcircuit " << i + 1 << " within the gate limit." << endl;
        }
    }
    cout << "Total minimized count: " << MinimizedCount << endl;
    cout << "Total minimized gate count: " << minimizedGateCount << endl;
    return 0;
}
