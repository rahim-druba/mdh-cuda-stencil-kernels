# GEMM - Matrix Multiplication

MDH shape: 2L + 1R (two parallel loop dimensions, one reduction dimension).

## What it computes

```
S[i, j] = sum_k ( Z[i, k] * W[j, k] )
```

Z is the input matrix (batch x features), W is the weight matrix (output_features x features),
S is the result (batch x output_features).

## MDH spec

`spec/matmul.cpp` - 25 lines. The reduction over k (R1) collapses into a dot product
per output element. MDH handles the parallel mapping over (L1, L2) and the serial
reduction over R1 automatically in the generated kernel.

## Generated kernel

`generated/matmul_1.cu` - auto-generated, ~20 MB. Kernel 2 handles the reduction
step when R1 exceeds one work-group.

## Test

`test/test_matmul.cu` - correctness check: 5000 elements, max error 5.72e-06, 0 mismatches.
`test/test_cpu_vs_gpu.cu` - side-by-side CPU vs GPU output with timing.

## Build command (after generating matmul_1.cu)

```bash
nvcc test/test_matmul.cu generated/matmul_1.cu -o test_matmul \
  -DTYPE_T=float -DTYPE_TS=float \
  -DCACHE_L_CB=0 -DCACHE_P_CB=0 \
  -DG_CB_RES_DEST_LEVEL=2 -DL_CB_RES_DEST_LEVEL=1 -DP_CB_RES_DEST_LEVEL=0 \
  -DG_CB_SIZE_L_1=10 -DG_CB_SIZE_L_2=500 -DG_CB_SIZE_R_1=64 \
  -DL_CB_SIZE_L_1=8  -DL_CB_SIZE_L_2=16  -DL_CB_SIZE_R_1=64 \
  -DP_CB_SIZE_L_1=1  -DP_CB_SIZE_L_2=1   -DP_CB_SIZE_R_1=1  \
  -DNUM_WG_L_1=2 -DNUM_WG_L_2=32 -DNUM_WG_R_1=1 \
  -DNUM_WI_L_1=4 -DNUM_WI_L_2=32 -DNUM_WI_R_1=8 \
  -DOCL_DIM_L_1=2 -DOCL_DIM_L_2=1 -DOCL_DIM_R_1=0
```
