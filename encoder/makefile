CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -I.

TARGET = qbfencoder

SRCS = main.cpp circuit.cpp gate.cpp utils.cpp qbfencoder.cpp

OBJS = $(SRCS:.cpp=.o)

# Add DepQBF and related libraries
DEPQBF_DIR = ./solver
DEPQBF_LIB = $(DEPQBF_DIR)/libqdpll.a
PICOSAT_LIB = $(DEPQBF_DIR)/picosat/libpicosat.a
NENOFEX_LIB = $(DEPQBF_DIR)/nenofex/libnenofex.a

# Architecture detection
ARCH = $(shell uname -m)
ifeq ($(ARCH),arm64)
    CXXFLAGS += -arch arm64
endif

all: $(TARGET)

# First make the DepQBF library
$(DEPQBF_LIB):
	$(MAKE) -C $(DEPQBF_DIR)

# Link against all required libraries
$(TARGET): $(OBJS) $(DEPQBF_LIB)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) \
		$(DEPQBF_LIB) \
		-L$(DEPQBF_DIR)/picosat -lpicosat \
		-L$(DEPQBF_DIR)/nenofex -lnenofex

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	$(MAKE) -C $(DEPQBF_DIR) clean

.PHONY: all clean