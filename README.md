# Extensions for PyTorch

This repo holds a number of extensions for PyTorch. The current development
branch is [e2.8.0](https://github.com/ghdk/pytorch/tree/e2.8.0) which is
based on the `v2.8.0` release of PyTorch.

<!-- toc -->

- [Bitarray](#bitarray)
- [Constraints](#constraints)
- [Graph](#graph)
- [GraphDB](#graphdb)
- [Iterators](#iterators)
- [LLVM plugin](#llvm-plugin)
- [Theory](#theory)
- [Build](#build)

<!-- tocstop -->

## Bitarray

A bitarray implementation built on Tensors. 

Lives under `/extensions/bitarray`.

## Constraints

A constraints solver built on Tensors. It implements conflict directed
backjumping and forward checking, see [Prosser93](#bibliography). Instead of
implementing a list of open nodes or recursion, it implements a state as an
array holding for each variable of the state, (ie. depth of the search tree),
the index from the domain of the variable included in the current path.
Similarly instead of lists it implements a 1D Tensor for CBJ, and a 2D Tensor
for FC.

Lives under `/extensions/constraints`.

## Graph

A graph implemented as two bitmaps, the vertex set and the adjacency matrix. The
vertex set defines the vertex indexes that are present in the graph. Information
about vertices and edges are kept on external containers, linked by the indexes.

Lives under `/extensions/graph`.

## GraphDB

A graph DB implemented on top of LMDB. The schema of the DB is documented in 
`/extensions/graphdb/schema.h`.

Lives under `/extensions/graphdb`.

## Iterators

Lives under `/extensions/iter`.

## LLVM plugin

Lives under `/extensions/llvmast`.

## Theory

The study of algorithms, mostly in the areas of AI, Graph, Optimisation, and
Parallelism.

Lives under `/extensions/theory`.

## Build

The extensions live under the `/extensions` directory. A dedicated `setup.py`
script is used to build the extensions, examples

```bash
# Linux
                              CC=clang CXX=clang++ SETUPTOOLS_EXT_SUFFIX=.so  DEBUG=1 USE_DISTRIBUTED=0 USE_MKLDNN=0 USE_CUDA=0 USE_ROCM=0 BUILD_TEST=0 USE_FBGEMM=0 USE_NNPACK=0 USE_QNNPACK=0 USE_XNNPACK=0 USE_MPS=0 EXT_USE_LLVM_CONFIG=/usr/bin/llvm-config                                                          python setup.py  build --build-lib=./build/lib  install

# MacOS
MACOSX_DEPLOYMENT_TARGET=14.5 CC=clang CXX=clang++ SETUPTOOLS_EXT_SUFFIX=.so  DEBUG=1 USE_DISTRIBUTED=0 USE_MKLDNN=0 USE_CUDA=0 USE_ROCM=0 BUILD_TEST=0 USE_FBGEMM=0 USE_NNPACK=0 USE_QNNPACK=0 USE_XNNPACK=0 USE_MPS=1 EXT_USE_LLVM_CONFIG=/root/to/llvm\@14/14.0.6/bin/llvm-config  EXT_USE_LMDB=/root/to/lmdb/0.9.33/  python setup.py install
```

## Prepare

In order to prepare the data we run

```bash
# Linux, MacOS
                              CC=clang CXX=clang++ EXT_USE_WORKSPACE=/mnt/macos/pytorch  python /mnt/macos/pytorch/extensions/llvmast/setup.py  prepare
```

The flags for LLVM are read from `extensions/llvmast/cached_cflags.py` which is
generated during the build step.

## Bibliography

- P. Prosser. Hybrid Algorithms for the Constraint Satisfaction Problem.
  Computational Intelligence. 1993.
