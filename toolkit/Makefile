include ../common.mk

CXXFLAGS += -Wno-error

LINK ?= $(COMPILER_HOME)/bin/llvm-link

TOOLS := print_tool counter_tool code-coverage instrument_function prof_lite props_tool bb-counter fed_test callsites_test demo_tool mem_tracer simple_mem_tracer
OBJECTS := $(patsubst %,%.o,$(TOOLS))

null_tool.o: null_tool.cpp
	$(CXX) -O3 -c -emit-llvm $< -o $@

all: $(TOOLS) callgraph.o

cilkprof.o: null_tool.o cilkprof/
	$(CC) -O3 -Wall -g -std=gnu11 -c -emit-llvm cilkprof/cilkprof.c -o $@
	$(LINK) $@ null_tool.o -o $@

call-graph.o: call-graph.c null_tool.o
	$(CC) -std=c99 -O3 `pkg-config --libs --cflags glib-2.0` -c -emit-llvm $< -o $@
	$(LINK) $@ null_tool.o -o $@

$(OBJECTS): %.o: %.cpp null_tool.o
	$(CXX) -std=c++11 -O3 -c -emit-llvm $< -o $@
	$(LINK) $@ null_tool.o -o $@

%.ll: %.cpp
	$(CXX) -O0 -S $(EXTRA) -fcsi -emit-llvm $^ null_tool.o -o $@

# Disable implict link step (we just want the object file).
%: %.o ;

clean:
	-rm -f prof_lite.o null_tool null_tool.o call-graph.o $(TOOLS) $(OBJECTS)
