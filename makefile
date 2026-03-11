CXX := g++
TARGET := alsarb
SRC := main.cpp

COMMON_FLAGS := -Wall -Wextra -std=c++17
DEBUG_FLAGS := -g -O0
RELEASE_FLAGS := -O2 -DNDEBUG
LDLIBS := -lasound

all: debug

.PHONY: debug release clean

debug: CXXFLAGS := $(COMMON_FLAGS) $(DEBUG_FLAGS)
debug: build

release: CXXFLAGS := $(COMMON_FLAGS) $(RELEASE_FLAGS)
release: build

build:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)

clean:
	rm -f $(TARGET)