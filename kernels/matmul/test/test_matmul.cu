// Runtime test for the generated matmul CUDA kernel.
//
// Build:
//   nvcc test_matmul.cu matmul_1.cu -o test_matmul \
//     -DTYPE_T=float -DTYPE_TS=float -DCACHE_L_CB=0 -DCACHE_P_CB=0     \
//     -DG_CB_RES_DEST_LEVEL=2  -DL_CB_RES_DEST_LEVEL=1 -DP_CB_RES_DEST_LEVEL=0 \
//     -DG_CB_SIZE_L_1=10 -DG_CB_SIZE_L_2=500 -DG_CB_SIZE_R_1=64        \
//     -DL_CB_SIZE_L_1=8  -DL_CB_SIZE_L_2=16  -DL_CB_SIZE_R_1=64        \
//     -DP_CB_SIZE_L_1=1  -DP_CB_SIZE_L_2=1   -DP_CB_SIZE_R_1=1         \
//     -DNUM_WG_L_1=2 -DNUM_WG_L_2=32 -DNUM_WG_R_1=1                    \
//     -DNUM_WI_L_1=4 -DNUM_WI_L_2=32 -DNUM_WI_R_1=8                    \
//     -DOCL_DIM_L_1=2 -DOCL_DIM_L_2=1 -DOCL_DIM_R_1=0

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cuda_runtime.h>

// Kernel 1: OCL_DIM_R_1=0→x, OCL_DIM_L_2=1→y, OCL_DIM_L_1=2→z
static const dim3 BLOCK1 { NUM_WI_R_1, NUM_WI_L_2, NUM_WI_L_1 };
static const dim3 GRID1  { NUM_WG_R_1, NUM_WG_L_2, NUM_WG_L_1 };

// Kernel 2: same L dims, but R1 shared-memory is sized by
//   K2_L_NUM_FU_R_1 = min(NUM_WI_R_1, NUM_WG_R_1).
// Launching more x-threads than K2_L_NUM_FU_R_1 overflows the __shared__ array.
// With NUM_WG_R_1=1: blockDim.x must be 1, not 8.
static const int  K2_BLOCK_X = (NUM_WG_R_1 < NUM_WI_R_1) ? NUM_WG_R_1 : NUM_WI_R_1;
static const dim3 BLOCK2 { (unsigned)K2_BLOCK_X, NUM_WI_L_2, NUM_WI_L_1 };
static const dim3 GRID2  { 1, NUM_WG_L_2, NUM_WG_L_1 };

static const int NL1 = G_CB_SIZE_L_1;   // 10
static const int NL2 = G_CB_SIZE_L_2;   // 500
static const int NR1 = G_CB_SIZE_R_1;   // 64

// Forward-declare the generated kernels
extern __global__ void matmul_1(
    float const * const __restrict__ Z,
    float const * const __restrict__ W,
    float       * const __restrict__ res_g,
    float       * const __restrict__ int_res,
    float       * const __restrict__ S_orig
);

// kernel_2: reduces NUM_WG_R_1 partial sums in int_res → final result in S
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
    printf("Config: L1=%d  L2=%d  R1=%d\n", NL1, NL2, NR1);
    printf("K1 Grid: (%d,%d,%d)  Block: (%d,%d,%d)\n",
           GRID1.x, GRID1.y, GRID1.z, BLOCK1.x, BLOCK1.y, BLOCK1.z);
    printf("K2 Grid: (%d,%d,%d)  Block: (%d,%d,%d)\n",
           GRID2.x, GRID2.y, GRID2.z, BLOCK2.x, BLOCK2.y, BLOCK2.z);

    // ── allocate host arrays ───────────────────────────────────────────────────
    int sz_Z   = NL1 * NR1;
    int sz_W   = NL1 * NL2 * NR1;
    int sz_res = NL1 * NL2;
    int sz_int = NL1 * NL2 * NUM_WG_R_1;  // partial sums per (L1,L2)

    float *h_Z   = new float[sz_Z];
    float *h_W   = new float[sz_W];
    float *h_res = new float[sz_res]();   // GPU result read-back
    float *h_ref = new float[sz_res]();   // CPU reference

    srand(42);
    for (int i = 0; i < sz_Z; i++) h_Z[i] = (rand() % 10) * 0.1f;
    for (int i = 0; i < sz_W; i++) h_W[i] = (rand() % 10) * 0.1f;

    // ── CPU reference ─────────────────────────────────────────────────────────
    // Buffer layouts (from K1_G_BUFFER_* macros in generated kernel):
    //   Z  row-major [L1][R1]      : Z[l1 * NR1 + r1]
    //   W  row-major [L1][L2][R1]  : W[(l1*NL2 + l2) * NR1 + r1]
    //
    // With L_CB_RES_DEST_LEVEL=LOCAL and P_CB_RES_DEST_LEVEL=PRIVATE,
    // the #else branch in the generated kernel applies:
    //   K1_RES_G_BUFFER_NAME() = int_res
    //   With K1_G_NUM_FU_R_1==1: int_res[(i) * K1_G_CB_SIZE_L_2 + (j)]
    //                           = int_res[l1 * NL2 + l2]  (row-major)
    for (int l1 = 0; l1 < NL1; l1++) {
        for (int l2 = 0; l2 < NL2; l2++) {
            float s = 0.0f;
            for (int r1 = 0; r1 < NR1; r1++)
                s += h_Z[l1 * NR1 + r1] * h_W[(l1 * NL2 + l2) * NR1 + r1];
            h_ref[l1 * NL2 + l2] = s;   // row-major [L1][L2]
        }
    }

    // ── GPU ───────────────────────────────────────────────────────────────────
    float *d_Z, *d_W, *d_res, *d_int, *d_S, *d_orig;
    CUDA_CHECK(cudaMalloc(&d_Z,    sz_Z   * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_W,    sz_W   * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_res,  sz_res * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_int,  sz_int * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_S,    sz_res * sizeof(float)));   // kernel_2 final output
    CUDA_CHECK(cudaMalloc(&d_orig, sz_res * sizeof(float)));
    CUDA_CHECK(cudaMemset(d_res, 0, sz_res * sizeof(float)));
    CUDA_CHECK(cudaMemset(d_S,   0, sz_res * sizeof(float)));

    CUDA_CHECK(cudaMemcpy(d_Z, h_Z, sz_Z * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_W, h_W, sz_W * sizeof(float), cudaMemcpyHostToDevice));

    // ── timing setup ──────────────────────────────────────────────────────────
    cudaEvent_t t_start, t_stop;
    CUDA_CHECK(cudaEventCreate(&t_start));
    CUDA_CHECK(cudaEventCreate(&t_stop));

    // ── kernel 1: compute partial sums → int_res[l1*NL2+l2] ─────────────────
    printf("\n--- kernel 1 ---\n");
    CUDA_CHECK(cudaEventRecord(t_start));
    matmul_1<<<GRID1, BLOCK1>>>(d_Z, d_W, d_res, d_int, d_orig);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());

    // verify kernel_1 intermediate result
    float *h_int = new float[sz_int]();
    CUDA_CHECK(cudaMemcpy(h_int, d_int, sz_res * sizeof(float), cudaMemcpyDeviceToHost));
    {
        float max_err = 0.0f; int mismatch = 0;
        for (int idx = 0; idx < sz_res; idx++) {
            float err = fabsf(h_int[idx] - h_ref[idx]);
            if (err > max_err) max_err = err;
            if (err > 1e-3f) mismatch++;
        }
        printf("int_res: Elements=%d  Max error=%.2e  Mismatches=%d  -> %s\n",
               sz_res, max_err, mismatch, mismatch == 0 ? "PASS" : "FAIL");
    }

    // ── kernel 2: reduce partial sums → S[l1*NL2+l2] ─────────────────────────
    // Same GRID/BLOCK as kernel_1 (extra R-axis threads are guarded out since
    // NUM_WG_R_1=1).  kernel_2 reads int_res and writes the final result to S.
    printf("--- kernel 2 ---\n");
    matmul_2<<<GRID2, BLOCK2>>>(d_int, d_res, d_S, d_orig);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());
    CUDA_CHECK(cudaEventRecord(t_stop));
    CUDA_CHECK(cudaEventSynchronize(t_stop));

    CUDA_CHECK(cudaMemcpy(h_res, d_S, sz_res * sizeof(float), cudaMemcpyDeviceToHost));

    // ── final compare ─────────────────────────────────────────────────────────
    printf("--- final result (S after kernel 2) ---\n");
    float max_err  = 0.0f;
    int   mismatch = 0;
    for (int idx = 0; idx < sz_res; idx++) {
        float err = fabsf(h_res[idx] - h_ref[idx]);
        if (err > max_err) max_err = err;
        if (err > 1e-3f) {
            if (mismatch < 5)
                printf("  MISMATCH idx=%d  gpu=%.5f  ref=%.5f  diff=%.2e\n",
                       idx, h_res[idx], h_ref[idx], err);
            mismatch++;
        }
    }

    printf("Elements checked: %d  Max error: %.2e  Mismatches: %d\n",
           sz_res, max_err, mismatch);

    float gpu_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&gpu_ms, t_start, t_stop));

    if (mismatch == 0) {
        printf("Matrix Multiplication (MDH) is SUCCESSFUL!\n");
        printf("GPU time measurement (MDH): %f ms\n", gpu_ms);
    } else {
        printf("Matrix Multiplication (MDH) FAILED - %d mismatches\n", mismatch);
    }

    cudaEventDestroy(t_start);
    cudaEventDestroy(t_stop);
    cudaFree(d_Z); cudaFree(d_W);
    cudaFree(d_res); cudaFree(d_int); cudaFree(d_S); cudaFree(d_orig);
    delete[] h_int;
    delete[] h_Z; delete[] h_W; delete[] h_res; delete[] h_ref;
    return mismatch != 0;
}
