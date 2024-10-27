CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2

TARGET = qbfencoder

SRCS = main.cpp circuit.cpp gate.cpp utils.cpp qbfencoder.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean