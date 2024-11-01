# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -std=c++11

# Executable name
TARGET = rft-client

# Source files
SRCS = main.cpp datagram.cpp unreliableTransport.cpp timerC.cpp
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Compile source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up object files and the executable
clean:
	rm -f $(OBJS) $(TARGET)

# Tar for the submission
mksubmit: clean
	tar -cvf IgnacioSanz.tar.gz *

# Additional convenience target for running the program
run: $(TARGET)
	./$(TARGET) -h <hostname> -p <port> -f <filename> [-d <debug level>]

# Phony targets
.PHONY: all clean mksubmit run
