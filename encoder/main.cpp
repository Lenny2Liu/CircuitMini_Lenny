#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include "circuit.h"
#include "qbfencoder.h"

using namespace std;

struct SubcircuitResult {
        int originalJoins;
        int minimizedJoins;
        int totalGates;
    };



MinimizationResult findMinimumJoinGates(const Circuit& subcircuit, int index, int numGates) {
    int left = 0;  // minimum possible JOIN gates
    int right = getJOINCount(subcircuit);  // maximum possible JOIN gates
    MinimizationResult bestResult;
    bestResult.success = false;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        
        string qbfFilename = "../qbf/subcircuit_" + to_string(index) + "_#join_" + to_string(mid) + ".qdimacs";
        if (right == 0) {
            cout << "No JOIN gates in subcircuit " << index << endl;
            break;
        }
        MinimizationResult currentResult = encodeSubcircuitAsQBF(
            subcircuit, numGates, qbfFilename, mid);  // Pass maximum allowed joins
            
        if (currentResult.success) {
            bestResult = currentResult;
            right = mid - 1;  // Try to find solution with fewer JOIN gates
        } else {
            // No solution with 'mid' JOIN gates, try more
            left = mid + 1;
        }
    }
    
    return bestResult;
}



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
    vector<SubcircuitResult> results;

    for (size_t i = 0; i < subcircuits.size(); ++i) {
        cout << "Processing subcircuit " << i + 1 << "..." << endl;
        
        // Get original JOIN gate count
        int originalJoins = 0;
        for (const auto& gate : subcircuits[i].gates) {
            if (gate.type == JOIN) originalJoins++;
        }
        
        // Find minimum JOIN gates solution
        MinimizationResult result = findMinimumJoinGates(
            subcircuits[i], 
            i + 1,
            getGateCount(subcircuits[i]) * 2  // Allow up to 2x gates
        );
        
        if (result.success) {
            cout << "Successfully minimized subcircuit " << i + 1 << ":" << endl;
            cout << "  Original JOIN gates: " << originalJoins << endl;
            cout << "  Minimized JOIN gates: " << result.joinGates << endl;
            cout << "  Total gates used: " << result.totalGates << endl;
        } else {
            cout << "Could not minimize subcircuit " << i + 1 << endl;
        }
    }
}