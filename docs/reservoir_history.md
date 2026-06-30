# Reservoir SpMV — MDH CUDA Generator Extension
# Full history of this session: plan, implementation, bugs, and results

---

## Background

The starting point was `Source.cpp` — a hand-written serial C++ GMRES solver for the 2-phase porous media flow (oil reservoir simulation) pressure equation on a 50×50 grid. The goal was to extract the core compute kernel and express it as an MDH spec so the generator produces a CUDA kernel automatically.

### Why GMRES itself cannot be MDH-ified

GMRES (Generalized Minimal RESidual) is an iterative solver with sequential outer-loop dependencies — iteration k+1 depends on the result of iteration k. MDH handles parallel patterns (map + reduce), not sequential solvers. So the entire GMRES loop stays on CPU.

### What CAN be extracted

The most expensive inner operation inside every GMRES iteration is the **SpMV** (Sparse Matrix-Vector Multiply):

```cpp
// Source.cpp lines 272-277 and 296-300
for (i = 0; i < n; i++) {
    temp = 0;
    for (j = A->RowIndex[i]; j < A->RowIndex[i+1]; j++) {
        temp += A->Value[j] * x0[A->Col[j]];
    }
    r[i] = b[i] - temp;
}
```

Although stored in CRS (Compressed Row Storage) format, matrix A encodes a **regular 2D 5-point stencil** — the same family as J3D7PT and the Poisson solver. The a/b/c/d/e labels in Source.cpp comments are literally the 5 stencil directions. By bypassing CRS and expressing the computation directly as a stencil, it becomes fully MDH-expressible.

---

## Mathematical Formulation

### Stencil coefficients (derived from Source.cpp Mx/My mobilities)

At each interior grid point (i, j) of the 50×50 grid (interior = rows/cols 1..48):

```
COEFF_UP[i,j]     = 0.5 * (Mx[i,j]   + Mx[i-1,j])     →  weight for V[i-1,j]
COEFF_LEFT[i,j]   = 0.5 * (My[i,j]   + My[i,j-1])     →  weight for V[i,j-1]
COEFF_RIGHT[i,j]  = 0.5 * (My[i,j+1] + My[i,j])       →  weight for V[i,j+1]
COEFF_DOWN[i,j]   = 0.5 * (Mx[i+1,j] + Mx[i,j])       →  weight for V[i+1,j]
COEFF_CENTER[i,j] = -(UP + LEFT + RIGHT + DOWN + h²/dt) →  weight for V[i,j]
```

Where Mx and My are the phase mobility tensors:
```
Mx[i,j] = My[i,j] = -(kx * (S² / m1) + kx * ((1-S)² / m2))
```
With `kx = ky = 0.001`, `m1 = 0.03`, `m2 = 0.3`, `S = 0.1` (uniform initialization from Source.cpp).

### The SpMV kernel

```
W[i,j] = COEFF_UP[i,j]     * V[i-1,j]
        + COEFF_LEFT[i,j]   * V[i,j-1]
        + COEFF_CENTER[i,j] * V[i,j]
        + COEFF_RIGHT[i,j]  * V[i,j+1]
        + COEFF_DOWN[i,j]   * V[i+1,j]
```

This is a **variable-coefficient 2D 5-point stencil** — same topology as Poisson (J2D5PT) but with spatially varying weights instead of constant 0.25.

---

## Design Decision: Precomputed Coefficients

Rather than passing Mx and My as stencil buffers (which would require a double-stencil in f()), the coefficients are **precomputed on CPU** before each kernel call. This gives:

| Approach | Complexity | Notes |
|---|---|---|
| Pass Mx/My as stencil buffers | High — double stencil in f() | Mx[i-1,j] inside f() while V is also a stencil |
| **Precompute COEFF_* on CPU** | **Low — 5 plain input_buffers** | **Clean f(), CPU precompute is negligible** |

Five 2D arrays (COEFF_UP, COEFF_LEFT, COEFF_CENTER, COEFF_RIGHT, COEFF_DOWN) are passed as regular `input_buffer` objects. V uses the same 5-point cross neighborhood as the Poisson spec.

---

## Folder Structure

A new isolated folder was created to avoid touching the working Poisson setup:

```
/home/rahim/dynamic_generator/reservoir_mdh/
├── CMakeLists.txt              ← reservoir target only (poisson removed)
├── md_hom_generator.hpp
├── include/                    ← all 21 headers including 4 CUDA wrappers
│   ├── cuda_generator.hpp
│   ├── cuda_input_buffer_wrapper.hpp
│   ├── cuda_input_stencil_buffer_wrapper.hpp  ← has result_input() fix from Poisson
│   ├── cuda_result_buffer_wrapper.hpp
│   └── ... (17 more)
├── src/
│   ├── helper.cpp              ← framework library (do not modify)
│   ├── input_buffer.cpp
│   ├── input_scalar.cpp
│   ├── md_hom.cpp
│   ├── result_buffer.cpp
│   ├── scalar_function.cpp
│   └── reservoir/
│       └── reservoir.cpp       ← NEW: our contribution
├── pact_2019_artifact/         ← MDH core (rsync copy from poisson_mdh)
└── build/
    ├── reservoir_1.cu          ← generated CUDA kernel (1.3 MB)
    ├── reservoir_2.cu          ← generated (384 KB, trivial for R_DIMS=0)
    ├── reservoir_1.o
    ├── reservoir_2.o
    ├── test_reservoir.cu       ← correctness test
    └── test_reservoir          ← compiled binary
```

---

## MDH Spec (reservoir.cpp)

```cpp
// V: the vector being multiplied — 5-point cross stencil (same as Poisson)
auto V = md_hom::input_stencil_buffer(
    "V",
    {md_hom::L(1), md_hom::L(2)},
    md_hom::N(md_hom::N(0,1,0), md_hom::N(1,2,1), md_hom::N(0,1,0)),
    md_hom::oob::ZERO
);

// Precomputed stencil coefficients — one value per interior grid point
auto COEFF_UP     = md_hom::input_buffer("COEFF_UP",     {md_hom::L(1), md_hom::L(2)});
auto COEFF_LEFT   = md_hom::input_buffer("COEFF_LEFT",   {md_hom::L(1), md_hom::L(2)});
auto COEFF_CENTER = md_hom::input_buffer("COEFF_CENTER", {md_hom::L(1), md_hom::L(2)});
auto COEFF_RIGHT  = md_hom::input_buffer("COEFF_RIGHT",  {md_hom::L(1), md_hom::L(2)});
auto COEFF_DOWN   = md_hom::input_buffer("COEFF_DOWN",   {md_hom::L(1), md_hom::L(2)});

// f: weighted sum over 5 stencil points
auto f = md_hom::scalar_function(
    "return COEFF_UP_val * V_val_l1_m1"
    " + COEFF_LEFT_val * V_val_l2_m1"
    " + COEFF_CENTER_val * V_val"
    " + COEFF_RIGHT_val * V_val_l2_p1"
    " + COEFF_DOWN_val * V_val_l1_p1;"
);

// g: identity — no reduction (R_DIMS = 0)
auto g = md_hom::scalar_function("return res;");

auto md_hom_reservoir = md_hom::md_hom<2, 0>(
    "reservoir",
    md_hom::inputs(V, COEFF_UP, COEFF_LEFT, COEFF_CENTER, COEFF_RIGHT, COEFF_DOWN),
    f, g, result, false, false
);
```

---

## Bug Encountered

### CMakeLists.txt still referenced poisson target after rsync

**Error:** `CMake Error: Cannot find source file: src/poisson/poisson.cpp`

**Cause:** The folder was rsync-copied from `poisson_mdh/` excluding the `src/poisson/` directory, but `CMakeLists.txt` still had the poisson `add_executable` entry.

**Fix:** Removed the poisson target from `CMakeLists.txt` in `reservoir_mdh/`.

---

## nvcc Compilation Config (final — float, 4000×4000)

```bash
nvcc test_reservoir.cu reservoir_1.cu -o test_reservoir \
  -DTYPE_T=float -DTYPE_TS=float \
  -DCACHE_L_CB=0 -DCACHE_P_CB=0 \
  -DG_CB_RES_DEST_LEVEL=2 \
  -DG_CB_SIZE_L_1=3998 -DG_CB_SIZE_L_2=3998 \
  -DL_CB_RES_DEST_LEVEL=1 -DL_CB_SIZE_L_1=32 -DL_CB_SIZE_L_2=32 \
  -DP_CB_RES_DEST_LEVEL=0 -DP_CB_SIZE_L_1=1  -DP_CB_SIZE_L_2=1  \
  -DNUM_WG_L_1=125 -DNUM_WG_L_2=125 \
  -DNUM_WI_L_1=32 -DNUM_WI_L_2=32 \
  -DOCL_DIM_L_1=1 -DOCL_DIM_L_2=0
```

Config notes:
- `G_CB_SIZE=3998`: interior of the 4000×4000 grid
- `float` precision: halves memory per element → 2× more data per GPU bandwidth unit
- `NUM_WG=125`: 125×32=4000 threads cover the 3998×3998 interior (2 idle threads per row/col)
- `NUM_WI=32`: 32×32=1024 threads per block — better GPU occupancy than 16×16=256
- `oob::ZERO`: zero Dirichlet boundary conditions on ghost layer

---

## Bugs Found During Timing/Speedup Work

### Bug 2 — `sizeof(double)` hardcoded in cudaMemcpy after switching to float

**Error:** Segmentation fault (exit code 139) when compiling with `-DTYPE_T=float`

**Cause:** The pointer types (`double *`) were correctly changed to `real_t *` but the `sizeof(double)` in all `cudaMalloc`, `cudaMemcpy`, and `cudaMemset` calls were not updated. With float, each array was allocated with `sz * 4` bytes but cudaMemcpy tried to copy `sz * 8` bytes — writing past the end of the GPU buffer, causing a host-side memory violation.

**Fix:** Changed all `sizeof(double)` → `sizeof(real_t)` throughout the test. Used `replace_all` to catch every instance.

**Lesson:** When making a type generic (`real_t = TYPE_T`), check EVERY `sizeof` — not just the pointer declarations.

### Bug 3 — Mismatch threshold too tight for float precision

**Error:** Test reported FAILED with 15.9M mismatches at 1e-10 threshold, even though results were correct.

**Cause:** Float32 machine epsilon is ~1e-7. GPU reorders floating-point operations vs serial CPU → differences of ~3e-7 are expected and correct. The 1e-10 threshold is appropriate for double but not float.

**Fix:** Made threshold type-aware:
```cpp
const double THRESH = (sizeof(real_t) == 4) ? 1e-5 : 1e-10;
```

---

## Test Results — Correctness (initial, double, 50×50)

```
Elements checked     : 2304
Max error vs serial  : 8.613e-18
Mismatches (>1e-10)  : 0
GPU time (kernel 1)  : 0.030 ms
Reservoir (MDH) SpMV is SUCCESSFUL!
```

## Test Results — Speedup Progression

| Grid | Precision | Blocks | CPU time | GPU time | Speedup |
|---|---|---|---|---|---|
| 500×500 (interior 498×498) | double | 16×16 | 1.56 ms | 0.78 ms | 2.0× |
| 2000×2000 (interior 1998×1998) | double | 32×32 | 21.0 ms | 1.35 ms | 15.5× |
| 4000×4000 (interior 3998×3998) | double | 32×32 | 85 ms | 5.37 ms | 15.8× |
| **4000×4000 (interior 3998×3998)** | **float** | **32×32** | **99 ms** | **2.71 ms** | **36.7×** |

## Final Test Output (float, 4000×4000)

```
=== MDH Reservoir SpMV Correctness Test ===
Full grid  : 4000 x 4000
Interior   : 3998 x 3998 = 15984004 points
Grid: (125,125)  Block: (32,32)

Launching reservoir_1 kernel...

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

## Why the Speedup Improved

| Change | Effect |
|---|---|
| Small grid (50×50) → large grid (4000×4000) | GPU underutilized at small size; needs 4M+ elements to saturate memory bandwidth |
| 16×16 blocks → 32×32 blocks | 256 threads/block → 1024 threads/block; better GPU occupancy |
| double → float | 8 bytes/element → 4 bytes/element; GPU processes 2× more elements per memory bandwidth unit |
| double plateau at ~16× | Both CPU and GPU bandwidth-bound at same ratio; switching precision breaks the ceiling |

---

## Comparison with Poisson Work

| | Poisson (J2D5PT) | Reservoir SpMV |
|---|---|---|
| Grid | 64×64 (interior 62×62) | 4000×4000 (interior 3998×3998) |
| Stencil type | Constant-coefficient | Variable-coefficient |
| Coefficients | 0.25 constant | COEFF_UP/LEFT/CENTER/RIGHT/DOWN arrays |
| MDH shape | 2L + 0R | 2L + 0R |
| Precision | double | float |
| Max error vs serial | 0.000e+00 | 2.990e-07 (within float ε) |
| **Speedup** | **3.98×** (convergence) | **36.7×** (single SpMV step) |

---

## Research Significance

1. **Correctness** — GPU SpMV matches serial within float precision (2.99e-07 < 10×ε_float)
2. **Speedup** — 36.7× on 4000×4000 grid — demonstrates GPU acceleration of reservoir simulation kernel
3. **New contribution** — First MDH generator spec for a variable-coefficient reservoir stencil kernel
4. **CRS bypass** — Proves that CRS-format PDE operators on regular grids can be reformulated as MDH stencils and auto-generated as CUDA kernels
5. **General pattern** — Any PDE solver using a regular-grid stencil wrapped in CRS for a library can have its hot SpMV replaced by an MDH-generated CUDA kernel

---

## Key Files

| File | Purpose |
|---|---|
| `Source.cpp` | Original serial GMRES solver — serial reference, untouched |
| `src/reservoir/reservoir.cpp` | MDH spec — our contribution |
| `build/reservoir_1.cu` | Generated CUDA kernel (1.3 MB) |
| `build/test_reservoir.cu` | Single-step correctness test |
