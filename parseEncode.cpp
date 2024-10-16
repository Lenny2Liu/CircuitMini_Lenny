#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <set>


using namespace std;

enum GateType { JOIN, BUFFER, XOR, CONST_ZERO, CONST_ONE };

enum State { ZERO = 0, ONE = 1, Z = 2, X = 3 };

struct Gate {
    GateType type;
    int input1;
    int input2;
    int output;
};

bool operator==(const Gate& lhs, const Gate& rhs) {
    return lhs.type == rhs.type &&
           lhs.input1 == rhs.input1 &&
           lhs.input2 == rhs.input2 &&
           lhs.output == rhs.output;
}

// 2bits wire states.... since at least 3 legal states......
struct WireVars {
    int v1; 
    int v2; 
};

// Circuit structure
struct Circuit {
    int numInputs;
    int numOutputs;
    vector<Gate> gates;
    vector<int> inputWires;  
    vector<int> outputWires;
};

// name map
unordered_map<string, GateType> gateMap = {
    {"JOIN", JOIN},
    {"BUFFER", BUFFER},
    {"XOR", XOR},
    {"CONST_ZERO", CONST_ZERO},
    {"CONST_ONE", CONST_ONE}
};

// helper func for pari hashing, will be used in gate selection
struct pair_hash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ h2;
    }
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

    int wireIndex = 0;

    circuit.numInputs = totalInputWires;
    for (int i = 0; i < totalInputWires; ++i) {
        circuit.inputWires.push_back(wireIndex++);
    }

    while (getline(infile, line)) {
        if (line.empty()) continue;
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
            // how to handle input wires for buffer??????????????????, currently set to -1?????????????
            gate.input1 = inputWires.size() > 0 ? inputWires[0] : -1;
            gate.input2 = inputWires.size() > 1 ? inputWires[1] : -1;
            circuit.gates.push_back(gate);
        }
    }

    circuit.numOutputs = totalOutputWires;
    for (int i = numWires - totalOutputWires; i < numWires; ++i) {
        circuit.outputWires.push_back(i);
    }

    infile.close();
    cout << "Input wires: ";
    for (int wire : circuit.inputWires) {
        cout << wire << " ";
    }
    cout << endl;
    return circuit;
}


vector<int> topologicalSort(const Circuit& circuit) {
    // Build adjacency list and compute in-degrees
    unordered_map<int, vector<int>> adjList;
    unordered_map<int, int> inDegree;
    unordered_set<int> nodeSet;
    for (const auto& gate : circuit.gates) {
        nodeSet.insert(gate.output);
        if (gate.input1 >= 0) {
            adjList[gate.input1].push_back(gate.output);
            inDegree[gate.output]++;
            nodeSet.insert(gate.input1);
        }
        if (gate.input2 >= 0) {
            adjList[gate.input2].push_back(gate.output);
            inDegree[gate.output]++;
            nodeSet.insert(gate.input2);
        }
        if (inDegree.find(gate.output) == inDegree.end()) {
            inDegree[gate.output] = 0; 
        }
    }
    queue<int> zeroInDegree;
    for (int node : nodeSet) {
        if (inDegree[node] == 0) {
            zeroInDegree.push(node);
        }
    }
    vector<int> topoOrder;
    while (!zeroInDegree.empty()) {
        int node = zeroInDegree.front();
        zeroInDegree.pop();
        topoOrder.push_back(node);

        for (int neighbor : adjList[node]) {
            inDegree[neighbor]--;
            if (inDegree[neighbor] == 0) {
                zeroInDegree.push(neighbor);
            }
        }
    }
    unordered_map<int, int> wireToGateIndex;
    for (size_t i = 0; i < circuit.gates.size(); ++i) {
        wireToGateIndex[circuit.gates[i].output] = i;
    }
    vector<int> sortedGateIndices;
    for (int wireID : topoOrder) {
        if (wireToGateIndex.find(wireID) != wireToGateIndex.end()) {
            sortedGateIndices.push_back(wireToGateIndex[wireID]);
        }
    }

    return sortedGateIndices;
}


// TODO 10.15: WIERD RESULT WHEN HANDLING THE LAST SUBCIRCUIT, NEED TO FIX

vector<Circuit> partitionCircuit(const Circuit& circuit, int windowSize) {
    vector<Circuit> subcircuits;

    unordered_map<int, vector<int>> wireToConsumerGates;
    for (size_t idx = 0; idx < circuit.gates.size(); ++idx) {
        const Gate& gate = circuit.gates[idx];
        if (gate.input1 >= 0) {
            wireToConsumerGates[gate.input1].push_back(idx);
        }
        if (gate.input2 >= 0) {
            wireToConsumerGates[gate.input2].push_back(idx);
        }
    }

    vector<int> currentGateIndices;      
    unordered_set<int> subcircuitWires;    
    unordered_set<int> subcircuitInputWires; 
    unordered_set<int> subcircuitOutputWires; 

    vector<int> sortedGateIndices = topologicalSort(circuit);

    for (int gateIndex : sortedGateIndices) {
        const Gate& gate = circuit.gates[gateIndex];

        currentGateIndices.push_back(gateIndex);
        subcircuitWires.insert(gate.output);

        if (gate.input1 >= 0 && subcircuitWires.count(gate.input1) == 0) {
            subcircuitInputWires.insert(gate.input1);
        }
        if (gate.input2 >= 0 && subcircuitWires.count(gate.input2) == 0) {
            subcircuitInputWires.insert(gate.input2);
        }

        if (currentGateIndices.size() == windowSize) {

            for (int wire : subcircuitWires) {
                bool isOutputWire = false;

                for (int consumerGateIndex : wireToConsumerGates[wire]) {
                    if (std::find(currentGateIndices.begin(), currentGateIndices.end(), consumerGateIndex) == currentGateIndices.end()) {
                        isOutputWire = true;
                        break;
                    }
                }

                if (isOutputWire) {
                    subcircuitOutputWires.insert(wire);
                }
            }

            Circuit subcircuit;
            subcircuit.numInputs = subcircuitInputWires.size();
            subcircuit.numOutputs = subcircuitOutputWires.size();

            for (int idx : currentGateIndices) {
                subcircuit.gates.push_back(circuit.gates[idx]);
            }

            subcircuit.inputWires.assign(subcircuitInputWires.begin(), subcircuitInputWires.end());
            subcircuit.outputWires.assign(subcircuitOutputWires.begin(), subcircuitOutputWires.end());

            subcircuits.push_back(subcircuit);

            currentGateIndices.clear();
            subcircuitWires.clear();
            subcircuitInputWires.clear();
            subcircuitOutputWires.clear();
        }
    }

    // Finalize the last subcircuit if it's not empty
    if (!currentGateIndices.empty()) {
        // Determine output wires
        for (int wire : subcircuitWires) {
            bool isOutputWire = false;

            for (int consumerGateIndex : wireToConsumerGates[wire]) {
                if (std::find(currentGateIndices.begin(), currentGateIndices.end(), consumerGateIndex) == currentGateIndices.end()) {
                    isOutputWire = true;
                    break;
                }
            }

            if (isOutputWire) {
                subcircuitOutputWires.insert(wire);
            }
        }

        Circuit subcircuit;
        subcircuit.numInputs = subcircuitInputWires.size();
        subcircuit.numOutputs = subcircuitOutputWires.size();

        for (int idx : currentGateIndices) {
            subcircuit.gates.push_back(circuit.gates[idx]);
        }

        subcircuit.inputWires.assign(subcircuitInputWires.begin(), subcircuitInputWires.end());
        subcircuit.outputWires.assign(subcircuitOutputWires.begin(), subcircuitOutputWires.end());

        subcircuits.push_back(subcircuit);
    }

    return subcircuits;
}


int getNextVarID() {
    static int varID = 0;
    return ++varID;
}


void addExactlyOneConstraint(const vector<int>& vars, vector<string>& clauses) {
    // At least one variable is true
    string atLeastOneClause;
    for (int var : vars) {
        atLeastOneClause += to_string(var) + " ";
    }
    atLeastOneClause += "0";
    clauses.push_back(atLeastOneClause);

    // At most one variable is true (pairwise mutual exclusion)
    for (size_t i = 0; i < vars.size(); ++i) {
        for (size_t j = i + 1; j < vars.size(); ++j) {
            clauses.push_back(to_string(-vars[i]) + " " + to_string(-vars[j]) + " 0");
        }
    }
}

void addConstGateCompatibilityConstraints(
    int funcVar,
    GateType funcType,
    int gateOutputVar_v1, int gateOutputVar_v2,
    vector<string>& clauses
) {
    if (funcType == CONST_ZERO) {
        // Clauses to enforce:
        // -funcVar ∨ -gateOutputVar_v1
        // -funcVar ∨ -gateOutputVar_v2

        clauses.push_back(to_string(-funcVar) + " " + to_string(-gateOutputVar_v1) + " 0");
        clauses.push_back(to_string(-funcVar) + " " + to_string(-gateOutputVar_v2) + " 0");

    } else if (funcType == CONST_ONE) {
        // Clauses to enforce:
        // -funcVar ∨ -gateOutputVar_v1
        // -funcVar ∨ gateOutputVar_v2

        clauses.push_back(to_string(-funcVar) + " " + to_string(-gateOutputVar_v1) + " 0");
        clauses.push_back(to_string(-funcVar) + " " + to_string(gateOutputVar_v2) + " 0");

    } else {
        cerr << "Invalid gate type in addConstGateCompatibilityConstraints" << endl;
    }
}

// This part seems incorrect??????????????????. FIXED 10.16, much better now :)

void addBUFFERCompatibilityConstraints(
    int funcVar, 
    int selVar1, int selVar2,  
    int controlVar_v1, int controlVar_v2, 
    int dataVar_v1, int dataVar_v2, 
    int gateOutputVar_v1, int gateOutputVar_v2, 
    vector<string>& clauses
) {
    // Possible states for control and data
    vector<tuple<int, int>> possibleStates = {
        {1, 0}, // Z
        {0, 0}, // 0
        {0, 1}  // 1
    };
    
    for (const auto& controlState : possibleStates) {
        int c_v1 = get<0>(controlState);
        int c_v2 = get<1>(controlState);
        
        for (const auto& dataState : possibleStates) {
            int d_v1 = get<0>(dataState);
            int d_v2 = get<1>(dataState);
            
            // Determine output based on control signal
            int out_v1, out_v2;
            if ( (c_v1 == 0 && c_v2 == 1) ) { // Control is 1
                out_v1 = d_v1;
                out_v2 = d_v2;
            } else {
                // Output is Z
                out_v1 = 1;
                out_v2 = 0;
            }
            
            // Create clauses enforcing the output state
            // For gateOutputVar_v1
            string clause_v1 = "";
            clause_v1 += to_string(-funcVar) + " ";
            clause_v1 += to_string(-selVar1) + " ";
            clause_v1 += to_string(-selVar2) + " ";
            clause_v1 += (c_v1 == 1 ? to_string(controlVar_v1) : to_string(-controlVar_v1)) + " ";
            clause_v1 += (c_v2 == 1 ? to_string(controlVar_v2) : to_string(-controlVar_v2)) + " ";
            clause_v1 += (d_v1 == 1 ? to_string(dataVar_v1) : to_string(-dataVar_v1)) + " ";
            clause_v1 += (d_v2 == 1 ? to_string(dataVar_v2) : to_string(-dataVar_v2)) + " ";
            clause_v1 += (out_v1 == 1 ? to_string(gateOutputVar_v1) : to_string(-gateOutputVar_v1)) + " 0";
            clauses.push_back(clause_v1);
            
            // For gateOutputVar_v2
            string clause_v2 = "";
            clause_v2 += to_string(-funcVar) + " ";
            clause_v2 += to_string(-selVar1) + " ";
            clause_v2 += to_string(-selVar2) + " ";
            clause_v2 += (c_v1 == 1 ? to_string(controlVar_v1) : to_string(-controlVar_v1)) + " ";
            clause_v2 += (c_v2 == 1 ? to_string(controlVar_v2) : to_string(-controlVar_v2)) + " ";
            clause_v2 += (d_v1 == 1 ? to_string(dataVar_v1) : to_string(-dataVar_v1)) + " ";
            clause_v2 += (d_v2 == 1 ? to_string(dataVar_v2) : to_string(-dataVar_v2)) + " ";
            clause_v2 += (out_v2 == 1 ? to_string(gateOutputVar_v2) : to_string(-gateOutputVar_v2)) + " 0";
            clauses.push_back(clause_v2);
        }
    }
}

void addJOINCompatibilityConstraints(
    int funcVar, 
    int selVar1, int selVar2, 
    int inputVar1_v1, int inputVar1_v2, 
    int inputVar2_v1, int inputVar2_v2, 
    int gateOutputVar_v1, int gateOutputVar_v2, 
    vector<string>& clauses
) {
    vector<tuple<int, int>> inputStates = {
        {1, 0}, // Z
        {0, 0}, // 0
        {0, 1}  // 1
    };

    // For each combination of input states
    for (const auto& inputState1 : inputStates) {
        int in1_v1 = get<0>(inputState1);
        int in1_v2 = get<1>(inputState1);

        for (const auto& inputState2 : inputStates) {
            int in2_v1 = get<0>(inputState2);
            int in2_v2 = get<1>(inputState2);
            int out_v1, out_v2;
            if ( (in1_v1 == 1 && in1_v2 == 1) || (in2_v1 == 1 && in2_v2 == 1) ) {
                continue; 
            }

            // Based on the truth table
            if (in1_v1 == 1 && in1_v2 == 0) {
                if (in2_v1 == 1 && in2_v2 == 0) {
                    out_v1 = 1; // Z
                    out_v2 = 0;
                } else if ( (in2_v1 == 0 && in2_v2 == 0) || (in2_v1 == 0 && in2_v2 == 1) ) {
                    out_v1 = in2_v1;
                    out_v2 = in2_v2;
                }
            } else if (in1_v1 == 0 && in1_v2 == 0) {
                if (in2_v1 == 1 && in2_v2 == 0) {
                    out_v1 = 0;
                    out_v2 = 0;
                } else if (in2_v1 == 0 && in2_v2 == 0) {
                    out_v1 = 0;
                    out_v2 = 0;
                } else if (in2_v1 == 0 && in2_v2 == 1) {
                    out_v1 = 0;
                    out_v2 = 0;
                }
            } else if (in1_v1 == 0 && in1_v2 == 1) {
                if (in2_v1 == 1 && in2_v2 == 0) {
                    out_v1 = 0;
                    out_v2 = 1;
                } else if (in2_v1 == 0 && in2_v2 == 0) {
                    out_v1 = 0;
                    out_v2 = 1;
                } else if (in2_v1 == 0 && in2_v2 == 1) {
                    out_v1 = 0;
                    out_v2 = 1;
                }
            } else {
                continue;
            }

            // Create clauses enforcing the output state when funcVar and selVars are true
            // For gateOutputVar_v1
            string clause_v1 = "";
            clause_v1 += to_string(-funcVar) + " ";
            clause_v1 += to_string(-selVar1) + " ";
            clause_v1 += to_string(-selVar2) + " ";
            clause_v1 += (in1_v1 == 1 ? to_string(inputVar1_v1) : to_string(-inputVar1_v1)) + " ";
            clause_v1 += (in1_v2 == 1 ? to_string(inputVar1_v2) : to_string(-inputVar1_v2)) + " ";
            clause_v1 += (in2_v1 == 1 ? to_string(inputVar2_v1) : to_string(-inputVar2_v1)) + " ";
            clause_v1 += (in2_v2 == 1 ? to_string(inputVar2_v2) : to_string(-inputVar2_v2)) + " ";
            clause_v1 += (out_v1 == 1 ? to_string(gateOutputVar_v1) : to_string(-gateOutputVar_v1)) + " 0";
            clauses.push_back(clause_v1);

            // For gateOutputVar_v2
            string clause_v2 = "";
            clause_v2 += to_string(-funcVar) + " ";
            clause_v2 += to_string(-selVar1) + " ";
            clause_v2 += to_string(-selVar2) + " ";
            clause_v2 += (in1_v1 == 1 ? to_string(inputVar1_v1) : to_string(-inputVar1_v1)) + " ";
            clause_v2 += (in1_v2 == 1 ? to_string(inputVar1_v2) : to_string(-inputVar1_v2)) + " ";
            clause_v2 += (in2_v1 == 1 ? to_string(inputVar2_v1) : to_string(-inputVar2_v1)) + " ";
            clause_v2 += (in2_v2 == 1 ? to_string(inputVar2_v2) : to_string(-inputVar2_v2)) + " ";
            clause_v2 += (out_v2 == 1 ? to_string(gateOutputVar_v2) : to_string(-gateOutputVar_v2)) + " 0";
            clauses.push_back(clause_v2);
        }
    }
}


void addXORCompatibilityConstraints(
    int funcVar, 
    int selVar1, int selVar2, 
    int inputVar1_v1, int inputVar1_v2, 
    int inputVar2_v1, int inputVar2_v2, 
    int gateOutputVar_v1, int gateOutputVar_v2, 
    vector<string>& clauses
) {

    vector<tuple<int, int>> inputStates = {
        {1, 0}, // Z
        {0, 0}, // 0
        {0, 1}  // 1
    };

    // For each combination of input states
    for (const auto& inputState1 : inputStates) {
        int in1_v1 = get<0>(inputState1);
        int in1_v2 = get<1>(inputState1);

        for (const auto& inputState2 : inputStates) {
            int in2_v1 = get<0>(inputState2);
            int in2_v2 = get<1>(inputState2);

            // Determine the output state based on the XOR truth table
            int out_v1, out_v2;
            // Handle illegal states
            if ( (in1_v1 == 1 && in1_v2 == 1) || (in2_v1 == 1 && in2_v2 == 1) ) {
                continue; // Skip illegal input combinations
            }

            // Based on the truth table
            if (in1_v1 == 1 && in1_v2 == 0) {
                if (in2_v1 == 1 && in2_v2 == 0) {
                    out_v1 = 1; 
                    out_v2 = 0;
                } else if ( (in2_v1 == 0 && in2_v2 == 0) || (in2_v1 == 0 && in2_v2 == 1) ) {
                    out_v1 = 1;
                    out_v2 = 0; 
                }
            } else if ( (in1_v1 == 0 && in1_v2 == 0) || (in1_v1 == 0 && in1_v2 == 1) ) {
                if (in2_v1 == 1 && in2_v2 == 0) {
                    out_v1 = 1;
                    out_v2 = 0; 
                } else if (in2_v1 == 0 && in2_v2 == 0) {
                    out_v1 = 0;
                    out_v2 = 0; 
                } else if (in2_v1 == 0 && in2_v2 == 1) {
                    out_v1 = 0;
                    out_v2 = 1;
                }
            } else {
                continue;
            }

            // Create clauses enforcing the output state when funcVar and selVars are true
            // For gateOutputVar_v1
            string clause_v1 = "";
            clause_v1 += to_string(-funcVar) + " ";
            clause_v1 += to_string(-selVar1) + " ";
            clause_v1 += to_string(-selVar2) + " ";
            clause_v1 += (in1_v1 == 1 ? to_string(inputVar1_v1) : to_string(-inputVar1_v1)) + " ";
            clause_v1 += (in1_v2 == 1 ? to_string(inputVar1_v2) : to_string(-inputVar1_v2)) + " ";
            clause_v1 += (in2_v1 == 1 ? to_string(inputVar2_v1) : to_string(-inputVar2_v1)) + " ";
            clause_v1 += (in2_v2 == 1 ? to_string(inputVar2_v2) : to_string(-inputVar2_v2)) + " ";
            clause_v1 += (out_v1 == 1 ? to_string(gateOutputVar_v1) : to_string(-gateOutputVar_v1)) + " 0";
            clauses.push_back(clause_v1);

            // For gateOutputVar_v2
            string clause_v2 = "";
            clause_v2 += to_string(-funcVar) + " ";
            clause_v2 += to_string(-selVar1) + " ";
            clause_v2 += to_string(-selVar2) + " ";
            clause_v2 += (in1_v1 == 1 ? to_string(inputVar1_v1) : to_string(-inputVar1_v1)) + " ";
            clause_v2 += (in1_v2 == 1 ? to_string(inputVar1_v2) : to_string(-inputVar1_v2)) + " ";
            clause_v2 += (in2_v1 == 1 ? to_string(inputVar2_v1) : to_string(-inputVar2_v1)) + " ";
            clause_v2 += (in2_v2 == 1 ? to_string(inputVar2_v2) : to_string(-inputVar2_v2)) + " ";
            clause_v2 += (out_v2 == 1 ? to_string(gateOutputVar_v2) : to_string(-gateOutputVar_v2)) + " 0";
            clauses.push_back(clause_v2);
        }
    }
}


// MAIN FUNCTION for ENCODING PROCEDURE
// -----------------------------------
// TODO: TEST THIS FUNCTION 
// -----------------------------------
// TODO: COMMENTS NEEDED. DONE 10.15
// -----------------------------------

void encodeSubcircuitAsQBF(const Circuit& subcircuit, const string& filename) {
    ofstream outfile(filename);
    if (!outfile) {
        cerr << "Cannot open the file: " << filename << endl;
        exit(1);
    }

    int varCounter = 0; // Variable counter for assigning unique IDs

    unordered_set<int> inputVars;        // Input variables (x_t)
    unordered_set<int> gateValueVars;    // Gate value variables (g_t)
    vector<int> selectionVars;           // Selection variables (s_{it})
    vector<int> gateFunctionVars;        // Gate definition variables (f_{i,a1a2})
    unordered_set<int> outputVars;       // Output variables (o_{tj})

    unordered_map<int, WireVars> wireVarMap; // Maps wire IDs to WireVars
    unordered_map<pair<int, int>, int, pair_hash> selectionVarMap; // Maps (gateIndex * maxNumInputPins + inputPin, t) to selection variable ID
    unordered_map<pair<int, GateType>, int, pair_hash> gateFunctionVarMap; // Maps (gateIndex, GateType) to function variable ID

    int n = subcircuit.numInputs;
    cout << "Number of inputs: " << n << endl;
    int numOutputs = subcircuit.numOutputs;
    cout << "Number of outputs: " << numOutputs << endl;
    int numGates = subcircuit.gates.size();
    cout << "Number of gates: " << numGates << endl;
    vector<GateType> possibleFunctions = {XOR, BUFFER, JOIN, CONST_ZERO, CONST_ONE};

    // Function to get the number of inputs for each gate type
    // TODO: For buffer, index the control wire.
    auto getNumInputs = [](GateType type) -> int {
        switch (type) {
            case BUFFER:
            case JOIN:
            case XOR:
                return 2;
            case CONST_ZERO:
            case CONST_ONE:
                return 0;
            default:
                return 0;
        }
    };

    // Collect wire IDs for primary inputs
    vector<int> possibleInputs;
    for (int i = 0; i < n; ++i) {
        possibleInputs.push_back(i); // Assuming wire IDs for inputs are 0 to n-1
        WireVars vars;
        vars.v1 = ++varCounter;
        inputVars.insert(vars.v1);
        vars.v2 = ++varCounter;
        inputVars.insert(vars.v2);
        wireVarMap[i] = vars;
    }

    // gate outputs (gate value variables)
    for (int i = 0; i < numGates; ++i) {
        int gateOutputWireID = subcircuit.gates[i].output;
        if (wireVarMap.find(gateOutputWireID) == wireVarMap.end()) {
            WireVars vars;
            vars.v1 = ++varCounter;
            vars.v2 = ++varCounter;
            gateValueVars.insert(vars.v1);
            gateValueVars.insert(vars.v2);
            wireVarMap[gateOutputWireID] = vars;
        }
        // Add gate output wire to possible inputs for subsequent gates
        possibleInputs.push_back(gateOutputWireID);
    }

    // selection variables (s_{it})
    int maxNumInputPins = 2; // Maximum number of inputs any gate can have, 2 by default..... Need to change for buffer
    for (int i = 0; i < numGates; ++i) {
        int numPins = getNumInputs(subcircuit.gates[i].type);
        if (numPins == 0) {
            continue; 
        }
        for (int inputPin = 0; inputPin < numPins; ++inputPin) {
            for (int t = 0; t < possibleInputs.size(); ++t) {
                int varID = ++varCounter;
                selectionVars.push_back(varID);
                selectionVarMap[{i * maxNumInputPins + inputPin, t}] = varID;
            }
        }
    }

    // Afunction variables (f_{i,a1a2})
    for (int i = 0; i < numGates; ++i) {
        for (const auto& funcType : possibleFunctions) {
            int varID = ++varCounter;
            gateFunctionVars.push_back(varID);
            gateFunctionVarMap[{i, funcType}] = varID;
        }
    }

    // output variables (o_{tj})
    vector<int> outputWireIDs;
    for (int i = numGates - numOutputs; i < numGates; ++i) {
        int outputWireID = subcircuit.gates[i].output;
        outputWireIDs.push_back(outputWireID);
        // Outputs are already in wireVarMap
        outputVars.insert(wireVarMap[outputWireID].v1);
        outputVars.insert(wireVarMap[outputWireID].v2);
    }

    outfile << "p cnf " << varCounter << " CLAUSE_COUNT_PLACEHOLDER" << endl;

    // Universal quantification for input variables (x_t)
    // Currently, universal quantification contains all other variables

    // -------------------------------------
    // TODO: Need verification if universal quantification is correct, NO? 10.15
    // -------------------------------------

    outfile << "a ";
    for (int var : inputVars) {
        outfile << var << " ";
    }
    outfile << "0" << endl;

    // Existential quantification for other variables
    outfile << "e ";
    for (int var = 1; var <= varCounter; ++var) {
        if (inputVars.find(var) == inputVars.end()) {
            outfile << var << " ";
        }
    }
    outfile << "0" << endl;

    vector<string> clauses;

    // 1. no wire in the illegal state
    for (const auto& entry : wireVarMap) {
        int v1 = entry.second.v1;
        int v2 = entry.second.v2;
        // Clause: -v1 ∨ -v2 (at least one of v1 or v2 is 0)
        clauses.push_back(to_string(-v1) + " " + to_string(-v2) + " 0");
    }

    // 2. exactly one selection variable is true
    for (int i = 0; i < numGates; ++i) {
        int numPins = getNumInputs(subcircuit.gates[i].type);
        if (numPins == 0) {
            continue; 
        }
        for (int inputPin = 0; inputPin < numPins; ++inputPin) {
            vector<int> gateSelectionVars;
            for (int t = 0; t < possibleInputs.size(); ++t) {
                int selVar = selectionVarMap[{i * maxNumInputPins + inputPin, t}];
                gateSelectionVars.push_back(selVar);
            }
            // Add constraints that exactly one selection variable is true
            addExactlyOneConstraint(gateSelectionVars, clauses);
        }
    }

    // 3. exactly one function is selected
    for (int i = 0; i < numGates; ++i) {
        vector<int> gateFuncVars;
        for (const auto& funcType : possibleFunctions) {
            int funcVar = gateFunctionVarMap[{i, funcType}];
            gateFuncVars.push_back(funcVar);
        }
        // Add constraint that exactly one function variable is true
        addExactlyOneConstraint(gateFuncVars, clauses);
    }

    // 4. gate outputs are consistent with selected inputs and functions
    for (int i = 0; i < numGates; ++i) {
        const Gate& gate = subcircuit.gates[i];
        int gateOutputWireID = gate.output;
        WireVars gateOutputVars = wireVarMap[gateOutputWireID];

        int numPins = getNumInputs(gate.type);

        if (numPins == 0) {
            for (const auto& funcType : possibleFunctions) {
                int funcVar = gateFunctionVarMap[{i, funcType}];

                if (funcType == CONST_ZERO || funcType == CONST_ONE) {
                    addConstGateCompatibilityConstraints(
                        funcVar,
                        funcType,
                        gateOutputVars.v1,
                        gateOutputVars.v2,
                        clauses
                    );
                }
            }
        } else if (numPins == 2) {
            for (int t1 = 0; t1 < possibleInputs.size(); ++t1) {
                int selVar1 = selectionVarMap[{i * maxNumInputPins, t1}];
                WireVars inputVars1 = wireVarMap[possibleInputs[t1]];

                for (int t2 = 0; t2 < possibleInputs.size(); ++t2) {
                    int selVar2 = selectionVarMap[{i * maxNumInputPins + 1, t2}];
                    WireVars inputVars2 = wireVarMap[possibleInputs[t2]];

                    for (const auto& funcType : possibleFunctions) {
                        int funcVar = gateFunctionVarMap[{i, funcType}];

                        if (funcType == BUFFER) {
                            addBUFFERCompatibilityConstraints(
                                funcVar, selVar1, selVar2,
                                inputVars1.v1, inputVars1.v2,
                                inputVars2.v1, inputVars2.v2,
                                gateOutputVars.v1, gateOutputVars.v2,
                                clauses
                            );
                        } else if (funcType == XOR) {
                            addXORCompatibilityConstraints(
                                funcVar, selVar1, selVar2,
                                inputVars1.v1, inputVars1.v2,
                                inputVars2.v1, inputVars2.v2,
                                gateOutputVars.v1, gateOutputVars.v2,
                                clauses
                            );
                        } else if (funcType == JOIN) {
                            addJOINCompatibilityConstraints(
                                funcVar, selVar1, selVar2,
                                inputVars1.v1, inputVars1.v2,
                                inputVars2.v1, inputVars2.v2,
                                gateOutputVars.v1, gateOutputVars.v2,
                                clauses
                            );
                        }
                    }
                }
            }
        } 
    }

    // 5. acyclicity
    for (int i = 0; i < numGates; ++i) {
        int numPins = getNumInputs(subcircuit.gates[i].type);
        if (numPins == 0) {
            continue; 
        }
        for (int inputPin = 0; inputPin < numPins; ++inputPin) {
            for (int t = 0; t < possibleInputs.size(); ++t) {
                int inputWireID = possibleInputs[t];
                bool invalidInput = false;
                for (int j = i; j < numGates; ++j) {
                    if (subcircuit.gates[j].output == inputWireID) {
                        invalidInput = true;
                        break;
                    }
                }
                if (invalidInput) {
                    // Add clause to prevent selection of this input
                    int selVar = selectionVarMap[{i * maxNumInputPins + inputPin, t}];
                    clauses.push_back(to_string(-selVar) + " 0");
                }
            }
        }
    }

    // 6. symmetry breaking
    // Manually check acyclicity of the circuit and add symmetry breaking constraints
    for (int i = 1; i < numGates; ++i) {
        for (const auto& funcTypePrev : possibleFunctions) {
            for (const auto& funcTypeCurr : possibleFunctions) {
                if (funcTypeCurr < funcTypePrev) {
                    int funcVarPrev = gateFunctionVarMap[{i - 1, funcTypePrev}];
                    int funcVarCurr = gateFunctionVarMap[{i, funcTypeCurr}];
                    // Add constraint: -(funcVarPrev) ∨ -(funcVarCurr)
                    clauses.push_back(to_string(-funcVarPrev) + " " + to_string(-funcVarCurr) + " 0");
                }
            }
        }
    }

    // output
    int totalClauses = clauses.size();
    for (const string& clause : clauses) {
        outfile << clause << endl;
    }

    outfile.seekp(0, ios::beg);
    outfile << "p cnf " << varCounter << " " << totalClauses << endl;

    outfile.close();
}




int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: <input_circuit_file>" << std::endl;
        return 1;
    }
    Circuit circuit = readCircuit(argv[1]);

    int windowSize = 7; // Define the window size as needed
    vector<Circuit> subcircuits = partitionCircuit(circuit, windowSize);

    for (size_t i = 0; i < subcircuits.size(); ++i) {
        string qbfFilename = "./qbf/subcircuit_" + to_string(i + 1) + ".qdimacs";
        encodeSubcircuitAsQBF(subcircuits[i], qbfFilename);
        cout << "Subcircuit " << i + 1 << " has been written to " << qbfFilename << endl;
    }

    return 0;
}
