CXX ?= g++
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra -pedantic

test: build/closed_loop_test
	./build/closed_loop_test build/cpp_results.csv

build/closed_loop_test: core/climate_controller.cpp core/climate_controller.h test/closed_loop_test.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ core/climate_controller.cpp test/closed_loop_test.cpp

clean:
	rm -rf build
