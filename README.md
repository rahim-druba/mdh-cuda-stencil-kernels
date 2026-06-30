# MDH CUDA Stencil Kernels

**Auto-generating optimized CUDA kernels from high-level stencil specs using the MDH framework.**

![Language](https://img.shields.io/badge/language-C%2B%2B14%20%2F%20CUDA-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Based on](https://img.shields.io/badge/based%20on-PACT%202019%20artifact-orange)

---

## What this is

This repo extends the MDH (Multi-Dimensional Homomorphism) PACT 2019 artifact with a
CUDA code generator. You write a short declarative spec describing the computation
pattern, and the generator produces a complete CUDA kernel automatically.

Three kernels are implemented and proven correct against serial CPU references:

| Kernel | Type | Grid | Speedup |
|--------|------|------|---------|
| GEMM (matmul) | 2L + 1R (map + reduction) | 10x500, R=64 | correctness proven |
| Poisson J2D5PT | 2L + 0R (pure stencil map) | 64x64 | 3.98x (7700 iterations) |
| Reservoir SpMV | 2L + 0R (variable-coeff stencil) | 4000x4000 | **36.7x** |

The reservoir result is the main contribution. A serial GMRES solver written with
CRS-format matrix storage was analyzed and its SpMV inner kernel was rewritten as
an MDH variable-coefficient stencil. The auto-generated CUDA kernel runs 36.7x faster
than the original serial C++ loop on a 4000x4000 grid.

---

## Key result

```
=== MDH Reservoir SpMV Correctness Test ===
Full grid  : 4000 x 4000
Interior   : 3998 x 3998 = 15984004 points
Grid: (125,125)  Block: (32,32)

--- Results ---
Elements checked     : 15984004
Max error vs serial  : 2.990e-07
Mismatches (>1e-10)  : 0
CPU time (serial)    : 99.385519 ms
GPU time (MDH)       : 2.706304 ms
Speedup              : 36.72x

Reservoir (MDH) SpMV is SUCCESSFUL!
```

---

## How it works

```
reservoir.cpp  (50 lines - your spec)
      |
      v
cmake + make -> ./reservoir   (MDH CUDA generator)
      |
      v
reservoir_1.cu  (1.3 MB - auto-generated CUDA kernel)
      |
      v
nvcc test_reservoir.cu reservoir_1.cu [+ config flags]
      |
      v
./test_reservoir  ->  36.7x speedup, 0 mismatches
```

The spec only describes WHAT to compute (the stencil pattern and the element-wise
function). The generator handles thread-block tiling, shared memory staging,
boundary checking, and all GPU-specific bookkeeping.

See `docs/architecture.md` for a full explanation of the generation pipeline.

---

## Repository structure

```
mdh-cuda-stencil-kernels/
|
+-- framework/                  MDH CUDA generator (the extended backend)
|   +-- include/
|   |   +-- cuda_generator.hpp  CUDA backend (new - not in original PACT artifact)
|   |   +-- cuda_input_*.hpp    CUDA wrapper classes
|   |   +-- ...                 rest of MDH framework headers
|   +-- src/                    framework library source
|   +-- md_hom_generator.hpp    top-level include for specs
|
+-- kernels/
|   +-- matmul/
|   |   +-- spec/matmul.cpp     MDH spec (25 lines)
|   |   +-- test/               test harnesses
|   |   +-- generated/          pre-generated matmul_1.cu (~20 MB)
|   |   +-- README.md
|   +-- poisson/
|   |   +-- spec/poisson.cpp    MDH spec (35 lines)
|   |   +-- test/               single-step + convergence tests
|   |   +-- generated/          pre-generated poisson_1.cu (~541 KB)
|   |   +-- README.md
|   +-- reservoir/
|       +-- spec/reservoir.cpp  MDH spec (50 lines) - main contribution
|       +-- test/               correctness + timing test
|       +-- reference/          Source.cpp - original serial GMRES solver
|       +-- generated/          pre-generated reservoir_1.cu (~1.3 MB)
|       +-- README.md
|
+-- docs/
|   +-- architecture.md         how the MDH -> CUDA pipeline works
|   +-- speedup_analysis.md     why 36x: bandwidth ceiling, float, block size
|   +-- reservoir_history.md    full development history with bugs and fixes
|   +-- reproduce.md            step-by-step commands to reproduce all results
|
+-- results/
    +-- speedup_table.md        all results in one place
    +-- progression.csv         raw numbers (500x500 -> 4000x4000 progression)
```

---

## Quick start

Requirements: GCC/G++ (C++14), CMake >= 2.8.11, CUDA toolkit, NVIDIA GPU.

```bash
git clone --recurse-submodules https://github.com/rahim-druba/mdh-cuda-stencil-kernels.git
cd mdh-cuda-stencil-kernels
mkdir build && cd build
cmake ..
make
./reservoir
```

This generates `reservoir_1.cu` in the build directory. Then compile and run the test:

```bash
nvcc ../kernels/reservoir/test/test_reservoir.cu reservoir_1.cu -o test_reservoir \
  -DTYPE_T=float -DTYPE_TS=float \
  -DCACHE_L_CB=0 -DCACHE_P_CB=0 \
  -DG_CB_RES_DEST_LEVEL=2 \
  -DG_CB_SIZE_L_1=3998 -DG_CB_SIZE_L_2=3998 \
  -DL_CB_RES_DEST_LEVEL=1 -DL_CB_SIZE_L_1=32 -DL_CB_SIZE_L_2=32 \
  -DP_CB_RES_DEST_LEVEL=0 -DP_CB_SIZE_L_1=1  -DP_CB_SIZE_L_2=1  \
  -DNUM_WG_L_1=125 -DNUM_WG_L_2=125 \
  -DNUM_WI_L_1=32  -DNUM_WI_L_2=32 \
  -DOCL_DIM_L_1=1  -DOCL_DIM_L_2=0

./test_reservoir
```

For all three kernels with exact commands, see `docs/reproduce.md`.

---

## The CRS to stencil insight

The original `Source.cpp` stores matrix A in CRS (Compressed Row Storage) format and
loops over it the standard way:

```cpp
for (i = 0; i < n; i++) {
    temp = 0;
    for (j = A->RowIndex[i]; j < A->RowIndex[i+1]; j++)
        temp += A->Value[j] * x0[A->Col[j]];
    r[i] = b[i] - temp;
}
```

CRS hides the structure. But this is a 2D PDE on a regular grid - the matrix A
always has exactly 5 non-zeros per row and they are always the north/south/east/west
neighbors plus the center. Once you recognize that, you can bypass CRS entirely,
precompute the five coefficient arrays from the mobility tensors on CPU, and express
the whole thing as a 5-point stencil that MDH can generate as CUDA automatically.

This pattern applies to any regular-grid PDE solver that wraps a structured stencil
in CRS for library compatibility.

---

## Speedup progression (reservoir)

| Grid | Precision | Block | CPU | GPU | Speedup |
|------|-----------|-------|-----|-----|---------|
| 500x500 | double | 16x16 | 1.56 ms | 0.78 ms | 2.0x |
| 2000x2000 | double | 32x32 | 21.0 ms | 1.35 ms | 15.5x |
| 4000x4000 | double | 32x32 | 85 ms | 5.37 ms | 15.8x |
| 4000x4000 | float | 32x32 | 99 ms | 2.71 ms | **36.7x** |

Double precision hits a bandwidth ceiling at ~16x. Float halves bytes per element
and breaks through to 36.7x. Full analysis in `docs/speedup_analysis.md`.

---

## Background

This work extends the MDH artifact from PACT 2019:

> Hagedorn, B., Lenfers, L., Koehler, T., Qin, X., Gorlatch, S., Steuwer, M.
> "Achieving high-performance the functional way: a functional pearl on expressing
> high-performance optimizations as rewrite rules."
> ICFP 2020. (Related: PACT 2019 artifact)

The original artifact generates OpenCL. This repo adds `framework/include/cuda_generator.hpp`
as a drop-in CUDA backend. The MDH spec files are identical for both backends - you
just call `cuda_generator(md_hom_X)` instead of `ocl_generator(md_hom_X)`.

Original artifact: https://gitlab.com/mdh-project/pact_2019_artifact

---

## License

MIT - see `LICENSE`.

Framework code in `framework/` originates from the PACT 2019 artifact and is used
under the terms of its original license. The CUDA generator headers, kernel specs,
test harnesses, and all documentation in this repo are original work.
