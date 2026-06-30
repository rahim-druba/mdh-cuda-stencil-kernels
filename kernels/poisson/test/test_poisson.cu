// Runtime test for the MDH-generated Poisson (Jacobi 2D 5-point) CUDA kernel.
//
// Build:
//   nvcc test_poisson.cu poisson_1.cu -o test_poisson              \
//     -DTYPE_T=double -DTYPE_TS=double                              \
//     -DCACHE_L_CB=0 -DCACHE_P_CB=0                                \
//     -DG_CB_RES_DEST_LEVEL=2                                       \
//     -DG_CB_SIZE_L_1=62 -DG_CB_SIZE_L_2=62                        \
//     -DL_CB_RES_DEST_LEVEL=1                                       \
//     -DL_CB_SIZE_L_1=16 -DL_CB_SIZE_L_2=16                        \
//     -DNUM_WG_L_1=4   -DNUM_WG_L_2=4                              \
//     -DNUM_WI_L_1=16  -DNUM_WI_L_2=16                             \
//     -DOCL_DIM_L_1=1  -DOCL_DIM_L_2=0                             \
//     -DP_CB_RES_DEST_LEVEL=0                                       \
//     -DP_CB_SIZE_L_1=1 -DP_CB_SIZE_L_2=1

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cuda_runtime.h>

// ── grid constants ─────────────────────────────────────────────────────────────
static const int    NX = 64;                         // full grid side
static const int    NI = NX - 2;                     // interior side = 62
static const double H  = 1.0 / (NX - 1.0);          // grid spacing
static const double H2 = H * H;                      // h squared

// ── CUDA kernel launch dims ────────────────────────────────────────────────────
// OCL_DIM_L_2=0 → x-axis, OCL_DIM_L_1=1 → y-axis
static const dim3 BLOCK1 { NUM_WI_L_2, NUM_WI_L_1, 1 };  // (16, 16, 1)
static const dim3 GRID1  { NUM_WG_L_2, NUM_WG_L_1, 1 };  // ( 4,  4, 1)

// ── forward declaration of generated kernel ────────────────────────────────────
extern __global__ void poisson_1(
    double const * const __restrict__ U,       // interior u_old, NI×NI
    double const * const __restrict__ SOURCE,  // interior source, NI×NI
    const double                      h2,      // scalar h²
    double       * const __restrict__ res_g,   // not the answer (unused here)
    double       * const __restrict__ int_res  // answer: NI×NI Jacobi step
);

// ── error check macro ──────────────────────────────────────────────────────────
#define CUDA_CHECK(e) do {                                                   \
    cudaError_t _err = (e);                                                  \
    if (_err != cudaSuccess) {                                               \
        fprintf(stderr, "CUDA error '%s' at %s:%d\n",                       \
                cudaGetErrorString(_err), __FILE__, __LINE__);               \
        exit(1);                                                             \
    }                                                                        \
} while(0)

int main()
{
    const int sz = NI * NI;   // 62 * 62 = 3844

    printf("=== MDH Poisson Jacobi 2D Test ===\n");
    printf("Full grid  : %d x %d\n", NX, NX);
    printf("Interior   : %d x %d = %d points\n", NI, NI, sz);
    printf("K1  Grid   : (%d, %d)   Block: (%d, %d)\n",
           GRID1.x, GRID1.y, BLOCK1.x, BLOCK1.y);
    printf("h = %.6f   h2 = %.10f\n\n", H, H2);

    // ── host arrays ────────────────────────────────────────────────────────────
    double *h_U_old  = new double[sz]();   // u_old interior, zero-init (IC=0)
    double *h_SOURCE = new double[sz];     // source term for interior
    double *h_result = new double[sz]();   // GPU result readback
    double *h_ref    = new double[sz]();   // serial reference (one step)

    // ── compute source term for interior ──────────────────────────────────────
    // Interior row k (0..61) maps to full-grid row (k+1), x=(l+1)*h, y=(k+1)*h
    for (int k = 0; k < NI; k++) {
        for (int l = 0; l < NI; l++) {
            double x = (l + 1) * H;
            double y = (k + 1) * H;
            h_SOURCE[k * NI + l] = 2.0 * (x*(1.0-x) + y*(1.0-y));
        }
    }

    // ── serial reference: one Jacobi step ─────────────────────────────────────
    // oob::ZERO → neighbors outside interior domain = 0 (matches Dirichlet BC=0)
    printf("Computing serial reference (1 Jacobi step from u=0)...\n");
    for (int k = 0; k < NI; k++) {
        for (int l = 0; l < NI; l++) {
            double top    = (k > 0)    ? h_U_old[(k-1)*NI + l]   : 0.0;
            double bottom = (k < NI-1) ? h_U_old[(k+1)*NI + l]   : 0.0;
            double left   = (l > 0)    ? h_U_old[k*NI + (l-1)]   : 0.0;
            double right  = (l < NI-1) ? h_U_old[k*NI + (l+1)]   : 0.0;
            h_ref[k*NI+l] = 0.25 * (top + bottom + left + right
                                     + h_SOURCE[k*NI+l] * H2);
        }
    }

    // ── GPU allocation ─────────────────────────────────────────────────────────
    double *d_U, *d_SOURCE, *d_res_g, *d_int_res;
    CUDA_CHECK(cudaMalloc(&d_U,       sz * sizeof(double)));
    CUDA_CHECK(cudaMalloc(&d_SOURCE,  sz * sizeof(double)));
    CUDA_CHECK(cudaMalloc(&d_res_g,   sz * sizeof(double)));  // unused but required
    CUDA_CHECK(cudaMalloc(&d_int_res, sz * sizeof(double)));

    CUDA_CHECK(cudaMemcpy(d_U,      h_U_old,  sz * sizeof(double), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_SOURCE, h_SOURCE, sz * sizeof(double), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemset(d_res_g,   0, sz * sizeof(double)));
    CUDA_CHECK(cudaMemset(d_int_res, 0, sz * sizeof(double)));

    // ── timing ─────────────────────────────────────────────────────────────────
    cudaEvent_t t_start, t_stop;
    CUDA_CHECK(cudaEventCreate(&t_start));
    CUDA_CHECK(cudaEventCreate(&t_stop));

    // ── launch kernel 1 ────────────────────────────────────────────────────────
    printf("Launching poisson_1 kernel...\n");
    CUDA_CHECK(cudaEventRecord(t_start));
    poisson_1<<<GRID1, BLOCK1>>>(d_U, d_SOURCE, H2, d_res_g, d_int_res);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());
    CUDA_CHECK(cudaEventRecord(t_stop));
    CUDA_CHECK(cudaEventSynchronize(t_stop));

    // ── read back result from int_res ──────────────────────────────────────────
    CUDA_CHECK(cudaMemcpy(h_result, d_int_res, sz * sizeof(double), cudaMemcpyDeviceToHost));

    // ── compare GPU vs serial reference ───────────────────────────────────────
    double max_error = 0.0;
    int    mismatches = 0;
    for (int idx = 0; idx < sz; idx++) {
        double err = fabs(h_result[idx] - h_ref[idx]);
        if (err > max_error) max_error = err;
        if (err > 1e-10) {
            if (mismatches < 5)
                printf("  MISMATCH idx=%d  gpu=%.12f  ref=%.12f  diff=%.3e\n",
                       idx, h_result[idx], h_ref[idx], err);
            mismatches++;
        }
    }

    float gpu_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&gpu_ms, t_start, t_stop));

    // ── also compute max error vs analytical exact solution ────────────────────
    double max_exact_err = 0.0;
    for (int k = 0; k < NI; k++) {
        for (int l = 0; l < NI; l++) {
            double x     = (l + 1) * H;
            double y     = (k + 1) * H;
            double exact = x * (1.0-x) * y * (1.0-y);
            double err   = fabs(h_result[k*NI+l] - exact);
            if (err > max_exact_err) max_exact_err = err;
        }
    }

    // ── print results ──────────────────────────────────────────────────────────
    printf("\n--- Results ---\n");
    printf("Elements checked     : %d\n", sz);
    printf("Max error vs serial  : %.3e\n", max_error);
    printf("Mismatches (>1e-10)  : %d\n", mismatches);
    printf("Max error vs exact   : %.3e  (expected large — only 1 iteration)\n",
           max_exact_err);
    printf("GPU time (kernel 1)  : %f ms\n\n", gpu_ms);

    if (mismatches == 0)
        printf("Poisson (MDH) Jacobi step is SUCCESSFUL!\n");
    else
        printf("Poisson (MDH) FAILED — %d mismatches vs serial reference\n",
               mismatches);

    // ── cleanup ────────────────────────────────────────────────────────────────
    cudaEventDestroy(t_start);
    cudaEventDestroy(t_stop);
    cudaFree(d_U);
    cudaFree(d_SOURCE);
    cudaFree(d_res_g);
    cudaFree(d_int_res);
    delete[] h_U_old;
    delete[] h_SOURCE;
    delete[] h_result;
    delete[] h_ref;

    return mismatches != 0;
}
