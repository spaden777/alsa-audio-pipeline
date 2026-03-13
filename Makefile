CXX := g++
TARGET := alsarb
SRC := main.cpp wav.cpp

COMMON_FLAGS := -Wall -Wextra -std=c++17
DEBUG_FLAGS := -g -O0
RELEASE_FLAGS := -O2 -DNDEBUG
LDLIBS := -lasound

VALGRIND := valgrind
VALGRIND_FLAGS := --leak-check=full --show-leak-kinds=all --track-origins=yes

all: debug

.PHONY: all debug release build clean valgrind

debug: CXXFLAGS := $(COMMON_FLAGS) $(DEBUG_FLAGS)
debug: build

release: CXXFLAGS := $(COMMON_FLAGS) $(RELEASE_FLAGS)
release: build

build:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)

valgrind: debug
	$(VALGRIND) $(VALGRIND_FLAGS) ./$(TARGET)

clean:
	rm -f $(TARGET) *.raw