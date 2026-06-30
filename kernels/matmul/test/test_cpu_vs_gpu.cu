// CPU vs GPU comparison test for MDH matmul.
// Prints side-by-side values, timing for both CPU and GPU.
//
// Build command is in cpu_gpu_compare.md

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <cuda_runtime.h>

static const dim3 BLOCK1 { NUM_WI_R_1, NUM_WI_L_2, NUM_WI_L_1 };
static const dim3 GRID1  { NUM_WG_R_1, NUM_WG_L_2, NUM_WG_L_1 };

static const int  K2_BLOCK_X = (NUM_WG_R_1 < NUM_WI_R_1) ? NUM_WG_R_1 : NUM_WI_R_1;
static const dim3 BLOCK2 { (unsigned)K2_BLOCK_X, NUM_WI_L_2, NUM_WI_L_1 };
static const dim3 GRID2  { 1, NUM_WG_L_2, NUM_WG_L_1 };

static const int NL1 = G_CB_SIZE_L_1;
static const int NL2 = G_CB_SIZE_L_2;
static const int NR1 = G_CB_SIZE_R_1;

extern __global__ void matmul_1(
    float const * const __restrict__ Z,
    float const * const __restrict__ W,
    float       * const __restrict__ res_g,
    float       * const __restrict__ int_res,
    float       * const __restrict__ S_orig
);

extern __global__ void matmul_2(
    float const * const __restrict__ int_res,
    float       * const __restrict__ res_g,
    float       * const __restrict__ S,
    float       * const __restrict__ S_orig
);

#define CUDA_CHECK(e) do { \
    cudaError_t _err = (e); \
    if (_err != cudaSuccess) { \
        fprintf(stderr, "CUDA error '%s' at %s:%d\n", \
                cudaGetErrorString(_err), __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

int main() {
    int sz_Z   = NL1 * NR1;
    int sz_W   = NL1 * NL2 * NR1;
    int sz_res = NL1 * NL2;
    int sz_int = NL1 * NL2 * NUM_WG_R_1;

    float *h_Z   = new float[sz_Z];
    float *h_W   = new float[sz_W];
    float *h_res = new float[sz_res]();
    float *h_ref = new float[sz_res]();

    srand(42);
    for (int i = 0; i < sz_Z; i++) h_Z[i] = (rand() % 10) * 0.1f;
    for (int i = 0; i < sz_W; i++) h_W[i] = (rand() % 10) * 0.1f;

    // ── CPU computation ───────────────────────────────────────────────────────
    printf("=== CPU Reference Computation ===\n");
    auto cpu_start = std::chrono::high_resolution_clock::now();
    for (int l1 = 0; l1 < NL1; l1++)
        for (int l2 = 0; l2 < NL2; l2++) {
            float s = 0.0f;
            for (int r1 = 0; r1 < NR1; r1++)
                s += h_Z[l1 * NR1 + r1] * h_W[(l1 * NL2 + l2) * NR1 + r1];
            h_ref[l1 * NL2 + l2] = s;
        }
    auto cpu_end = std::chrono::high_resolution_clock::now();
    double cpu_ms = std::chrono::duration<double, std::milli>(cpu_end - cpu_start).count();
    printf("Config: L1=%d  L2=%d  R1=%d\n", NL1, NL2, NR1);
    printf("CPU time: %.3f ms\n\n", cpu_ms);

    // ── GPU computation ───────────────────────────────────────────────────────
    printf("=== GPU Kernel Execution (MDH CUDA Generator) ===\n");
    printf("K1 Grid: (%d,%d,%d)  Block: (%d,%d,%d)\n",
           GRID1.x, GRID1.y, GRID1.z, BLOCK1.x, BLOCK1.y, BLOCK1.z);
    printf("K2 Grid: (%d,%d,%d)  Block: (%d,%d,%d)\n",
           GRID2.x, GRID2.y, GRID2.z, BLOCK2.x, BLOCK2.y, BLOCK2.z);

    float *d_Z, *d_W, *d_res, *d_int, *d_S, *d_orig;
    CUDA_CHECK(cudaMalloc(&d_Z,    sz_Z   * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_W,    sz_W   * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_res,  sz_res * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_int,  sz_int * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_S,    sz_res * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_orig, sz_res * sizeof(float)));
    CUDA_CHECK(cudaMemset(d_res, 0, sz_res * sizeof(float)));
    CUDA_CHECK(cudaMemset(d_S,   0, sz_res * sizeof(float)));
    CUDA_CHECK(cudaMemcpy(d_Z, h_Z, sz_Z * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_W, h_W, sz_W * sizeof(float), cudaMemcpyHostToDevice));

    cudaEvent_t t_start, t_stop;
    CUDA_CHECK(cudaEventCreate(&t_start));
    CUDA_CHECK(cudaEventCreate(&t_stop));

    CUDA_CHECK(cudaEventRecord(t_start));
    matmul_1<<<GRID1, BLOCK1>>>(d_Z, d_W, d_res, d_int, d_orig);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());
    matmul_2<<<GRID2, BLOCK2>>>(d_int, d_res, d_S, d_orig);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());
    CUDA_CHECK(cudaEventRecord(t_stop));
    CUDA_CHECK(cudaEventSynchronize(t_stop));

    float gpu_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&gpu_ms, t_start, t_stop));
    printf("GPU time: %f ms\n\n", gpu_ms);

    CUDA_CHECK(cudaMemcpy(h_res, d_S, sz_res * sizeof(float), cudaMemcpyDeviceToHost));

    // ── side-by-side comparison ───────────────────────────────────────────────
    printf("=== CPU vs GPU Comparison ===\n");
    printf(" %-6s | %-14s | %-14s | %-12s\n", "Index", "CPU Result", "GPU Result", "Diff");
    printf("--------|----------------|----------------|-------------\n");
    for (int i = 0; i < 10; i++)
        printf(" %-6d | %14.5f | %14.5f | %12.2e\n",
               i, h_ref[i], h_res[i], fabsf(h_res[i] - h_ref[i]));
    printf("  ...   |      ...       |      ...       |     ...\n\n");

    float max_err = 0.0f;
    int   mismatch = 0;
    for (int idx = 0; idx < sz_res; idx++) {
        float err = fabsf(h_res[idx] - h_ref[idx]);
        if (err > max_err) max_err = err;
        if (err > 1e-3f) mismatch++;
    }

    printf("Total elements checked : %d\n", sz_res);
    printf("Max error              : %.2e\n", max_err);
    printf("Mismatches (>1e-3)     : %d\n\n", mismatch);

    if (mismatch == 0) {
        printf("Matrix Multiplication (MDH) is SUCCESSFUL!\n");
        printf("CPU vs GPU             : PASS\n");
        printf("CPU time               : %.3f ms\n", cpu_ms);
        printf("GPU time (MDH)         : %f ms\n", gpu_ms);
    } else {
        printf("Matrix Multiplication (MDH) FAILED - %d mismatches\n", mismatch);
        printf("CPU vs GPU             : FAIL\n");
    }

    cudaEventDestroy(t_start);
    cudaEventDestroy(t_stop);
    cudaFree(d_Z); cudaFree(d_W);
    cudaFree(d_res); cudaFree(d_int); cudaFree(d_S); cudaFree(d_orig);
    delete[] h_Z; delete[] h_W; delete[] h_res; delete[] h_ref;
    return mismatch != 0;
}
