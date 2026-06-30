# Reproducing All Results

Requirements: GCC/G++ with C++14, CMake >= 2.8.11, CUDA toolkit with nvcc, NVIDIA GPU.

All commands run from the repo root unless noted otherwise.

---

## Step 1 - Build the generator executables

```bash
mkdir build && cd build
cmake ..
make
```

Expected output:
```
[100%] Built target matmul
[100%] Built target poisson
[100%] Built target reservoir
```

---

## Step 2 - Generate CUDA kernels

Run each generator from the build directory. Each writes its .cu output silently.

```bash
cd build
./matmul
./poisson
./reservoir
```

Confirm the files were created:
```bash
ls -lh matmul_1.cu poisson_1.cu reservoir_1.cu
```

Expected sizes: matmul_1.cu ~20 MB, poisson_1.cu ~541 KB, reservoir_1.cu ~1.3 MB.

Note: these are the same files already committed in kernels/*/generated/ for reference.

---

## Step 3 - Test matmul

```bash
nvcc ../kernels/matmul/test/test_matmul.cu matmul_1.cu -o test_matmul \
  -DTYPE_T=float -DTYPE_TS=float \
  -DCACHE_L_CB=0 -DCACHE_P_CB=0 \
  -DG_CB_RES_DEST_LEVEL=2 -DL_CB_RES_DEST_LEVEL=1 -DP_CB_RES_DEST_LEVEL=0 \
  -DG_CB_SIZE_L_1=10 -DG_CB_SIZE_L_2=500 -DG_CB_SIZE_R_1=64 \
  -DL_CB_SIZE_L_1=8  -DL_CB_SIZE_L_2=16  -DL_CB_SIZE_R_1=64 \
  -DP_CB_SIZE_L_1=1  -DP_CB_SIZE_L_2=1   -DP_CB_SIZE_R_1=1  \
  -DNUM_WG_L_1=2 -DNUM_WG_L_2=32 -DNUM_WG_R_1=1 \
  -DNUM_WI_L_1=4 -DNUM_WI_L_2=32 -DNUM_WI_R_1=8 \
  -DOCL_DIM_L_1=2 -DOCL_DIM_L_2=1 -DOCL_DIM_R_1=0

./test_matmul
```

Expected: 5000 elements, max error 5.72e-06, 0 mismatches, SUCCESSFUL.

---

## Step 4 - Test Poisson (single step)

```bash
nvcc ../kernels/poisson/test/test_poisson.cu poisson_1.cu -o test_poisson \
  -DTYPE_T=double -DTYPE_TS=double \
  -DCACHE_L_CB=0 -DCACHE_P_CB=0 \
  -DG_CB_RES_DEST_LEVEL=2 \
  -DG_CB_SIZE_L_1=62 -DG_CB_SIZE_L_2=62 \
  -DL_CB_RES_DEST_LEVEL=1 -DL_CB_SIZE_L_1=16 -DL_CB_SIZE_L_2=16 \
  -DP_CB_RES_DEST_LEVEL=0 -DP_CB_SIZE_L_1=1  -DP_CB_SIZE_L_2=1  \
  -DNUM_WG_L_1=4  -DNUM_WG_L_2=4 \
  -DNUM_WI_L_1=16 -DNUM_WI_L_2=16 \
  -DOCL_DIM_L_1=1 -DOCL_DIM_L_2=0

./test_poisson
```

Expected: 3844 elements, max error 0.000e+00, 0 mismatches, SUCCESSFUL.

## Step 4b - Test Poisson (convergence)

```bash
nvcc ../kernels/poisson/test/test_poisson_converge.cu poisson_1.cu -o test_poisson_converge \
  -DTYPE_T=double -DTYPE_TS=double \
  -DCACHE_L_CB=0 -DCACHE_P_CB=0 \
  -DG_CB_RES_DEST_LEVEL=2 \
  -DG_CB_SIZE_L_1=62 -DG_CB_SIZE_L_2=62 \
  -DL_CB_RES_DEST_LEVEL=1 -DL_CB_SIZE_L_1=16 -DL_CB_SIZE_L_2=16 \
  -DP_CB_RES_DEST_LEVEL=0 -DP_CB_SIZE_L_1=1  -DP_CB_SIZE_L_2=1  \
  -DNUM_WG_L_1=4  -DNUM_WG_L_2=4 \
  -DNUM_WI_L_1=16 -DNUM_WI_L_2=16 \
  -DOCL_DIM_L_1=1 -DOCL_DIM_L_2=0

./test_poisson_converge
```

Expected: ~7700 iterations to convergence, speedup ~3.98x.

---

## Step 5 - Test Reservoir SpMV (main result)

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

Expected output:
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
CPU time (serial)    : ~99 ms
GPU time (MDH)       : ~2.7 ms
Speedup              : ~36x

Reservoir (MDH) SpMV is SUCCESSFUL!
```

---

## Using pre-generated kernels

If you want to skip the generator step and use the committed reference kernels:

```bash
cd build
cp ../kernels/reservoir/generated/reservoir_1.cu .
cp ../kernels/poisson/generated/poisson_1.cu .
cp ../kernels/matmul/generated/matmul_1.cu .
```

Then run the nvcc commands above from the build directory.
