# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread

# Project name
TARGET = crawler

# Source files (We will add more later)
SOURCES = main.cpp HttpUtils.cpp UrlFrontier.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
