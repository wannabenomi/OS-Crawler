# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread

# Linker flags (Must come LAST)
LDFLAGS = -lssl -lcrypto

# Project name
TARGET = crawler

# Source files
SOURCES = main.cpp HttpUtils.cpp UrlFrontier.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)