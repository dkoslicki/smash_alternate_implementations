CXX = g++
CXXFLAGS = -O3 -std=c++17
TARGET = gather
SRCS = src/gather.cpp src/utils.cpp src/MultiSketchIndex.cpp src/Sketch.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
	rm -f $(TARGET)