CXX = g++
CXXFLAGS = -O3 -std=c++17
TARGET = prefetch
SRCS = src/prefetch.cpp src/utils.cpp src/MultiSketchIndex.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
	rm -f $(TARGET)