# Poisson - Jacobi 2D 5-Point Stencil (J2D5PT)

MDH shape: 2L + 0R (pure map, no reduction).

## What it computes

One Jacobi iteration step for the 2D Poisson equation on a uniform grid:

```
U_new[i,j] = 0.25 * (U[i-1,j] + U[i+1,j] + U[i,j-1] + U[i,j+1] + h^2 * SOURCE[i,j])
```

Boundary condition: U = 0 at all ghost layer points (Dirichlet, oob::ZERO).

## MDH spec

`spec/poisson.cpp` - 35 lines. Uses `input_stencil_buffer` for U with a 5-point cross
neighborhood. SOURCE is a plain `input_buffer`. h^2 is passed as a scalar.

## Generated kernel

`generated/poisson_1.cu` - auto-generated, ~541 KB.

## Tests

`test/test_poisson.cu` - single-step correctness: 3844 elements (62x62 interior),
max error 0.000e+00, 0 mismatches.

`test/test_poisson_converge.cu` - full convergence run: 7700 Jacobi iterations until
residual < 1e-6. GPU matches serial CPU result, 3.98x speedup.

## Build command

```bash
nvcc test/test_poisson.cu generated/poisson_1.cu -o test_poisson \
  -DTYPE_T=double -DTYPE_TS=double \
  -DCACHE_L_CB=0 -DCACHE_P_CB=0 \
  -DG_CB_RES_DEST_LEVEL=2 \
  -DG_CB_SIZE_L_1=62 -DG_CB_SIZE_L_2=62 \
  -DL_CB_RES_DEST_LEVEL=1 -DL_CB_SIZE_L_1=16 -DL_CB_SIZE_L_2=16 \
  -DP_CB_RES_DEST_LEVEL=0 -DP_CB_SIZE_L_1=1  -DP_CB_SIZE_L_2=1  \
  -DNUM_WG_L_1=4  -DNUM_WG_L_2=4 \
  -DNUM_WI_L_1=16 -DNUM_WI_L_2=16 \
  -DOCL_DIM_L_1=1 -DOCL_DIM_L_2=0
```
