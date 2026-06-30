# Speedup Analysis - Why 36x on the Reservoir SpMV

## Progression

| Grid | Precision | Block size | CPU time | GPU time | Speedup |
|------|-----------|------------|----------|----------|---------|
| 500x500 (498x498 interior) | double | 16x16 | 1.56 ms | 0.78 ms | 2.0x |
| 2000x2000 (1998x1998 interior) | double | 32x32 | 21.0 ms | 1.35 ms | 15.5x |
| 4000x4000 (3998x3998 interior) | double | 32x32 | 85 ms | 5.37 ms | 15.8x |
| 4000x4000 (3998x3998 interior) | float | 32x32 | 99 ms | 2.71 ms | 36.7x |

## Factor 1 - Problem size matters

At 500x500 (roughly 250K elements), the GPU is not busy. A modern GPU needs millions
of elements to fill its memory pipeline and amortize kernel launch overhead. Moving
to 4000x4000 (16M elements) gives the GPU enough work to sustain near-peak bandwidth
throughput.

## Factor 2 - Block size affects occupancy

16x16 = 256 threads per block. 32x32 = 1024 threads per block. A higher thread count
per block means the GPU scheduler can hide memory latency better by switching between
warps. With 125x125 blocks of 32x32 threads, the total thread count is 16M - exactly
matching the problem size with no wasted launches.

## Factor 3 - Float precision breaks through the bandwidth ceiling

At double precision the speedup plateaus around 15-16x regardless of grid size.
The reason: both the CPU and GPU are memory-bandwidth limited for this kernel (it reads
6 arrays and writes 1, with very little arithmetic reuse). When CPU and GPU are both
saturated at their respective bandwidths, the ratio stays fixed.

Switching to float halves the bytes per element:
- double: 8 bytes x 7 arrays x 16M points = ~896 MB transferred
- float:  4 bytes x 7 arrays x 16M points = ~448 MB transferred

The GPU's memory bandwidth is now serving the same number of elements with half the
traffic, effectively doubling the useful throughput and breaking past the ~16x ceiling.

## Why the CPU gets slightly slower in float

Counter-intuitively, the CPU is about 15% slower in float (99 ms vs 85 ms).
This is because the CPU reference uses double precision arithmetic internally
even when array values are stored as float - the intermediate sums are computed
in double, adding conversion overhead.

## Summary

| Change | Speedup gain |
|--------|-------------|
| 500x500 -> 4000x4000 | 2x -> 15x (GPU starts to saturate) |
| 16x16 blocks -> 32x32 blocks | small gain from better occupancy |
| double -> float | 15x -> 36x (bandwidth ceiling lifted) |
