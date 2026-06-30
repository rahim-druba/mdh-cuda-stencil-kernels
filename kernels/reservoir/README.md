# Reservoir - Variable-Coefficient SpMV from GMRES

MDH shape: 2L + 0R (pure map, no reduction).

## Background

`reference/Source.cpp` is a serial C++ GMRES solver for the 2-phase porous media flow
(oil reservoir simulation) pressure equation on a 50x50 grid. The most expensive
inner operation inside every GMRES iteration is the SpMV:

```cpp
for (i = 0; i < n; i++) {
    temp = 0;
    for (j = A->RowIndex[i]; j < A->RowIndex[i+1]; j++)
        temp += A->Value[j] * x0[A->Col[j]];
    r[i] = b[i] - temp;
}
```

Although matrix A is stored in CRS (Compressed Row Storage) format, it encodes a regular
2D 5-point stencil. The a/b/c/d/e labels in Source.cpp comments are literally the five
stencil directions. By recognizing this structure, the entire SpMV can be expressed as
an MDH stencil and auto-generated as a CUDA kernel.

## What it computes

```
W[i,j] = COEFF_UP[i,j]     * V[i-1,j]
        + COEFF_LEFT[i,j]   * V[i,j-1]
        + COEFF_CENTER[i,j] * V[i,j]
        + COEFF_RIGHT[i,j]  * V[i,j+1]
        + COEFF_DOWN[i,j]   * V[i+1,j]
```

Unlike the Poisson stencil (constant 0.25 weights), the coefficients vary per grid point.
They come from the phase mobility tensors Mx and My and are precomputed on CPU before
each kernel call.

## Coefficient precomputation

```
COEFF_UP[i,j]     = 0.5 * (Mx[i,j]   + Mx[i-1,j])
COEFF_LEFT[i,j]   = 0.5 * (My[i,j]   + My[i,j-1])
COEFF_RIGHT[i,j]  = 0.5 * (My[i,j+1] + My[i,j])
COEFF_DOWN[i,j]   = 0.5 * (Mx[i+1,j] + Mx[i,j])
COEFF_CENTER[i,j] = -(UP + LEFT + RIGHT + DOWN + h^2/dt)
```

## MDH spec

`spec/reservoir.cpp` - 50 lines. V uses the same 5-point cross neighborhood as Poisson.
The five COEFF arrays are plain `input_buffer` objects (not stencil buffers) since
they are evaluated at the center point only.

## Generated kernel

`generated/reservoir_1.cu` - auto-generated, ~1.3 MB.

## Test

`test/test_reservoir.cu` - correctness + timing on a 4000x4000 grid:

```
Elements checked  : 15984004
Max error         : 2.990e-07   (within float32 precision)
Mismatches        : 0
CPU time          : ~99 ms
GPU time          : ~2.7 ms
Speedup           : ~36x
```

## Build command (float, 4000x4000 grid)

```bash
nvcc test/test_reservoir.cu generated/reservoir_1.cu -o test_reservoir \
  -DTYPE_T=float -DTYPE_TS=float \
  -DCACHE_L_CB=0 -DCACHE_P_CB=0 \
  -DG_CB_RES_DEST_LEVEL=2 \
  -DG_CB_SIZE_L_1=3998 -DG_CB_SIZE_L_2=3998 \
  -DL_CB_RES_DEST_LEVEL=1 -DL_CB_SIZE_L_1=32 -DL_CB_SIZE_L_2=32 \
  -DP_CB_RES_DEST_LEVEL=0 -DP_CB_SIZE_L_1=1  -DP_CB_SIZE_L_2=1  \
  -DNUM_WG_L_1=125 -DNUM_WG_L_2=125 \
  -DNUM_WI_L_1=32  -DNUM_WI_L_2=32 \
  -DOCL_DIM_L_1=1  -DOCL_DIM_L_2=0
```
