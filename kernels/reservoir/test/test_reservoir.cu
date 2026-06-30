// Single SpMV step correctness + timing test for the MDH-generated reservoir kernel.
//
// Build:
//   nvcc test_reservoir.cu reservoir_1.cu -o test_reservoir         \
//     -DTYPE_T=double -DTYPE_TS=double                              \
//     -DCACHE_L_CB=0 -DCACHE_P_CB=0                                \
//     -DG_CB_RES_DEST_LEVEL=2                                       \
//     -DG_CB_SIZE_L_1=3998 -DG_CB_SIZE_L_2=3998                    \
//     -DL_CB_RES_DEST_LEVEL=1 -DL_CB_SIZE_L_1=32 -DL_CB_SIZE_L_2=32 \
//     -DP_CB_RES_DEST_LEVEL=0 -DP_CB_SIZE_L_1=1  -DP_CB_SIZE_L_2=1  \
//     -DNUM_WG_L_1=125 -DNUM_WG_L_2=125                            \
//     -DNUM_WI_L_1=32 -DNUM_WI_L_2=32                              \
//     -DOCL_DIM_L_1=1 -DOCL_DIM_L_2=0

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <cuda_runtime.h>

// TYPE_T is set by nvcc -D flag (float or double)
typedef TYPE_T real_t;

// Grid - large enough to saturate GPU memory bandwidth
static const int    FULLN = 4000;
static const int    NI    = FULLN - 2;         // 3998 interior points per side
static const double KX    = 0.001, KY = 0.001;
static const double M1    = 0.03,  M2 = 0.3;
static const double H     = 1.0 / (FULLN - 1.0);
static const double H2    = H * H;
static const double DT    = H2 / 0.00001;      // dt = h² / 0.00001  (from Source.cpp)
static const double H2_DT = H2 / DT;           // = 0.00001

// Kernel launch dims (OCL_DIM_L_2=0 → x-axis, OCL_DIM_L_1=1 → y-axis)
static const dim3 BLOCK1 { NUM_WI_L_2, NUM_WI_L_1, 1 };
static const dim3 GRID1  { NUM_WG_L_2, NUM_WG_L_1, 1 };

extern __global__ void reservoir_1(
    real_t const * const __restrict__ V,
    real_t const * const __restrict__ COEFF_UP,
    real_t const * const __restrict__ COEFF_LEFT,
    real_t const * const __restrict__ COEFF_CENTER,
    real_t const * const __restrict__ COEFF_RIGHT,
    real_t const * const __restrict__ COEFF_DOWN,
    real_t       * const __restrict__ res_g,
    real_t       * const __restrict__ int_res
);

#define CUDA_CHECK(e) do {                                              \
    cudaError_t _err = (e);                                             \
    if (_err != cudaSuccess) {                                          \
        fprintf(stderr, "CUDA error '%s' at %s:%d\n",                  \
                cudaGetErrorString(_err), __FILE__, __LINE__);          \
        exit(1);                                                        \
    }                                                                   \
} while(0)

int main()
{
    const int sz_full = FULLN * FULLN;
    const int sz      = NI * NI;

    printf("=== MDH Reservoir SpMV Correctness Test ===\n");
    printf("Full grid  : %d x %d\n", FULLN, FULLN);
    printf("Interior   : %d x %d = %d points\n", NI, NI, sz);
    printf("Grid: (%d,%d)  Block: (%d,%d)\n\n",
           GRID1.x, GRID1.y, BLOCK1.x, BLOCK1.y);

    // ── build Mx, My on full grid (uniform S=0.1, matching Source.cpp init) ─
    real_t *Mx = new real_t[sz_full];
    real_t *My = new real_t[sz_full];
    const double S = 0.1;
    const double mob = KX * (S*S/M1) + KX * ((1.0-S)*(1.0-S)/M2);
    for (int i = 0; i < sz_full; i++) {
        Mx[i] = -mob;
        My[i] = -mob;
    }

    // ── compute coefficient arrays for NI×NI interior ───────────────────────
    // Interior (r,c) [0-indexed] maps to full-grid (r+1, c+1)
    real_t *h_CU = new real_t[sz];   // COEFF_UP
    real_t *h_CL = new real_t[sz];   // COEFF_LEFT
    real_t *h_CC = new real_t[sz];   // COEFF_CENTER
    real_t *h_CR = new real_t[sz];   // COEFF_RIGHT
    real_t *h_CD = new real_t[sz];   // COEFF_DOWN

    for (int r = 0; r < NI; r++) {
        for (int c = 0; c < NI; c++) {
            int fi = r + 1, fj = c + 1;
            double cu = 0.5*(Mx[ fi   *FULLN+fj] + Mx[(fi-1)*FULLN+fj]);
            double cl = 0.5*(My[ fi   *FULLN+fj] + My[ fi   *FULLN+fj-1]);
            double cr = 0.5*(My[ fi   *FULLN+fj+1] + My[fi  *FULLN+fj]);
            double cd = 0.5*(Mx[(fi+1)*FULLN+fj] + Mx[ fi   *FULLN+fj]);
            double cc = -(cu + cl + cr + cd + H2_DT);
            h_CU[r*NI+c] = cu;
            h_CL[r*NI+c] = cl;
            h_CC[r*NI+c] = cc;
            h_CR[r*NI+c] = cr;
            h_CD[r*NI+c] = cd;
        }
    }

    // ── test vector V: linear ramp so values vary across grid ───────────────
    real_t *h_V = new real_t[sz];
    for (int r = 0; r < NI; r++)
        for (int c = 0; c < NI; c++)
            h_V[r*NI+c] = (r + 1) * 0.1 + (c + 1) * 0.01;

    // ── CPU reference: direct stencil apply (timed) ─────────────────────────
    // oob::ZERO → V = 0 outside interior boundary (Dirichlet BC)
    real_t *h_ref = new real_t[sz]();
    auto cpu_start = std::chrono::high_resolution_clock::now();
    for (int r = 0; r < NI; r++) {
        for (int c = 0; c < NI; c++) {
            double vup    = (r > 0)    ? h_V[(r-1)*NI+c]   : 0.0;
            double vleft  = (c > 0)    ? h_V[r*NI+(c-1)]   : 0.0;
            double vctr   =              h_V[r*NI+c];
            double vright = (c < NI-1) ? h_V[r*NI+(c+1)]   : 0.0;
            double vdown  = (r < NI-1) ? h_V[(r+1)*NI+c]   : 0.0;
            h_ref[r*NI+c] = h_CU[r*NI+c] * vup
                           + h_CL[r*NI+c] * vleft
                           + h_CC[r*NI+c] * vctr
                           + h_CR[r*NI+c] * vright
                           + h_CD[r*NI+c] * vdown;
        }
    }
    auto cpu_stop = std::chrono::high_resolution_clock::now();
    double cpu_ms = std::chrono::duration<double, std::milli>(cpu_stop - cpu_start).count();

    // ── GPU allocation ───────────────────────────────────────────────────────
    real_t *d_V, *d_CU, *d_CL, *d_CC, *d_CR, *d_CD, *d_res_g, *d_int_res;
    CUDA_CHECK(cudaMalloc(&d_V,       sz * sizeof(real_t)));
    CUDA_CHECK(cudaMalloc(&d_CU,      sz * sizeof(real_t)));
    CUDA_CHECK(cudaMalloc(&d_CL,      sz * sizeof(real_t)));
    CUDA_CHECK(cudaMalloc(&d_CC,      sz * sizeof(real_t)));
    CUDA_CHECK(cudaMalloc(&d_CR,      sz * sizeof(real_t)));
    CUDA_CHECK(cudaMalloc(&d_CD,      sz * sizeof(real_t)));
    CUDA_CHECK(cudaMalloc(&d_res_g,   sz * sizeof(real_t)));
    CUDA_CHECK(cudaMalloc(&d_int_res, sz * sizeof(real_t)));

    CUDA_CHECK(cudaMemcpy(d_V,  h_V,  sz*sizeof(real_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_CU, h_CU, sz*sizeof(real_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_CL, h_CL, sz*sizeof(real_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_CC, h_CC, sz*sizeof(real_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_CR, h_CR, sz*sizeof(real_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_CD, h_CD, sz*sizeof(real_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemset(d_res_g,   0, sz*sizeof(real_t)));
    CUDA_CHECK(cudaMemset(d_int_res, 0, sz*sizeof(real_t)));

    // ── timing ───────────────────────────────────────────────────────────────
    cudaEvent_t t_start, t_stop;
    CUDA_CHECK(cudaEventCreate(&t_start));
    CUDA_CHECK(cudaEventCreate(&t_stop));

    // ── launch kernel ────────────────────────────────────────────────────────
    printf("Launching reservoir_1 kernel...\n");
    CUDA_CHECK(cudaEventRecord(t_start));
    reservoir_1<<<GRID1, BLOCK1>>>(d_V, d_CU, d_CL, d_CC, d_CR, d_CD,
                                   d_res_g, d_int_res);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());
    CUDA_CHECK(cudaEventRecord(t_stop));
    CUDA_CHECK(cudaEventSynchronize(t_stop));

    // ── read back ────────────────────────────────────────────────────────────
    real_t *h_result = new real_t[sz]();
    CUDA_CHECK(cudaMemcpy(h_result, d_int_res, sz*sizeof(real_t), cudaMemcpyDeviceToHost));

    // ── compare ──────────────────────────────────────────────────────────────
    // float32 has ~1e-7 machine epsilon; allow 1e-5 for accumulated rounding
    // double has ~1e-16; keep 1e-10 threshold
    const double THRESH = (sizeof(real_t) == 4) ? 1e-5 : 1e-10;
    double max_err   = 0.0;
    int    mismatches = 0;
    for (int idx = 0; idx < sz; idx++) {
        double err = fabs((double)h_result[idx] - (double)h_ref[idx]);
        if (err > max_err) max_err = err;
        if (err > THRESH) {
            if (mismatches < 5)
                printf("  MISMATCH idx=%d  gpu=%.12f  ref=%.12f  diff=%.3e\n",
                       idx, (double)h_result[idx], (double)h_ref[idx], err);
            mismatches++;
        }
    }

    float gpu_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&gpu_ms, t_start, t_stop));
    double speedup = cpu_ms / (double)gpu_ms;

    printf("\n--- Results ---\n");
    printf("Elements checked     : %d\n", sz);
    printf("Max error vs serial  : %.3e\n", max_err);
    printf("Mismatches (>1e-10)  : %d\n", mismatches);
    printf("CPU time (serial)    : %.6f ms\n", cpu_ms);
    printf("GPU time (MDH)       : %.6f ms\n", (double)gpu_ms);
    printf("Speedup              : %.2fx\n\n", speedup);

    if (mismatches == 0)
        printf("Reservoir (MDH) SpMV is SUCCESSFUL!\n");
    else
        printf("Reservoir (MDH) FAILED - %d mismatches vs serial reference\n",
               mismatches);

    // ── cleanup ──────────────────────────────────────────────────────────────
    cudaEventDestroy(t_start);
    cudaEventDestroy(t_stop);
    cudaFree(d_V);   cudaFree(d_CU);  cudaFree(d_CL);
    cudaFree(d_CC);  cudaFree(d_CR);  cudaFree(d_CD);
    cudaFree(d_res_g);  cudaFree(d_int_res);
    delete[] Mx;  delete[] My;
    delete[] h_CU; delete[] h_CL; delete[] h_CC;
    delete[] h_CR; delete[] h_CD;
    delete[] h_V;  delete[] h_ref;  delete[] h_result;

    return mismatches != 0;
}
