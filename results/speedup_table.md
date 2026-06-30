# All Results Summary

## Kernel results

| Kernel | Grid | Precision | CPU time | GPU time | Speedup | Elements | Max error | Mismatches |
|--------|------|-----------|----------|----------|---------|----------|-----------|------------|
| GEMM (matmul) | 10x500, R=64 | float | - | 0.085 ms | - | 5000 | 5.72e-06 | 0 |
| Poisson J2D5PT | 64x64 (62x62 interior) | double | - | 0.040 ms | 3.98x (convergence) | 3844 | 0.000e+00 | 0 |
| Reservoir SpMV | 4000x4000 (3998x3998) | float | 99.4 ms | 2.71 ms | 36.7x | 15984004 | 2.990e-07 | 0 |

## Reservoir speedup progression

| Grid | Precision | Block | CPU time | GPU time | Speedup |
|------|-----------|-------|----------|----------|---------|
| 500x500 | double | 16x16 | 1.56 ms | 0.78 ms | 2.0x |
| 2000x2000 | double | 32x32 | 21.0 ms | 1.35 ms | 15.5x |
| 4000x4000 | double | 32x32 | 85 ms | 5.37 ms | 15.8x |
| 4000x4000 | float | 32x32 | 99.4 ms | 2.71 ms | **36.7x** |

Key insight: double precision hits a bandwidth ceiling at ~16x. Switching to float
halves memory traffic per element and breaks through to 36.7x.
