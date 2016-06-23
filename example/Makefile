LEVEL = ../..
LLVM_BUILDDIR ?= $(LEVEL)/build
CSI_RTLIBDIR = $(LLVM_BUILDDIR)/lib/clang/3.9.0/lib/linux
CSI_RTLIBA = $(CSI_RTLIBDIR)/libclang_rt.csi-x86_64.a

CC = $(LLVM_BUILDDIR)/bin/clang
CXX = $(LLVM_BUILDDIR)/bin/clang++
CXXFLAGS = -g -O0
LDFLAGS = -fuse-ld=gold -flto

TOOL = code-coverage
TOOLKIT = ../toolkit
TOOL_OBJ = $(TOOLKIT)/$(TOOL).o

default: main

.PHONY: $(TOOL_OBJ)
$(TOOL_OBJ):
	@test -f $(TOOL_OBJ) || (echo Please \"make $(TOOL).o\" in directory \"$(TOOLKIT)\" and then rerun make here.; exit 1)

main.o: main.cpp $(TOOL_OBJ)
	$(CXX) $(CXXFLAGS) -fcsi -emit-llvm -c $< -o $@

main: main.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) main.o $(TOOL_OBJ) $(CSI_RTLIBA) -o $@

clean:
	rm -f main.o main
