CXX = g++
CXXFLAGS = -O3 -std=c++17
TARGET = prefetch
SRCS = src/prefetch.cpp src/utils.cpp src/MultiSketchIndex.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)