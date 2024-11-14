import re
from collections import defaultdict

# Define file paths (modify these if your files are named differently or located elsewhere)
VARIABLE_MAPPING_FILE = 'variable_mapping.txt'
VARIABLE_ASSIGNMENTS_FILE = 'output_temp.txt'

# Define data structures
variable_mapping = {}
variable_assignments = {}

# Define Gate and Wire classes for better structure
class Gate:
   def __init__(self, gate_id):
       self.gate_id = gate_id
       self.function = None  # 'XOR', 'BUFFER', 'JOIN'
       self.input_pins = {}  # pin_number: wire_id
       self.output_wire = None
       self.output_selection = None  # Output wire selection

   def __str__(self):
       return f'Gate {self.gate_id}:\n' \
              f'  Function: {self.function}\n' \
              f'  Input Pins: {self.input_pins}\n' \
              f'  Output Wire: {self.output_wire}\n' \
              f'  Output Selection: {self.output_selection}'

class Wire:
   def __init__(self, wire_id):
       self.wire_id = wire_id
       self.v1 = False
       self.v2 = False
       self.wire_type = None  # "Input", "Output", or "Internal"

   def get_value(self):
       if self.v1 and not self.v2:
           return 'Z'
       elif not self.v1 and not self.v2:
           return '0'
       elif not self.v1 and self.v2:
           return '1'
       else:
           return 'X'  # Illegal state

   def __str__(self):
       return f'Wire {self.wire_id} ({self.wire_type}): v1={int(self.v1)}, v2={int(self.v2)} ({self.get_value()})'

# Function to parse files
def parse_variable_mapping(file_path):
   with open(file_path, 'r') as f:
       for line in f:
           parts = line.strip().split()
           if len(parts) >= 2:
               var_id = int(parts[0])
               var_name = parts[1]
               variable_mapping[var_id] = var_name

def parse_variable_assignments(file_path):
   with open(file_path, 'r') as f:
       for line in f:
           line = line.strip()
           if line.startswith('V'):
               val = int(line.split()[1])
               variable_assignments[abs(val)] = (val > 0)

def reconstruct_circuit():
   gates = {}
   wires = {}
   gates[0] = Gate(0)
   gates[0].function = 'CONST_ONE'   # gate0是CONST_ONE
   gates[1] = Gate(1)
   gates[1].function = 'CONST_ZERO'  # gate1是CONST_ZERO

   # First pass: Create wire objects and set their values
   for var_id, var_name in variable_mapping.items():
       if 'Wire' in var_name:
           parts = var_name.split('_')
           if parts[0] == 'InputWire':
               wire_id = int(parts[1])
               wire_type = "Input"
           elif parts[0] == 'OutputWire':
               wire_id = int(parts[1])
               wire_type = "Output"
           elif parts[0] == 'Wire':
               wire_id = int(parts[1])
               wire_type = "Internal"
           else:
               continue
               
           if wire_id not in wires:
               wires[wire_id] = Wire(wire_id)
               wires[wire_id].wire_type = wire_type
               
           if parts[2] == 'v1':
               wires[wire_id].v1 = variable_assignments.get(var_id, False)
           elif parts[2] == 'v2':
               wires[wire_id].v2 = variable_assignments.get(var_id, False)

   # Second pass: Handle gates
   for var_id, var_name in variable_mapping.items():
       if var_name.startswith('F_'):  # Function variables
           parts = var_name.split('_')
           gate_id = int(parts[1])
           gate_type = parts[2]
           if variable_assignments.get(var_id, False):
               if gate_id not in gates:
                   gates[gate_id] = Gate(gate_id)
               gates[gate_id].function = gate_type

       elif var_name.startswith('S_'):  # Selection variables
           parts = var_name.split('_')
           gate_id = int(parts[1])
           pin_num = int(parts[2])
           wire_id = int(parts[3])
           if variable_assignments.get(var_id, False):
               if gate_id not in gates:
                   gates[gate_id] = Gate(gate_id)
               gates[gate_id].input_pins[pin_num] = wire_id

       elif var_name.startswith('O_'):  # Output selection variables
           parts = var_name.split('_')
           gate_id = int(parts[1])
           wire_id = int(parts[2])
           if variable_assignments.get(var_id, False):
               if gate_id not in gates:
                   gates[gate_id] = Gate(gate_id)
               gates[gate_id].output_selection = wire_id
               gates[gate_id].output_wire = wire_id

   # Print circuit information
   print("\n=== Circuit Configuration ===\n")
   
   # Print Wires
   print("Wires:")
   for wire_id in sorted(wires.keys()):
       wire = wires[wire_id]
       if wire.wire_type == "Input":
           print(f"  Input {wire}")
       elif wire.wire_type == "Output":
           print(f"  Output {wire}")
       else:
           print(f"  Internal {wire}")
   
   # Print Gates
   print("\nGates:")
   for gate_id in sorted(gates.keys()):
       print(f"\n{gates[gate_id]}")
       if gates[gate_id].output_wire in wires:
           print(f"  Output Wire Value: {wires[gates[gate_id].output_wire].get_value()}")

   # Print circuit summary
   print("\n=== Circuit Summary ===")
   print(f"Total Gates: {len(gates)}")
   print(f"Total Wires: {len(wires)}")
   input_wires = sum(1 for w in wires.values() if w.wire_type == "Input")
   output_wires = sum(1 for w in wires.values() if w.wire_type == "Output")
   print(f"Input Wires: {input_wires}")
   print(f"Output Wires: {output_wires}")
   print(f"Internal Wires: {len(wires) - input_wires - output_wires}")

if __name__ == '__main__':
   parse_variable_mapping(VARIABLE_MAPPING_FILE)
   parse_variable_assignments(VARIABLE_ASSIGNMENTS_FILE)
   reconstruct_circuit()