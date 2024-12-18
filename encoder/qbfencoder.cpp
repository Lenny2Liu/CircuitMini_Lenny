#include "qbfencoder.h"
#include <fstream>
#include <iostream>
#include <tuple>
#include <set>
#include <sstream>

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

int addJoinMinimizationConstraints(
    const vector<int>& joinGateVars,
    int maxAllowedJoins,
    vector<string>& clauses) {
    int initialClauseCount = clauses.size();
    
    // Add cardinality constraint to limit number of JOIN gates
    // We'll use sequential counter encoding for this
    int n = joinGateVars.size();
    
    // Create sequential counter variables
    vector<vector<int>> s(n + 1, vector<int>(maxAllowedJoins + 1));
    int nextVar = 1;
    for(int i = 0; i <= n; i++) {
        for(int j = 0; j <= maxAllowedJoins; j++) {
            if(i == 0 || j == 0) {
                s[i][j] = -1;  // dummy value
            } else {
                s[i][j] = ++nextVar;
            }
        }
    }
    
    // Add sequential counter clauses
    for(int i = 1; i <= n; i++) {
        // First bit
        clauses.push_back(to_string(-joinGateVars[i-1]) + " " + 
                         to_string(s[i][1]) + " 0");
                         
        // Propagate bits
        for(int j = 1; j < maxAllowedJoins; j++) {
            // s[i][j] → s[i+1][j]
            clauses.push_back(to_string(-s[i][j]) + " " + 
                            to_string(s[i+1][j]) + " 0");
            
            // (x[i] ∧ s[i][j]) → s[i+1][j+1]
            clauses.push_back(to_string(-joinGateVars[i-1]) + " " + 
                            to_string(-s[i][j]) + " " + 
                            to_string(s[i+1][j+1]) + " 0");
        }
        
        // Last bit
        clauses.push_back(to_string(-joinGateVars[i-1]) + " " + 
                         to_string(-s[i][maxAllowedJoins]) + " 0");
    }
    
    // For each k from 1 to maxAllowedJoins-1, add clauses that
    // prefer k joins over k+1 joins when possible
    for(int k = 1; k < maxAllowedJoins; k++) {
        string preference_clause;
        for(int i = 1; i <= n; i++) {
            preference_clause += to_string(-s[i][k+1]) + " ";
        }
        preference_clause += "0";
        clauses.push_back(preference_clause);
    }

    return clauses.size() - initialClauseCount;
}




vector<int> simulateOriginalCircuit(
    const Circuit& originalCircuit,
    const vector<int>& inputAssignment)
{
    unordered_map<int, int> wireValues; // Wire ID to value mapping
    
    // Initialize input wires
    for (size_t i = 0; i < originalCircuit.inputWires.size(); ++i) {
        int wireID = originalCircuit.inputWires[i];
        wireValues[wireID] = inputAssignment[i];
        // cout << "Input wire id " << wireID << " index " << i  << " value: " << inputAssignment[i] << endl;
    }

    // Build dependency graph and in-degree count
    unordered_map<int, vector<int>> graph; // gate index -> dependent gate indices
    vector<int> inDegree(originalCircuit.gates.size(), 0);
    
    for (size_t i = 0; i < originalCircuit.gates.size(); ++i) {
        const Gate& gate = originalCircuit.gates[i];
        
        // Skip constant gates as they have no dependencies
        if (gate.type == CONST_ZERO || gate.type == CONST_ONE) {
            continue;
        }
        
        // For each input wire, find which gate produces it
        for (int inputWire : {gate.input1, gate.input2}) {
            if (inputWire >= 0) { // Skip invalid inputs (-1)
                // Find which gate produces this input wire
                for (size_t j = 0; j < originalCircuit.gates.size(); ++j) {
                    if (originalCircuit.gates[j].output == inputWire) {
                        graph[j].push_back(i);
                        inDegree[i]++;
                    }
                }
            }
        }
    }

    // Topological sort using queue
    queue<int> q;
    vector<int> evaluationOrder;
    
    // Add all gates with no dependencies to queue
    for (size_t i = 0; i < originalCircuit.gates.size(); ++i) {
        if (inDegree[i] == 0) {
            q.push(i);
        }
    }

    // Process queue
    while (!q.empty()) {
        int current = q.front();
        q.pop();
        evaluationOrder.push_back(current);

        // Update dependencies
        for (int next : graph[current]) {
            inDegree[next]--;
            if (inDegree[next] == 0) {
                q.push(next);
            }
        }
    }

    // Now simulate gates in topological order
    for (int gateIdx : evaluationOrder) {
        const Gate& gate = originalCircuit.gates[gateIdx];
        int outputWire = gate.output;

        if (gate.type == CONST_ZERO) {
            wireValues[outputWire] = 0;
        } else if (gate.type == CONST_ONE) {
            wireValues[outputWire] = 1;
        } else if (gate.type == BUFFER) {
            int data = wireValues[gate.input2];
            int control = wireValues[gate.input1];
            int output;
            if (control == 1) {
                output = data;
            } else {
                output = 2; // Z state
            }
            wireValues[outputWire] = output;
        } else if (gate.type == JOIN) {
            int in1 = wireValues[gate.input1];
            int in2 = wireValues[gate.input2];
            int output;
            if (in1 == 2 && in2 == 2) {
                output = 2; // Both Z
            } else if (in1 == 2) {
                output = in2;
            } else if (in2 == 2) {
                output = in1;
            } else if (in1 == in2) {
                output = in1;
            } else {
                output = -1; // Invalid state
            }
            wireValues[outputWire] = output;
        } else if (gate.type == XOR) {
            int in1 = wireValues[gate.input1];
            int in2 = wireValues[gate.input2];
            int output;
            if (in1 == 2 || in2 == 2) {
                output = 2;
            } else {
                output = in1 ^ in2;
            }
            wireValues[outputWire] = output;
        }

        // cout << "Gate input id " << gate.input1 << " " << gate.input2 
        //      << " value " << wireValues[gate.input1] << " " << wireValues[gate.input2] << endl;
        // cout << "Gate output wire, value " << outputWire << " " << wireValues[outputWire] << endl;
    }

    // Collect the values of all output wires
    vector<int> outputValues;
    for (int outputWireID : originalCircuit.outputWires) {
        outputValues.push_back(wireValues[outputWireID]);
        // cout << "OUTPUT WIRE ID " << outputWireID << endl;
    }

    return outputValues;
}


// Function to add input-output constraints to the clauses
void addInputOutputConstraint(
    const vector<int>& inputAssignment,
    int expectedOutput,
    const vector<int>& inputVars_v1,
    const vector<int>& inputVars_v2,
    int optOutputVar_v1,
    int optOutputVar_v2,
    vector<string>& clauses)
{
    // Build the input assignment clause
    vector<int> inputClauseVars;
    for (size_t i = 0; i < inputAssignment.size(); ++i) {
        int v1 = inputVars_v1[i];
        int v2 = inputVars_v2[i];
        int val = inputAssignment[i];

        if (val == 0) {
            // Input is 0: (-v1 ∧ -v2)
            inputClauseVars.push_back(-v1);
            inputClauseVars.push_back(-v2);
        } else if (val == 1) {
            // Input is 1: (-v1 ∧ v2)
            inputClauseVars.push_back(-v1);
            inputClauseVars.push_back(v2);
        } else if (val == 2) {
            // Input is Z: (v1 ∧ -v2)
            inputClauseVars.push_back(v1);
            inputClauseVars.push_back(-v2);
        }
    }

    vector<int> outputClauseVars;
    if (expectedOutput == 0) {
        // Output is 0: (-out_v1 ∧ -out_v2)
        outputClauseVars.push_back(-optOutputVar_v1);
        outputClauseVars.push_back(-optOutputVar_v2);
    } else if (expectedOutput == 1) {
        // Output is 1: (-out_v1 ∧ out_v2)
        outputClauseVars.push_back(-optOutputVar_v1);
        outputClauseVars.push_back(optOutputVar_v2);
    } else if (expectedOutput == 2) {
        // Output is Z: (out_v1 ∧ -out_v2)
        outputClauseVars.push_back(optOutputVar_v1);
        outputClauseVars.push_back(-optOutputVar_v2);
    } else if (expectedOutput == -1) {
        // outputClauseVars.push_back(optOutputVar_v1);
        // outputClauseVars.push_back(optOutputVar_v2);
        return;
    }

    // Negate the input assignment for implication
    vector<int> negatedInputClauseVars;
    for (int var : inputClauseVars) {
        negatedInputClauseVars.push_back(-var);
    }

    // Build the clause: (negated inputs ∨ outputs)
    string clause;
    for (int var : negatedInputClauseVars) {
        clause += to_string(var) + " ";
    }
    for (int var : outputClauseVars) {
        // cout << "var: " << var << endl;
        string clause_1 = clause + to_string(var) + " 0";
        // cout << "clause: " << clause << endl;
        clauses.push_back(clause_1);
    }
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
    int outputSelVar,         
    int dataVar_v1, int dataVar_v2,      
    int controlVar_v1, int controlVar_v2, 
    int outVar_v1, int outVar_v2,      
    vector<string>& clauses) 
{
    int initialClauseCount = clauses.size();
    string prefix = to_string(-funcVar) + " " + 
                   to_string(-selVar1) + " " + 
                   to_string(-selVar2) + " " +
                   to_string(-outputSelVar) + " ";
    // 1. control = Z (v1=1,v2=0) -> output = Z
    clauses.push_back(prefix + to_string(-controlVar_v1) + " " + to_string(controlVar_v2) + " " +
                to_string(-dataVar_v1) + " " + to_string(dataVar_v2) + " " +
                to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(-controlVar_v1) + " " + to_string(controlVar_v2) + " " +
                to_string(dataVar_v1) + " " + to_string(dataVar_v2) + " " +
                to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(-controlVar_v1) + " " + to_string(controlVar_v2) + " " +
                to_string(dataVar_v1) + " " + to_string(-dataVar_v2) + " " +
                to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(-controlVar_v1) + " " + to_string(controlVar_v2) + " " +
                to_string(-dataVar_v1) + " " + to_string(dataVar_v2) + " " +
                to_string(-outVar_v2) + " 0");
    clauses.push_back(prefix + to_string(-controlVar_v1) + " " + to_string(controlVar_v2) + " " +
                to_string(dataVar_v1) + " " + to_string(dataVar_v2) + " " +
                to_string(-outVar_v2) + " 0");
    clauses.push_back(prefix + to_string(-controlVar_v1) + " " + to_string(controlVar_v2) + " " +
                to_string(dataVar_v1) + " " + to_string(-dataVar_v2) + " " +
                to_string(-outVar_v2) + " 0");
    // 2. control = 0 (v1=0,v2=0) -> output = Z
    clauses.push_back(prefix + to_string(controlVar_v1) + " " + to_string(controlVar_v2) + " " +
                to_string(-dataVar_v1) + " " + to_string(dataVar_v2) + " " +
                to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(controlVar_v1) + " " + to_string(controlVar_v2) + " " +
                to_string(dataVar_v1) + " " + to_string(dataVar_v2) + " " +
                to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(controlVar_v1) + " " + to_string(controlVar_v2) + " " +
                to_string(dataVar_v1) + " " + to_string(-dataVar_v2) + " " +
                to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(controlVar_v1) + " " + to_string(controlVar_v2) + " " +
                to_string(-dataVar_v1) + " " + to_string(dataVar_v2) + " " +
                to_string(-outVar_v2) + " 0");
    clauses.push_back(prefix + to_string(controlVar_v1) + " " + to_string(controlVar_v2) + " " +
                to_string(dataVar_v1) + " " + to_string(-dataVar_v2) + " " +
                to_string(-outVar_v2) + " 0");
    clauses.push_back(prefix + to_string(controlVar_v1) + " " + to_string(controlVar_v2) + " " +
                to_string(dataVar_v1) + " " + to_string(dataVar_v2) + " " +
                to_string(-outVar_v2) + " 0");
    // 3. control = 1 (v1=0,v2=1):
    // 3.1 data = Z -> output = Z
    clauses.push_back(prefix + to_string(controlVar_v1) + " " + to_string(-controlVar_v2) + " " +
                to_string(-dataVar_v1) + " " + to_string(dataVar_v2) + " " +
                to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(controlVar_v1) + " " + to_string(-controlVar_v2) + " " +
                to_string(-dataVar_v1) + " " + to_string(dataVar_v2) + " " +
                to_string(-outVar_v2) + " 0");

    // 3.2 data = 0 (v1=0,v2=0) -> output = 0
    clauses.push_back(prefix + to_string(controlVar_v1) + " " + to_string(-controlVar_v2) + " " +
                to_string(dataVar_v1) + " " + to_string(dataVar_v2) + " " +
                to_string(-outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(controlVar_v1) + " " + to_string(-controlVar_v2) + " " +
                to_string(dataVar_v1) + " " + to_string(dataVar_v2) + " " +
                to_string(-outVar_v2) + " 0");

    // 3.3 data = 1 (v1=0,v2=1) -> output = 1
    clauses.push_back(prefix + to_string(controlVar_v1) + " " + to_string(-controlVar_v2) + " " +
                to_string(dataVar_v1) + " " + to_string(-dataVar_v2) + " " +
                to_string(-outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(controlVar_v1) + " " + to_string(-controlVar_v2) + " " +
                to_string(dataVar_v1) + " " + to_string(-dataVar_v2) + " " +
                to_string(outVar_v2) + " 0");
\
    return clauses.size() - initialClauseCount;
}


int addJOINCompatibilityConstraints(
    int funcVar, 
    int selVar1, int selVar2,  
    int outputSelVar,       
    int in1Var_v1, int in1Var_v2,  
    int in2Var_v1, int in2Var_v2,  
    int outVar_v1, int outVar_v2,  
    vector<string>& clauses)
{
    int initialClauseCount = clauses.size();

    string prefix = to_string(-funcVar) + " " + 
                   to_string(-selVar1) + " " + 
                   to_string(-selVar2) + " " +
                   to_string(-outputSelVar) + " ";
    // 1. 两个输入都是Z -> 输出为Z
    clauses.push_back(prefix + to_string(-in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(-in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(-in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(-in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(-outVar_v2) + " 0");

    // 2. input1为Z，input2为非Z时，output = input2
    // 2.1 input2 = 0
    clauses.push_back(prefix + to_string(-in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(-outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(-in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(-outVar_v2) + " 0");
    // 2.2 input2 = 1
    clauses.push_back(prefix + to_string(-in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(-in2Var_v2) + " " +
                     to_string(-outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(-in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(-in2Var_v2) + " " +
                     to_string(outVar_v2) + " 0");
    // 3. input2为Z，input1为非Z时，output = input1
    // 3.1 input1 = 0
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(-in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(-outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(-in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(-outVar_v2) + " 0");

    // 3.2 input1 = 1
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(-in1Var_v2) + " " +
                     to_string(-in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(-outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(-in1Var_v2) + " " +
                     to_string(-in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(outVar_v2) + " 0");

    // 4. 都不为Z时
    // 4.1 都为0 -> output = 0
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(-outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(-outVar_v2) + " 0");

    // 4.2 都为1 -> output = 1
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(-in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(-in2Var_v2) + " " +
                     to_string(-outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(-in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(-in2Var_v2) + " " +
                     to_string(outVar_v2) + " 0");

    // 5. 输入冲突（一个0一个1）是非法的
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                  to_string(in2Var_v1) + " " + to_string(-in2Var_v2) + " " +
                  to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                  to_string(in2Var_v1) + " " + to_string(-in2Var_v2) + " " +
                  to_string(outVar_v2) + " 0");

    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(-in1Var_v2) + " " +
                  to_string(in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                  to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(-in1Var_v2) + " " +
                  to_string(in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                  to_string(outVar_v2) + " 0");
    // clauses.push_back("FINISH JOIN_CONSTRAINTS");
    return clauses.size() - initialClauseCount;
}


int addXORCompatibilityConstraints(
    int funcVar, 
    int selVar1, int selVar2,  
    int outputSelVar,  
    int in1Var_v1, int in1Var_v2,
    int in2Var_v1, int in2Var_v2,
    int outVar_v1, int outVar_v2,
    vector<string>& clauses)
{
    int initialClauseCount = clauses.size();
    
    string prefix = to_string(-funcVar) + " " + 
                   to_string(-selVar1) + " " + 
                   to_string(-selVar2) + " " +
                   to_string(-outputSelVar) + " ";
    // clauses.push_back(to_string(-funcVar) + " 0");
    // clauses.push_back(to_string(-selVar1) + " 0");
    // clauses.push_back(to_string(-selVar2) + " 0");
    // clauses.push_back(to_string(-outputSelVar) + " 0");
    // 1. 如果任一输入是Z，输出为Z
    clauses.push_back(prefix + to_string(-in1Var_v1) + " " + to_string(in1Var_v2) + " " + 
                to_string(in2Var_v1) + " " + to_string(-in2Var_v2) + " " +
                to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(-in1Var_v1) + " " + to_string(in1Var_v2) + " " + 
                to_string(in2Var_v1) + " " + to_string(-in2Var_v2) + " " +
                to_string(-outVar_v2) + " 0");
    clauses.push_back(prefix + to_string(-in1Var_v1) + " " + to_string(in1Var_v2) + " " + 
                to_string(-in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(-in1Var_v1) + " " + to_string(in1Var_v2) + " " + 
                to_string(-in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                to_string(-outVar_v2) + " 0");
    clauses.push_back(prefix + to_string(-in1Var_v1) + " " + to_string(in1Var_v2) + " " + 
                to_string(in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(-in1Var_v1) + " " + to_string(in1Var_v2) + " " + 
                to_string(in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                to_string(-outVar_v2) + " 0");
    clauses.push_back(prefix + to_string(-in2Var_v1) + " " + to_string(in2Var_v2) + " " + 
                to_string(in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(-in2Var_v1) + " " + to_string(in2Var_v2) + " " + 
                to_string(in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                to_string(-outVar_v2) + " 0");
    clauses.push_back(prefix + to_string(-in2Var_v1) + " " + to_string(in2Var_v2) + " " + 
                to_string(in1Var_v1) + " " + to_string(-in1Var_v2) + " " +
                to_string(outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(-in2Var_v1) + " " + to_string(in2Var_v2) + " " + 
                to_string(in1Var_v1) + " " + to_string(-in1Var_v2) + " " +
                to_string(-outVar_v2) + " 0");


    // 2. 当两个输入都不是Z时的XOR操作
    // input1=0, input2=0 -> output=0 
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(-outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(-outVar_v2) + " 0");

    // input1=0, input2=1 -> output=1
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(-in2Var_v2) + " " +
                     to_string(-outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(-in2Var_v2) + " " +
                     to_string(outVar_v2) + " 0");

    // input1=1, input2=0 -> output=1
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(-in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(-outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(-in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(in2Var_v2) + " " +
                     to_string(outVar_v2) + " 0");

    // input1=1, input2=1 -> output=0
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(-in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(-in2Var_v2) + " " +
                     to_string(-outVar_v1) + " 0");
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + to_string(-in1Var_v2) + " " +
                     to_string(in2Var_v1) + " " + to_string(-in2Var_v2) + " " +
                     to_string(-outVar_v2) + " 0");

    // 3. 确保在输入非Z的情况下输出不可能为Z
    clauses.push_back(prefix + to_string(in1Var_v1) + " " + 
                     to_string(in2Var_v1) + " " +
                     to_string(-outVar_v1) + " 0");

    return clauses.size() - initialClauseCount;
}


MinimizationResult encodeSubcircuitAsQBF(const Circuit& subcircuit, const int numGates, const string& filename, const int numJOINs) {
    ofstream outfile(filename);
    if (!outfile) {
        cerr << "Cannot open the file: " << filename << endl;
        exit(1);
    }

    // Variable mapping for recording variable meanings
    unordered_map<int, string> variableMapping;
    int varCounter = 1; // Variable counter for assigning unique IDs

    unordered_set<int> inputVars;        // Input variables (x_t)
    unordered_set<int> gateValueVars;    // Gate value variables (g_t)
    vector<int> selectionVars;           // Selection variables (S_{i,p,t})
    vector<int> gateFunctionVars;        // Gate function variables (F_{i,funcType})
    vector<int> outputSelectionVars;     // Output selection variables (O_{i,t})

    unordered_map<int, WireVars> wireVarMap; // Maps wire IDs to WireVars
    unordered_map<pair<int, int>, int, pair_hash> selectionVarMap; // Maps (i * maxNumInputPins + inputPin, t) to selection variable ID
    unordered_map<pair<int, GateType>, int, pair_hash> gateFunctionVarMap; // Maps (i, GateType) to function variable ID
    unordered_map<pair<int, int>, int, pair_hash> outputSelectionVarMap; // Maps (i, t) to output selection variable ID

    int n = subcircuit.numInputs;
    int numOutputs = subcircuit.numOutputs;
    vector<GateType> possibleFunctions = {XOR, BUFFER, JOIN};
    cout << "Number of OUTPUTs: " << numOutputs << endl;
    auto gateTypeToString = [](GateType type) -> string {
        switch (type) {
            case XOR: return "XOR";
            case BUFFER: return "BUFFER";
            case JOIN: return "JOIN";
            default: return "UNKNOWN";
        }
    };

    vector<int> possibleWires;
    int wireCounter = 0; // Counter for wire IDs

    // Input wires
    vector<int> inputWireIDs;
    vector<int> inputVars_v1;
    vector<int> inputVars_v2;

    for (int i = 0; i < n; ++i) {
        int wireID = wireCounter++;
        possibleWires.push_back(wireID);
        inputWireIDs.push_back(wireID);

        WireVars vars;
        vars.v1 = varCounter++;
        vars.v2 = varCounter++;

        variableMapping[vars.v1] = "InputWire_" + to_string(wireID) + "_v1";
        variableMapping[vars.v2] = "InputWire_" + to_string(wireID) + "_v2";

        inputVars.insert(vars.v1);
        inputVars.insert(vars.v2);
        wireVarMap[wireID] = vars;

        inputVars_v1.push_back(vars.v1);
        inputVars_v2.push_back(vars.v2);
    }

    // Gate output wires
    int maxNumInputPins = 2; // Maximum number of inputs any gate can have
    vector<int> gateOutputWireIDs(numGates); // Wire IDs for gate outputs

    for (int i = 0; i < numGates; ++i) {
        int wireID = wireCounter++;
        possibleWires.push_back(wireID);
        gateOutputWireIDs[i] = wireID;

        WireVars vars;
        vars.v1 = varCounter++;
        vars.v2 = varCounter++;

        variableMapping[vars.v1] = "Wire_" + to_string(wireID) + "_v1";
        variableMapping[vars.v2] = "Wire_" + to_string(wireID) + "_v2";

        gateValueVars.insert(vars.v1);
        gateValueVars.insert(vars.v2);
        wireVarMap[wireID] = vars;
    }

    // Output wires of the circuit
    vector<int> circuitOutputWireIDs(numOutputs);
    vector<int> origOutputVars_v1;
    vector<int> origOutputVars_v2;
    for (int i = 0; i < numOutputs; ++i) {
        int wireID = wireCounter++;
        possibleWires.push_back(wireID);
        circuitOutputWireIDs[i] = wireID;

        WireVars vars;
        vars.v1 = varCounter++;
        vars.v2 = varCounter++;
        // cout << "Output wire: " << wireID << " -> " << vars.v1 << ", " << vars.v2 << endl;
        variableMapping[vars.v1] = "OutputWire_" + to_string(wireID) + "_v1";
        variableMapping[vars.v2] = "OutputWire_" + to_string(wireID) + "_v2";

        gateValueVars.insert(vars.v1);
        gateValueVars.insert(vars.v2);
        wireVarMap[wireID] = vars;


        origOutputVars_v1.push_back(vars.v1);
        origOutputVars_v2.push_back(vars.v2);

    }

    // Selection variables (S_{i,p,t}) and Output selection variables (O_{i,t})
    for (int i = 0; i < numGates; ++i) {
        // Output selection variables for all gates, including constants
        for (size_t t = 0; t < possibleWires.size(); ++t) {
            int varID = varCounter++;


            variableMapping[varID] = "O_" + to_string(i) + "_" + to_string(possibleWires[t]);

            outputSelectionVars.push_back(varID);
            outputSelectionVarMap[{i, possibleWires[t]}] = varID;
        }

        // For gates other than constants, add function variables and selection variables
        if (i >= 2) { // Assuming Gates 0 and 1 are constants
            // Function variables (F_{i,funcType})
            for (const auto& funcType : possibleFunctions) {
                int varID = varCounter++;

                variableMapping[varID] = "F_" + to_string(i) + "_" + gateTypeToString(funcType);

                gateFunctionVars.push_back(varID);
                gateFunctionVarMap[{i, funcType}] = varID;
            }

            // Input selection variables (S_{i,p,t})
            int numPins = maxNumInputPins;
            for (int inputPin = 0; inputPin < numPins; ++inputPin) {
                for (size_t t = 0; t < possibleWires.size(); ++t) {
                    int varID = varCounter++;

                    variableMapping[varID] = "S_" + to_string(i) + "_" + to_string(inputPin) + "_" + to_string(possibleWires[t]);

                    selectionVars.push_back(varID);
                    selectionVarMap[{i * maxNumInputPins + inputPin, t}] = varID;
                }
            }
        }
    }

    unordered_map<pair<int, int>, int, pair_hash> orderingVarMap; // Maps (i, j) to ordering variable ID
    vector<int> orderingVars; // List of ordering variable IDs

    // Ordering variables (Order_{i,j})
    for (int i = 2; i < numGates; ++i) {
        for (int j = 2; j < numGates; ++j) {
            if (i == j) continue;

            int varID = varCounter++;
            variableMapping[varID] = "Order_" + to_string(i) + "_" + to_string(j);
            orderingVarMap[{i, j}] = varID;
            orderingVars.push_back(varID);
        }
    }

    int joinCount = 0;
    vector<int> joinGateVars;  // Track variables representing JOIN gates

    for (int i = 2; i < numGates; ++i) {
        if (gateFunctionVarMap.find({i, JOIN}) != gateFunctionVarMap.end()) {
            int joinVar = gateFunctionVarMap[{i, JOIN}];
            joinGateVars.push_back(joinVar);
        }
    }
    vector<string> clauses;

    // 1. No wire in the illegal state, CHECKED
    for (const auto& entry : wireVarMap) {
        int wireID = entry.first;
        if (inputVars.find(entry.second.v1) != inputVars.end()) {
            continue;
        }
        int v1 = entry.second.v1;
        int v2 = entry.second.v2;
        if (v1 <= 0 || v2 <= 0) {
            continue;
        }
        clauses.push_back(to_string(-v1) + " " + to_string(-v2) + " 0");
    }

    // 2. Exactly one output selection variable is true for each gate CHECKED
    // When setting up output selection constraints
    for (int i = 0; i < numGates; ++i) {
        vector<int> gateOutputSelectionVars;
        for (size_t t = 0; t < possibleWires.size(); ++t) {
            int outputSelVar = outputSelectionVarMap[{i, possibleWires[t]}];
            gateOutputSelectionVars.push_back(outputSelVar);
        }
        
        // For constant gates (i == 0 or i == 1), don't force exactly one output
        // Only require at most one output
        if (i <= 1) {  // Constant gates
            // At most one output (prevent multiple outputs)
            for (size_t j = 0; j < gateOutputSelectionVars.size(); ++j) {
                for (size_t k = j + 1; k < gateOutputSelectionVars.size(); ++k) {
                    clauses.push_back(to_string(-gateOutputSelectionVars[j]) + " " + 
                                    to_string(-gateOutputSelectionVars[k]) + " 0");
                }
            }
        } else {
            // For non-constant gates, keep the exactly-one constraint
            addExactlyOneConstraint(gateOutputSelectionVars, clauses);
        }
    }
    // 3. No two gates output to the same wire CHECKED
    for (size_t t = 0; t < possibleWires.size(); ++t) {
        for (int i = 0; i < numGates; ++i) {
            for (int j = i + 1; j < numGates; ++j) {
                int outputSelVar_i = outputSelectionVarMap[{i, possibleWires[t]}];
                int outputSelVar_j = outputSelectionVarMap[{j, possibleWires[t]}];
                clauses.push_back(to_string(-outputSelVar_i) + " " + to_string(-outputSelVar_j) + " 0");
            }
        }
    }

    // 4. Assign the constants' outputs CHECKED
    vector<pair<int, bool>> constWires; // <wire_id, isOne>
    for (const Gate& gate : subcircuit.gates) {
        if (gate.type == CONST_ONE) {
            constWires.push_back({gate.output, true});
            for (size_t t = 0; t < possibleWires.size(); ++t) {
                int outputSelVar = outputSelectionVarMap[{0, possibleWires[t]}];
                if (possibleWires[t] != gate.output) {
                    clauses.push_back(to_string(-outputSelVar) + " 0");
                }
            }
        }
        else if (gate.type == CONST_ZERO) {
            constWires.push_back({gate.output, false});
            cout << "CONST_ZERO OUTPUT WIRE: " << gate.output << endl;
            for (size_t t = 0; t < possibleWires.size(); ++t) {
                int outputSelVar = outputSelectionVarMap[{1, possibleWires[t]}];
                if (possibleWires[t] != gate.output) {
                    clauses.push_back(to_string(-outputSelVar) + " 0");
                }
            }
        }
    }
    // clauses.push_back("CONSTRAINTS FOR CONSTANTS");
    for (const auto& [wireId, isOne] : constWires) {
        if (wireVarMap.find(wireId) == wireVarMap.end()) {
            continue;
        }
        WireVars wire = wireVarMap[wireId];
        if (isOne) {
            // cout << "CONST_ONE: " << wire.v1 << " " << wire.v2 << endl;
            // CONST_ONE: v1=0, v2=1  

            clauses.push_back(to_string(-wire.v1) + " 0");
            clauses.push_back(to_string(wire.v2) + " 0");
        } else {
            // CONST_ZERO: v1=0, v2=0
            // cout << "CONST_ZERO: " << wire.v1 << " " << wire.v2 << endl;
            clauses.push_back(to_string(-wire.v1) + " 0");
            clauses.push_back(to_string(-wire.v2) + " 0");
        }
    }

    for (int inputWireID : inputWireIDs) {  
        vector<int> useInputVars;  
        
        for (int i = 2; i < numGates; ++i) {
            for (int inputPin = 0; inputPin < maxNumInputPins; ++inputPin) {
                int selVar = selectionVarMap[{i * maxNumInputPins + inputPin, inputWireID}];
                useInputVars.push_back(selVar);
            }
        }
        
        //  At least one gate will use the input wires
        string clause;
        for (int var : useInputVars) {
            clause += to_string(var) + " ";
        }
        clause += "0";
        clauses.push_back(clause);
    }

    // 5. For gates other than constants, add function and selection constraints CHECKED
    for (int i = 2; i < numGates; ++i) {
        // Function variables
        vector<int> gateFuncVars;
        for (const auto& funcType : possibleFunctions) {
            int funcVar = gateFunctionVarMap[{i, funcType}];
            gateFuncVars.push_back(funcVar);
        }

        addExactlyOneConstraint(gateFuncVars, clauses);

        // Input selection variables
        int numPins = maxNumInputPins;
        for (int inputPin = 0; inputPin < numPins; ++inputPin) {
            vector<int> gateSelectionVars;
            for (size_t t = 0; t < possibleWires.size(); ++t) {
                int selVar = selectionVarMap[{i * maxNumInputPins + inputPin, t}];
                gateSelectionVars.push_back(selVar);
            }
            addExactlyOneConstraint(gateSelectionVars, clauses);
        }
    }

    // 6. Gate functionality constraints
    for (int i = 2; i < numGates; ++i) {
        int numPins = maxNumInputPins;

        for (size_t t_out = 0; t_out < possibleWires.size(); ++t_out) {
            int outputWireID = possibleWires[t_out];
            // cout << "outputWireID: " << outputWireID << endl;
            int outputSelVar = outputSelectionVarMap[{i, outputWireID}];
            if (find(inputWireIDs.begin(), inputWireIDs.end(), outputWireID) != inputWireIDs.end()) {
                // cout << "outputWireID: " << outputWireID << " var " << outputSelVar << endl;
                clauses.push_back(to_string(-outputSelVar) + " 0");
                continue;
            }
           
            WireVars gateOutputVars = wireVarMap[outputWireID];
            // cout << wireVarMap[8].v1 << " " << wireVarMap[8].v2 << endl;
            for (size_t t1 = 0; t1 < possibleWires.size(); ++t1) {
                int selVar1 = selectionVarMap[{i * maxNumInputPins + 0, t1}]; // pin 0
                int inputWireID1 = possibleWires[t1];
                WireVars inputVars1 = wireVarMap[inputWireID1];

                for (size_t t2 = 0; t2 < possibleWires.size(); ++t2) {
                    int selVar2 = selectionVarMap[{i * maxNumInputPins + 1, t2}];
                    // cout << "selVar1, 2: " << selVar1 << ", " << selVar2 << endl;
                    int inputWireID2 = possibleWires[t2];
                    // cout << "inputWireID1, 2: " << inputWireID1 << ", " << inputWireID2 << endl;
                    // cout << "outputWireID: " << outputWireID << endl;
                    WireVars inputVars2 = wireVarMap[inputWireID2];

                    // cout << "inputVars1: " << inputVars1.v1 << " " << inputVars1.v2 << endl;   
                    // cout << "inputVars2: " << inputVars2.v1 << " " << inputVars2.v2 << endl;
                    // cout << "gateOutputVars: " << gateOutputVars.v1 << " " << gateOutputVars.v2 << endl;
                    // Prevent selecting the same wire for both inputs
                    if (inputWireID1 == inputWireID2) {
                        clauses.push_back(to_string(-selVar1) + " " + to_string(-selVar2) + " 0");
                    }

                    // // Prevent the gate from selecting its output wire as one of its inputs
                    if (inputWireID1 == outputWireID) {
                        // Add constraints: -selVar1 ∨ -outputSelVar, -selVar2 ∨ -outputSelVar
                        clauses.push_back(to_string(-selVar1) + " " + to_string(-outputSelVar) + " 0");
                    }
                    if (inputWireID2 == outputWireID) {
                        clauses.push_back(to_string(-selVar2) + " " + to_string(-outputSelVar) + " 0");
                    }

                    if (inputWireID1 == inputWireID2 || inputWireID1 == outputWireID || inputWireID2 == outputWireID) {
                        continue;
                    }


                    for (const auto& funcType : possibleFunctions) {
                        int funcVar = gateFunctionVarMap[{i, funcType}];
                        if (funcType == BUFFER) {
                            addBUFFERCompatibilityConstraints(
                                funcVar, selVar1, selVar2, outputSelVar,
                                inputVars2.v1, inputVars2.v2, // Data pin1
                                inputVars1.v1, inputVars1.v2, // Control pin0
                                gateOutputVars.v1, gateOutputVars.v2,
                                clauses
                            );
                        } else if (funcType == JOIN) {
                            addJOINCompatibilityConstraints(
                                funcVar, selVar1, selVar2, outputSelVar,
                                inputVars1.v1, inputVars1.v2,
                                inputVars2.v1, inputVars2.v2,
                                gateOutputVars.v1, gateOutputVars.v2,
                                clauses
                            );
                        } else if (funcType == XOR) {
                            addXORCompatibilityConstraints(
                                funcVar, selVar1, selVar2, outputSelVar,
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


    for (int i = 2; i < numGates; ++i) {
        for (int inputPin = 0; inputPin < maxNumInputPins; ++inputPin) {
            for (size_t t_idx = 0; t_idx < possibleWires.size(); ++t_idx) {
                int t = possibleWires[t_idx];
                int selVar = selectionVarMap.at({i * maxNumInputPins + inputPin, t_idx});

                // Skip if t is an input wire
                if (find(inputWireIDs.begin(), inputWireIDs.end(), t) != inputWireIDs.end()) {
                    continue;
                }

                // Build the clause: -selVar ∨ O_{0,t} ∨ O_{1,t} ∨ ... ∨ O_{numGates-1,t}
                vector<int> clauseVars;
                clauseVars.push_back(-selVar);

                for (int j = 0; j < numGates; ++j) {
                    int outputSelVar = outputSelectionVarMap.at({j, t});
                    clauseVars.push_back(outputSelVar);
                }

                // Convert clauseVars to a clause string
                string clause;
                for (int var : clauseVars) {
                    clause += to_string(var) + " ";
                }
                clause += "0";
                clauses.push_back(clause);
            }
        }
    }
    // Add constraint: every internal wire (not circuit output) must be used as input somewhere
    for (size_t t = 0; t < possibleWires.size(); ++t) {
        int wireID = possibleWires[t];
        
        // Skip if this wire is a circuit output, input wire, or output wire of a constant
        if (find(circuitOutputWireIDs.begin(), circuitOutputWireIDs.end(), wireID) != circuitOutputWireIDs.end() ||
            find(inputWireIDs.begin(), inputWireIDs.end(), wireID) != inputWireIDs.end()) {
            // cout << "Skipping wire " << wireID << endl;
            continue;
        }

        // Skip wires that come from constant gates
        bool isConstantWire = false;
        for (const auto& [constWireID, _] : constWires) {
            if (wireID == constWireID) {
            isConstantWire = true;
            break;
            }
        }
        if (isConstantWire) {
            continue;
        }

        // Get all selection variables that could select this wire as input
        vector<int> wireUsageVars;
        for (int i = 2; i < numGates; ++i) {
            for (int inputPin = 0; inputPin < maxNumInputPins; ++inputPin) {
                int selVar = selectionVarMap[{i * maxNumInputPins + inputPin, t}];
                wireUsageVars.push_back(selVar);
            }
        }

        // For each gate that could produce this wire
        for (int i = 0; i < numGates; ++i) {
            int outputSelVar = outputSelectionVarMap[{i, wireID}];
            
            // If this wire is produced (-outputSelVar), it must be used somewhere
            string clause = to_string(-outputSelVar) + " ";
            for (int usageVar : wireUsageVars) {
                clause += to_string(usageVar) + " ";
            }
            clause += "0";
            clauses.push_back(clause);
        }
    }

    // 7. Ordering constraints CHECKED
    // 7.1. Transitivity constraints: (-O_{i,j} ∨ -O_{j,k} ∨ O_{i,k})
    for (int i = 2; i < numGates; ++i) {
        for (int j = 2; j < numGates; ++j) {
            if (i == j) continue;
            clauses.push_back(to_string(-orderingVarMap[{i, j}]) + " " + to_string(-orderingVarMap[{j, i}]) + " 0");

            for (int k = 2; k < numGates; ++k) {
                if (i == j || j == k || i == k) continue;

                int O_i_j = orderingVarMap[{i, j}];
                int O_j_k = orderingVarMap[{j, k}];
                int O_i_k = orderingVarMap[{i, k}];

                clauses.push_back(to_string(-O_i_j) + " " + to_string(-O_j_k) + " " + to_string(O_i_k) + " 0");
            }
        }
    }

    // 7.2. Connection constraints: If gate i can use output of gate j, then add (-S_{i,p,t} ∨ O_{j,i})
    for (int i = 2; i < numGates; ++i) {
        for (int inputPin = 0; inputPin < maxNumInputPins; ++inputPin) {
            for (size_t t = 0; t < possibleWires.size(); ++t) {
                int selVar = selectionVarMap.at({i * maxNumInputPins + inputPin, t});
                int wireID = possibleWires[t];

                // Find all gates that can produce wireID
                for (int j = 2; j < numGates; ++j) {
                    if (i == j) continue;

                    int outputSelVar = outputSelectionVarMap.at({j, wireID});

                    int O_j_i = orderingVarMap[{j, i}];
                    // cout << "adding constraint for ordering" << selVar << " " << outputSelVar << " " << O_j_i << endl;
                    clauses.push_back(to_string(-selVar) + " " + to_string(-outputSelVar) + " " + to_string(O_j_i) + " 0");
                }
            }
        }
    }

    int numInputs = inputVars_v1.size();
    int totalCombinations = pow(3, numInputs); // Each input can be 0, 1, or Z

    for (int combo = 0; combo < totalCombinations; ++combo) {
        // Generate the specific input assignment
        vector<int> inputAssignment(numInputs);
        int temp = combo;
        for (int i = 0; i < numInputs; ++i) {
            inputAssignment[i] = temp % 3; // 0: 0, 1: 1, 2: Z
            temp /= 3;
        }

        // Simulate the original circuit to get the expected output
        vector<int> expectedOutputs = simulateOriginalCircuit(subcircuit, inputAssignment);
        
        // Add constraints only if expectedOutput is valid
        for (auto output : expectedOutputs) {
            // cout << "Input assignment: " << inputAssignment[0] << " " << inputAssignment[1] << endl;
            // cout << "output result " << output << endl;
        }
        for (size_t outIdx = 0; outIdx < expectedOutputs.size(); ++outIdx) {
            int expectedOutput = expectedOutputs[outIdx];
                // Add input-output constraint
            addInputOutputConstraint(
                inputAssignment,
                expectedOutput,
                inputVars_v1,
                inputVars_v2,
                origOutputVars_v1[outIdx],
                origOutputVars_v2[outIdx],
                clauses
            );
            
        }
    }

    for (int outputWireID : circuitOutputWireIDs) {  
        vector<int> produceOutputVars;  
        for (int i = 2; i < numGates; ++i) {
            int outputSelVar = outputSelectionVarMap[{i, outputWireID}];
            produceOutputVars.push_back(outputSelVar);
        }
        // At least one gate will produce each output wire
        string clause;
        for (int var : produceOutputVars) {
            clause += to_string(var) + " ";
        }
        clause += "0";
        clauses.push_back(clause);
    }

    int maxAllowedJoins = numJOINs;
    // Create counter variables and add constraints
    vector<int> counterVars;
    int startCounterVars = varCounter;
    cout << "maximum allowed joins: " << maxAllowedJoins << endl;
    cout << "joinGateVars size: " << joinGateVars.size() << endl;
    // Add sequential counter variables
    for(int i = 1; i <= joinGateVars.size(); i++) {
        for(int j = 1; j <= maxAllowedJoins; j++) {
            counterVars.push_back(varCounter++);
        }
    }
    // Add JOIN minimization constraints
    addJoinMinimizationConstraints(joinGateVars, maxAllowedJoins, clauses);


    int totalClauses = clauses.size();

    cout << "Selection Vars Size : " << selectionVars.size() << endl;
    cout << "Output Selection Vars Size : " << outputSelectionVars.size() << endl;
    cout << "Gate Function Vars Size : " << gateFunctionVars.size() << endl;
    cout << "Ordering Vars Size : " << orderingVars.size() << endl;
    cout << "Gate Value Vars Size : " << gateValueVars.size() << endl;
    cout << "Counter Vars Size : " << counterVars.size() << endl;


    outfile << "p cnf " << varCounter - 1 << " " << totalClauses << endl;
    outfile << "e ";

    for (int var : selectionVars) { // 51-158
        outfile << var << " ";
    }
    for (int var : outputSelectionVars) { // 21-47, 69-77, 99-107, 129-137
        outfile << var << " ";
    }
    for (int var : gateFunctionVars) { // 48-50, 78-80, 108-110, 138-140
        outfile << var << " ";
    }
    for (int var : orderingVars) { // 159-170
        outfile << var << " ";
    }
    outfile << "0\n";

    outfile << "a ";
    for (int var : inputVars) { // 1-4
        outfile << var << " ";
    }
    outfile << "0\n";

    outfile << "e ";
    for (int var : gateValueVars) { // 5-18

        outfile << var << " ";
    }
    for (int var : counterVars) {
        outfile << var << " ";
    }

  // Write variable mapping
    ofstream mappingFile("variable_mapping.txt");
    if (!mappingFile) {
        cerr << "Cannot open the file: variable_mapping.txt" << endl;
        exit(1);
    }

    for (const auto& entry : variableMapping) {
        mappingFile << entry.first << " " << entry.second << endl;
    }

    mappingFile.close();

    outfile << "0" << endl;

    // First, create clause groups and track them
    std::unordered_map<int, ClauseGroupID> clauseGroups;
    std::unordered_map<ClauseGroupID, std::vector<std::string>> groupClauses;

    QDPLL* depqbf = qdpll_create();

    qdpll_configure(depqbf, "--incremental-use");
    qdpll_configure(depqbf, "--dep-man=simple");

    // First scope: Existential - Selection variables, output selection variables, gate function variables
    Nesting scope1 = qdpll_new_scope(depqbf, QDPLL_QTYPE_EXISTS);
    for (int var : selectionVars) {
        qdpll_add(depqbf, var);
    }
    for (int var : outputSelectionVars) {
        qdpll_add(depqbf, var);
    }
    for (int var : gateFunctionVars) {
        qdpll_add(depqbf, var);
    }
    for (int var : orderingVars) {
        qdpll_add(depqbf, var);
    }
    qdpll_add(depqbf, 0);  // Close scope

    // Second scope: Universal - Input variables 
    Nesting scope2 = qdpll_new_scope(depqbf, QDPLL_QTYPE_FORALL);
    for (int var : inputVars) {
        qdpll_add(depqbf, var);
    }
    qdpll_add(depqbf, 0);  // Close scope

    // Third scope: Existential - Gate value variables
    Nesting scope3 = qdpll_new_scope(depqbf, QDPLL_QTYPE_EXISTS);
    for (int var : gateValueVars) {
        qdpll_add(depqbf, var);
    }
    for (int var : counterVars) {
        qdpll_add(depqbf, var);
    }
    qdpll_add(depqbf, 0);  // Close scope


    for (const string& clause : clauses) {
        outfile << clause << endl;
    }

    for (int i = 0; i < clauses.size(); i++) {
        // Create a new group for each clause
        ClauseGroupID group = qdpll_new_clause_group(depqbf);
        qdpll_open_clause_group(depqbf, group);
        
        // Store the clause text
        groupClauses[group].push_back(clauses[i]);
        
        // Parse the clause string and add literals
        std::istringstream iss(clauses[i]);
        std::string literal;
        
        // Read literals until we reach "0"
        while (iss >> literal) {
            if (literal == "0") {
                break;
            }
            // Convert string to integer and add to solver 
            qdpll_add(depqbf, std::stoi(literal));
        }
        // Close the clause by adding 0
        qdpll_add(depqbf, 0);
        
        qdpll_close_clause_group(depqbf, group);
    }
    // Solve

//     // After solving
//     if (result == QDPLL_RESULT_SAT) {
//         std::cout << "SAT Result - Variable Assignments:" << std::endl;
        
//         // Print assignments for all variables

//         // for (int var = 1; var <= varCounter-1; var++) {
//         //     QDPLLAssignment value = qdpll_get_value(depqbf, var);
//         //     std::string value_str;
//         //     switch(value) {
//         //         case QDPLL_ASSIGNMENT_TRUE:
//         //             value_str = "TRUE";
//         //             break;
//         //         case QDPLL_ASSIGNMENT_FALSE:
//         //             value_str = "FALSE";
//         //             break;
//         //         case QDPLL_ASSIGNMENT_UNDEF:
//         //             value_str = "UNDEFINED";
//         //             break;
//         //         default:
//         //             value_str = "UNKNOWN";
//         //     }
            
//         //     // If you're tracking variable meanings with variableMapping
//         //     if (variableMapping.find(var) != variableMapping.end()) {
//         //         std::cout << "Variable " << var << " (" << variableMapping[var] << "): " << value_str << std::endl;
//         //     } else {
//         //         std::cout << "Variable " << var << ": " << value_str << std::endl;
//         //     }
//         // }
//         int actualJoinCount = 0;
//         for (int var : joinGateVars) {
//             if (qdpll_get_value(depqbf, var) == QDPLL_ASSIGNMENT_TRUE) {
//                 actualJoinCount++;
//             }
//         }
//         cout << "Solution uses " << actualJoinCount << " JOIN gates" << endl;
//     } 

//     // If UNSAT, get the core
//     if (result == QDPLL_RESULT_UNSAT) {
//         ClauseGroupID* relevantGroups = qdpll_get_relevant_clause_groups(depqbf);
        
//         std::cout << "UNSAT Core clauses:\n";
//         for (int i = 0; relevantGroups[i] != 0; i++) {
//             ClauseGroupID group = relevantGroups[i];
//             for (const std::string& clause : groupClauses[group]) {
//                 std::cout << clause << "\n";
//             }
//         }
        
//         // Don't forget to free the array
//         free(relevantGroups);
//     }



//     if (result == QDPLL_RESULT_UNSAT) {
//         ClauseGroupID* relevantGroups = qdpll_get_relevant_clause_groups(depqbf);
        
//         // Collect all variables involved in core
//         std::set<int> coreVars;
//         for (int i = 0; relevantGroups[i] != 0; i++) {
//             for (const std::string& clause : groupClauses[relevantGroups[i]]) {
//                 std::istringstream iss(clause);
//                 int var;
//                 while (iss >> var) {
//                     if (var != 0) {
//                         coreVars.insert(abs(var));  // Store absolute value of variable ID
//                     }
//                 }
//             }
//         }
        
//         // Print variables involved
//         std::cout << "Variables involved in conflict:\n";
//         for (int var : coreVars) {
//             if (variableMapping.find(var) != variableMapping.end()) {
//                 std::cout << variableMapping[var] << " " << var << "\n";
//             }
//         }
        
//         free(relevantGroups);
//     }

//     qdpll_delete(depqbf);





//     outfile.close();

//     // Write variable mapping
//     ofstream mappingFile("variable_mapping.txt");
//     if (!mappingFile) {
//         cerr << "Cannot open the file: variable_mapping.txt" << endl;
//         exit(1);
//     }

//     for (const auto& entry : variableMapping) {
//         mappingFile << entry.first << " " << entry.second << endl;
//     }

//     mappingFile.close();
//     return result;
// }


//     // SMALL CIRCUIT, MINIMIZE JOIN GATES, DELETE ACYCLICITY CONSTRAINTS, 
//     // RUNTIME ACYCLICITY CHECK
//     // DEF 5
    QDPLLResult result = qdpll_sat(depqbf);
    
    MinimizationResult minResult;
    minResult.success = (result == QDPLL_RESULT_SAT);
    
    if (minResult.success) {
        // Count JOIN gates in solution
        minResult.joinGates = 0;
        for (int var : joinGateVars) {
            if (qdpll_get_value(depqbf, var) == QDPLL_ASSIGNMENT_TRUE) {
                minResult.joinGates++;
            }
        }
        
        // Count total gates used
        minResult.totalGates = 0;
        for (int i = 2; i < numGates; ++i) {
            bool gateUsed = false;
            for (const auto& funcType : {XOR, BUFFER, JOIN}) {
                auto it = gateFunctionVarMap.find({i, funcType});
                if (it != gateFunctionVarMap.end() && 
                    qdpll_get_value(depqbf, it->second) == QDPLL_ASSIGNMENT_TRUE) {
                    gateUsed = true;
                    break;
                }
            }
            if (gateUsed) minResult.totalGates++;
        }
    }


   

    return minResult;
}
