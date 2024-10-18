#include "qbfencoder.h"
#include <fstream>
#include <iostream>
#include <tuple>

using namespace std;

int addExactlyOneConstraint(const vector<int>& vars, vector<string>& clauses) {
    // At least one variable is true
    string atLeastOneClause;
    int initialClauseCount = clauses.size();
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
    return clauses.size() - initialClauseCount;
}

int addConstGateCompatibilityConstraints(
    int funcVar,
    GateType funcType,
    int gateOutputVar_v1, int gateOutputVar_v2,
    vector<string>& clauses
) {
    int initialClauseCount = clauses.size();
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
    return clauses.size() - initialClauseCount;
}

int addBUFFERCompatibilityConstraints(
    int funcVar, 
    int selVar1, int selVar2,  
    int controlVar_v1, int controlVar_v2, 
    int dataVar_v1, int dataVar_v2, 
    int gateOutputVar_v1, int gateOutputVar_v2, 
    vector<string>& clauses
) {
    int initialClauseCount = clauses.size();
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
            string clause_v1 = to_string(-funcVar) + " " + to_string(-selVar1) + " " + to_string(-selVar2) + " ";
            clause_v1 += (c_v1 == 1 ? to_string(controlVar_v1) : to_string(-controlVar_v1)) + " ";
            clause_v1 += (c_v2 == 1 ? to_string(controlVar_v2) : to_string(-controlVar_v2)) + " ";
            clause_v1 += (d_v1 == 1 ? to_string(dataVar_v1) : to_string(-dataVar_v1)) + " ";
            clause_v1 += (d_v2 == 1 ? to_string(dataVar_v2) : to_string(-dataVar_v2)) + " ";
            clause_v1 += (out_v1 == 1 ? to_string(gateOutputVar_v1) : to_string(-gateOutputVar_v1)) + " 0";
            clauses.push_back(clause_v1);
            
            string clause_v2 = to_string(-funcVar) + " " + to_string(-selVar1) + " " + to_string(-selVar2) + " ";
            clause_v2 += (c_v1 == 1 ? to_string(controlVar_v1) : to_string(-controlVar_v1)) + " ";
            clause_v2 += (c_v2 == 1 ? to_string(controlVar_v2) : to_string(-controlVar_v2)) + " ";
            clause_v2 += (d_v1 == 1 ? to_string(dataVar_v1) : to_string(-dataVar_v1)) + " ";
            clause_v2 += (d_v2 == 1 ? to_string(dataVar_v2) : to_string(-dataVar_v2)) + " ";
            clause_v2 += (out_v2 == 1 ? to_string(gateOutputVar_v2) : to_string(-gateOutputVar_v2)) + " 0";
            clauses.push_back(clause_v2);
        }
    }
    return clauses.size() - initialClauseCount;
}

int addJOINCompatibilityConstraints(
    int funcVar, 
    int selVar1, int selVar2, 
    int inputVar1_v1, int inputVar1_v2, 
    int inputVar2_v1, int inputVar2_v2, 
    int gateOutputVar_v1, int gateOutputVar_v2, 
    vector<string>& clauses
) {

    int initialClauseCount = clauses.size();
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

            // Create clauses enforcing the output state
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
    return clauses.size() - initialClauseCount;
}

int addXORCompatibilityConstraints(
    int funcVar, 
    int selVar1, int selVar2, 
    int inputVar1_v1, int inputVar1_v2, 
    int inputVar2_v1, int inputVar2_v2, 
    int gateOutputVar_v1, int gateOutputVar_v2, 
    vector<string>& clauses
) {
    int initialClauseCount = clauses.size();
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

            // Create clauses enforcing the output state
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
    return clauses.size() - initialClauseCount;
}

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
    int numOutputs = subcircuit.numOutputs;
    int numGates = subcircuit.gates.size();
    vector<GateType> possibleFunctions = {XOR, BUFFER, JOIN, CONST_ZERO, CONST_ONE};

    // Initialize counters for clauses
    int numNoIllegalStateClauses = 0;
    int numSelectionConstraints = 0;
    int numFunctionConstraints = 0;
    int numGateConsistencyConstraints = 0;
    int numAcyclicityConstraints = 0;
    int numSymmetryBreakingConstraints = 0;

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
        int wireID = subcircuit.inputWires[i];
        possibleInputs.push_back(wireID);
        WireVars vars;
        vars.v1 = ++varCounter;
        inputVars.insert(vars.v1);
        vars.v2 = ++varCounter;
        inputVars.insert(vars.v2);
        wireVarMap[wireID] = vars;
    }

    // Gate outputs
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

    // Selection variables (s_{it})
    int maxNumInputPins = 2; // Maximum number of inputs any gate can have
    for (int i = 0; i < numGates; ++i) {
        int numPins = getNumInputs(subcircuit.gates[i].type);
        if (numPins == 0) {
            continue; 
        }
        for (int inputPin = 0; inputPin < numPins; ++inputPin) {
            vector<int> gateSelectionVars;
            for (int t = 0; t < possibleInputs.size(); ++t) {
                int varID = ++varCounter;
                selectionVars.push_back(varID);
                selectionVarMap[{i * maxNumInputPins + inputPin, t}] = varID;
            }
        }
    }

    // Function variables (f_{i,a1a2})
    for (int i = 0; i < numGates; ++i) {
        for (const auto& funcType : possibleFunctions) {
            int varID = ++varCounter;
            gateFunctionVars.push_back(varID);
            gateFunctionVarMap[{i, funcType}] = varID;
        }
    }

    // Output variables (o_{tj})
    for (int i = 0; i < numOutputs; ++i) {
        int outputWireID = subcircuit.outputWires[i];
        outputVars.insert(wireVarMap[outputWireID].v1);
        outputVars.insert(wireVarMap[outputWireID].v2);
    }

    outfile << "p cnf " << varCounter << " CLAUSE_COUNT_PLACEHOLDER" << endl;

    // Universal quantification for input variables (x_t)
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

    // 1. No wire in the illegal state
    for (const auto& entry : wireVarMap) {
        int v1 = entry.second.v1;
        int v2 = entry.second.v2;
        // Clause: -v1 ∨ -v2 (at least one of v1 or v2 is 0)
        clauses.push_back(to_string(-v1) + " " + to_string(-v2) + " 0");
        numNoIllegalStateClauses++;
    }

    // 2. Exactly one selection variable is true
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
            numSelectionConstraints += addExactlyOneConstraint(gateSelectionVars, clauses);
        }
    }

    // 3. Exactly one function is selected
    for (int i = 0; i < numGates; ++i) {
        vector<int> gateFuncVars;
        for (const auto& funcType : possibleFunctions) {
            int funcVar = gateFunctionVarMap[{i, funcType}];
            gateFuncVars.push_back(funcVar);
        }
        // Add constraint that exactly one function variable is true
        numFunctionConstraints += addExactlyOneConstraint(gateFuncVars, clauses);
    }

    // 4. Gate outputs are consistent with selected inputs and functions
    for (int i = 0; i < numGates; ++i) {
        const Gate& gate = subcircuit.gates[i];
        int gateOutputWireID = gate.output;
        WireVars gateOutputVars = wireVarMap[gateOutputWireID];

        int numPins = getNumInputs(gate.type);

        if (numPins == 0) {
            for (const auto& funcType : possibleFunctions) {
                int funcVar = gateFunctionVarMap[{i, funcType}];

                if (funcType == CONST_ZERO || funcType == CONST_ONE) {
                    numGateConsistencyConstraints += addConstGateCompatibilityConstraints(
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
                            numGateConsistencyConstraints += addBUFFERCompatibilityConstraints(
                                funcVar, selVar1, selVar2,
                                inputVars1.v1, inputVars1.v2,
                                inputVars2.v1, inputVars2.v2,
                                gateOutputVars.v1, gateOutputVars.v2,
                                clauses
                            );
                        } else if (funcType == XOR) {
                            numGateConsistencyConstraints += addXORCompatibilityConstraints(
                                funcVar, selVar1, selVar2,
                                inputVars1.v1, inputVars1.v2,
                                inputVars2.v1, inputVars2.v2,
                                gateOutputVars.v1, gateOutputVars.v2,
                                clauses
                            );
                        } else if (funcType == JOIN) {
                            numGateConsistencyConstraints += addJOINCompatibilityConstraints(
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

    // 5. Acyclicity
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
                    numAcyclicityConstraints++;
                }
            }
        }
    }

    // 6. Symmetry breaking
    for (int i = 1; i < numGates; ++i) {
        for (const auto& funcTypePrev : possibleFunctions) {
            for (const auto& funcTypeCurr : possibleFunctions) {
                if (funcTypeCurr < funcTypePrev) {
                    int funcVarPrev = gateFunctionVarMap[{i - 1, funcTypePrev}];
                    int funcVarCurr = gateFunctionVarMap[{i, funcTypeCurr}];
                    // Add constraint: -(funcVarPrev) ∨ -(funcVarCurr)
                    clauses.push_back(to_string(-funcVarPrev) + " " + to_string(-funcVarCurr) + " 0");
                    numSymmetryBreakingConstraints++;
                }
            }
        }
    }

    // Output the clauses
    int totalClauses = clauses.size();
    for (const string& clause : clauses) {
        outfile << clause << endl;
    }

    // Output counts
    cout << "Subcircuit has " << n << " input wires and " << numGates << " gates." << endl;
    cout << "Initial possibleInputs size: " << possibleInputs.size() << endl;
    cout << "Number of 'no illegal state' clauses: " << numNoIllegalStateClauses << endl;
    cout << "Number of 'selection variable' constraints: " << numSelectionConstraints << endl;
    cout << "Number of 'function variable' constraints: " << numFunctionConstraints << endl;
    cout << "Number of 'gate consistency' constraints: " << numGateConsistencyConstraints << endl;
    cout << "Number of 'acyclicity' constraints: " << numAcyclicityConstraints << endl;
    cout << "Number of 'symmetry breaking' constraints: " << numSymmetryBreakingConstraints << endl;
    cout << "Total clauses: " << totalClauses << endl;

    // Update the header line with the correct number of clauses
    outfile.seekp(0, ios::beg);
    outfile << "p cnf " << varCounter << " " << totalClauses << endl;

    outfile.close();
}
