LEVEL = ../..
LLVM_BUILDDIR ?= $(LEVEL)/build
CSI_RTLIBDIR = $(LLVM_BUILDDIR)/lib/clang/3.9.0/lib/linux
CSI_RTLIBA = $(CSI_RTLIBDIR)/libclang_rt.csi-x86_64.a

CC = $(LLVM_BUILDDIR)/bin/clang
CXX = $(LLVM_BUILDDIR)/bin/clang++
CXXFLAGS = -g -O0
LDFLAGS = -fuse-ld=gold -flto

TOOLKIT = ../toolkit
TOOL = mem-tracer
TOOL_OBJ = $(TOOLKIT)/$(TOOL).o

default: simple

$(TOOL_OBJ):
	make -C $(TOOLKIT) $(TOOL_OBJ)

simple.o: simple.cpp $(TOOL_OBJ)
	$(CXX) $(CXXFLAGS) -fcsi -emit-llvm -c $< -o $@

simple: simple.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) simple.o $(TOOL_OBJ) $(CSI_RTLIBA)

clean:
	rm -f simple.o
	make -C $(TOOLKIT) clean
