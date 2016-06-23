## Building CSI

To build and run LLVM tests for CSI, execute the following commands:

    git clone --recursive git@github.com:csi-llvm/llvm.git
    cd llvm/csi
    ./build.sh
    
(Note that this repository, `csi`, is set up as a submodule of `llvm`.) This can take a long time to complete, depending on the type of machine you are building on.

## Example of usage

The `toolkit` directory contains several small example tools. The `example` directory contains a simple example program and Makefile to generate a CSI-instrumented binary. If all the tests from `build.sh` passed, you should be able to execute `cd example && make` to build the example CSI-instrumented binary.
