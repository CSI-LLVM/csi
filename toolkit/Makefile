LEVEL = ../..

LLVM_BUILDDIR ?= $(LEVEL)/build
LLVM_SRCDIR ?= $(LEVEL)

CSI_NULLTOOL_C = $(LLVM_SRCDIR)/projects/compiler-rt/test/csi/tools/null-tool.c
CSI_INCDIR = $(LLVM_SRCDIR)/projects/compiler-rt/lib/csi
CSI_RTLIBDIR = $(LLVM_BUILDDIR)/lib/clang/3.9.0/lib/linux
CSI_RTLIBA = $(CSI_RTLIBDIR)/libclang_rt.csi-x86_64.a
LINK = $(LLVM_BUILDDIR)/bin/llvm-link

CC = $(LLVM_BUILDDIR)/bin/clang
CXX = $(LLVM_BUILDDIR)/bin/clang++

CFLAGS += -I$(CSI_INCDIR)
CXXFLAGS += -I$(CSI_INCDIR)

TOOLS := mem-counter code-coverage prof-lite demo-tool mem-tracer
OBJECTS := $(patsubst %,%.o,$(TOOLS))

all: $(TOOLS)

null-tool.o: $(CSI_NULLTOOL_C)
	$(CC) $(CFLAGS) -O3 -c -emit-llvm $< -o $@

$(OBJECTS): %.o: %.cpp null-tool.o
	$(CXX) $(CXXFLAGS) -std=c++11 -O3 -c -emit-llvm $< -o $@
	$(LINK) $@ null-tool.o -o $@

# Disable implict link step. For tools, we just want the object files.
%: %.o ;

clean:
	-rm -f null-tool.o $(TOOLS) $(OBJECTS)
