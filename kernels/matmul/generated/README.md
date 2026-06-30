# matmul_1.cu - Generated Kernel

The generated CUDA kernel for GEMM is ~20 MB and is not committed to keep the
repository size manageable.

To generate it, follow the steps in `docs/reproduce.md`:

```bash
mkdir build && cd build
cmake ..
make
./matmul
# writes matmul_1.cu here
```

The kernel file is ~20 MB because GEMM has two loop dimensions and one reduction
dimension (2L + 1R), which results in many unrolled code paths for different tiling
configurations.
