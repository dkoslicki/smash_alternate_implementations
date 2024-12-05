# Compiler and flags
CXX = g++
CXXFLAGS = -O3 -std=c++17

# Source files
SRC_DIR = src
SRCS = $(SRC_DIR)/gather.cpp \
       $(SRC_DIR)/compare.cpp \
	   $(SRC_DIR)/prefetch.cpp \
       $(SRC_DIR)/Sketch.cpp \
       $(SRC_DIR)/MultiSketchIndex.cpp \
       $(SRC_DIR)/utils.cpp

# Object files
OBJ_DIR = obj
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# Executables
BIN_DIR = bin
TARGETS = $(BIN_DIR)/gather $(BIN_DIR)/compare $(BIN_DIR)/prefetch

# Default target
.PHONY: all
all: $(TARGETS)

# Rules to build executables
$(BIN_DIR)/gather: $(OBJ_DIR)/gather.o $(OBJ_DIR)/Sketch.o $(OBJ_DIR)/MultiSketchIndex.o $(OBJ_DIR)/utils.o
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN_DIR)/compare: $(OBJ_DIR)/compare.o $(OBJ_DIR)/Sketch.o $(OBJ_DIR)/MultiSketchIndex.o $(OBJ_DIR)/utils.o
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN_DIR)/prefetch: $(OBJ_DIR)/prefetch.o $(OBJ_DIR)/Sketch.o $(OBJ_DIR)/MultiSketchIndex.o $(OBJ_DIR)/utils.o
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Rule to build object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up
.PHONY: clean
clean:
	rm -f $(OBJ_DIR)/*.o $(BIN_DIR)/*
