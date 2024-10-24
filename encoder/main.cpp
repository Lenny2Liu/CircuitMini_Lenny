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

    int windowSize = 5; // Define the window size as needed
    vector<Circuit> subcircuits = partitionCircuit(circuit, windowSize);
    cout << "Circuit has been partitioned into " << subcircuits.size() << " subcircuits" << endl;
    int maxEll = 5; // Set a reasonable upper limit for ell
    for (size_t i = 0; i < subcircuits.size(); ++i) {
        bool found = false;
        for (int ell = maxEll / 2; ell <= maxEll; ++ell) {
            string qbfFilename = "../qbf/subcircuit_" + to_string(i + 1) + "_ell_" + to_string(ell) + ".qdimacs";
            encodeSubcircuitAsQBF(subcircuits[i], ell, qbfFilename);
            cout << "Subcircuit " << i + 1 << " with ell = " << ell << " has been written to " << qbfFilename << endl;

            // Run the QBF solver
            string solverCommand = "../qbf/depqbf -v " + qbfFilename + " > solver_output.txt";
            int result = system(solverCommand.c_str());

            // Parse the solver output to check if SAT or UNSAT
            ifstream solverOutput("solver_output.txt");
            string firstLine;
            getline(solverOutput, firstLine);
            if (firstLine == "SAT") {
                cout << "Found a solution with ell = " << ell << " gates for subcircuit " << i + 1 << endl;
                found = true;
                break;
            } else {
                cout << "No solution with ell = " << ell << " gates for subcircuit " << i + 1 << endl;
            }
        }
        if (!found) {
            cout << "Could not synthesize subcircuit " << i + 1 << " within the gate limit." << endl;
        }
    }

    return 0;
}
