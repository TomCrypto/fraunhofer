EXECUTABLE = fraunhofer
INCLUDE = -Iinclude/
CXX = g++

           # The OpenCL C++ wrapper isn't fully 1.2 yet
CXXFLAGS = -DCL_USE_DEPRECATED_OPENCL_1_1_APIS -Wno-cpp \
           -O3 -march=native -Wall -Wextra -pedantic -pipe

HEADERS = $(shell find include/ -name '*.hpp')

OBJECTS = $(subst cpp,o,$(subst src/,obj/,$(shell find src/ -name '*.cpp')))

LDLIBS = -lOpenCL

$(EXECUTABLE): $(OBJECTS)
	@mkdir -p bin/
	@$(CXX) $(OBJECTS) -o $(addprefix bin/, $(EXECUTABLE)) $(LDLIBS)

$(OBJECTS): obj/%.o : src/%.cpp $(HEADERS)
	@mkdir -p $(@D)
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean:
	@rm -f $(addprefix bin/, $(EXECUTABLE))
	@rm -f obj/ --recursive
	@rm -f bin/ --recursive
