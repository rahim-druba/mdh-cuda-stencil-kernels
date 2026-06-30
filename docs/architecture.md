# How MDH Kernel Generation Works

## The MDH Model

MDH (Multi-Dimensional Homomorphism) is a functional parallel pattern that captures
a large class of numerical kernels. An MDH computation has the form:

```
result[l1, l2, ...] = g( reduce_r( f(inputs at (l1, l2, ..., r1, r2, ...)) ) )
```

Where:
- L dimensions (L1, L2, ...) are the parallel output dimensions
- R dimensions (R1, R2, ...) are reduction dimensions (contracted away)
- f() is the per-element function applied at every point
- g() is a post-processing function on the reduced result

The spec file declares L dims, R dims, inputs with their access patterns, and the f/g
functions as inline strings. The generator handles all thread-block tiling, shared memory
staging, and boundary checks automatically.

## The CUDA Generator

The original PACT 2019 artifact only generates OpenCL. This repo adds a CUDA backend
(`framework/include/cuda_generator.hpp`) that mirrors the OpenCL generator output but
produces valid CUDA C++ using `__global__` kernels, `__shared__` memory, and
`cudaEventElapsedTime` compatible timing.

The generator is parameterized via `-D` flags passed to nvcc at compile time:

| Flag | Meaning |
|------|---------|
| G_CB_SIZE_L_1, G_CB_SIZE_L_2 | Global (total) problem size per loop dimension |
| NUM_WG_L_1, NUM_WG_L_2 | Number of work-groups (grid blocks) per dimension |
| NUM_WI_L_1, NUM_WI_L_2 | Work-items per work-group (block size) per dimension |
| L_CB_SIZE_L_1, L_CB_SIZE_L_2 | Local cache tile size |
| TYPE_T, TYPE_TS | Floating point type (float or double) |

## Generation pipeline

```
spec/X.cpp   (MDH spec - your input)
      |
      v
cmake + make
      |
      v
./X            (runs the generator - silent, writes files)
      |
      v
X_1.cu         (main compute kernel - the output)
X_2.cu         (reduction cleanup kernel, trivial for R_DIMS=0)
      |
      v
nvcc test/test_X.cu X_1.cu -o test_X  [+ -D config flags]
      |
      v
./test_X       (correctness + timing)
```

## What each kernel spec does

| Spec | L dims | R dims | Key input type | Output |
|------|--------|--------|----------------|--------|
| matmul.cpp | 2 | 1 | input_buffer | dot product per (i,j) |
| poisson.cpp | 2 | 0 | input_stencil_buffer | Jacobi step |
| reservoir.cpp | 2 | 0 | input_stencil_buffer + 5x input_buffer | variable-coeff SpMV |

## Stencil neighborhood notation

The neighborhood object passed to `input_stencil_buffer` encodes which offsets
are accessed. The 5-point cross used by both Poisson and Reservoir is:

```cpp
md_hom::N(
    md_hom::N(0,1,0),    // L1: no offset in this dimension (size 1)
    md_hom::N(1,2,1),    // L1: -1, center, +1 (size 3 with 1 padding each side)
    md_hom::N(0,1,0)     // L2: no offset in this dimension (size 1)
)
// combined with the transpose for L2 gives top/bottom/left/right/center
```

`oob::ZERO` means out-of-bounds accesses (boundary) return 0 - Dirichlet BC.
