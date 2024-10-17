CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2

# Target executable name
TARGET = qbfencoder

# Source files
SRCS = main.cpp circuit.cpp gate.cpp utils.cpp qbfencoder.cpp

# Object files (replace .cpp with .o)
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Target to build the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Compile source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets (not associated with files)
.PHONY: all clean