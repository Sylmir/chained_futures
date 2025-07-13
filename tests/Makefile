CXX ?= g++
CXXFLAGS = -Wall -Wextra -std=c++20 -I ../include

TESTS = base void class decay

all: $(TESTS)

base: base.cpp
void: void.cpp
class: class.cpp
decay: decay.cpp

base void class decay:
	$(CXX) $(CXXFLAGS) -o $@ $^
	./$@

clean:
	rm -f $(TESTS)
