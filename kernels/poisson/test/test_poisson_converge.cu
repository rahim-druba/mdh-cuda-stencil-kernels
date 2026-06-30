// Full convergence test: MDH GPU Jacobi vs Serial Jacobi
//
// Runs both solvers to convergence (max_res < 1e-4) on a 64x64 grid.
// Reports iteration count, GPU kernel time, serial time, and solution accuracy.
//
// Build:
//   nvcc test_poisson_converge.cu poisson_1.cu -o test_poisson_converge   \
//     -DTYPE_T=double -DTYPE_TS=double                                     \
//     -DCACHE_L_CB=0 -DCACHE_P_CB=0                                       \
//     -DG_CB_RES_DEST_LEVEL=2                                              \
//     -DG_CB_SIZE_L_1=62 -DG_CB_SIZE_L_2=62                               \
//     -DL_CB_RES_DEST_LEVEL=1 -DL_CB_SIZE_L_1=16 -DL_CB_SIZE_L_2=16      \
//     -DNUM_WG_L_1=4  -DNUM_WG_L_2=4                                      \
//     -DNUM_WI_L_1=16 -DNUM_WI_L_2=16                                     \
//     -DOCL_DIM_L_1=1 -DOCL_DIM_L_2=0                                     \
//     -DP_CB_RES_DEST_LEVEL=0 -DP_CB_SIZE_L_1=1 -DP_CB_SIZE_L_2=1

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cuda_runtime.h>

// ── problem constants ──────────────────────────────────────────────────────────
static const int    NX          = 64;
static const int    NI          = NX - 2;          // interior side = 62
static const double H           = 1.0 / (NX - 1.0);
static const double H2          = H * H;
static const double EPS         = 1e-4;
static const int    N_ITER_MAX  = 3000000;

// ── kernel launch config ───────────────────────────────────────────────────────
// OCL_DIM_L_2=0 → x-axis, OCL_DIM_L_1=1 → y-axis
static const dim3 BLOCK1 { NUM_WI_L_2, NUM_WI_L_1, 1 };   // (16,16,1)
static const dim3 GRID1  { NUM_WG_L_2, NUM_WG_L_1, 1 };   // ( 4, 4,1)

// ── generated kernel ───────────────────────────────────────────────────────────
extern __global__ void poisson_1(
    double const * const __restrict__ U,
    double const * const __restrict__ SOURCE,
    const double                      h2,
    double       * const __restrict__ res_g,
    double       * const __restrict__ int_res
);

#define CUDA_CHECK(e) do {                                              \
    cudaError_t _err = (e);                                            \
    if (_err != cudaSuccess) {                                         \
        fprintf(stderr,"CUDA error '%s' at %s:%d\n",                  \
                cudaGetErrorString(_err),__FILE__,__LINE__);           \
        exit(1);                                                       \
    }                                                                  \
} while(0)

// ── source term w(x,y) = 2*(x(1-x)+y(1-y)) ───────────────────────────────────
static inline double source_fn(double x, double y) {
    return 2.0 * (x*(1.0-x) + y*(1.0-y));
}

// ── residual on interior array (62x62, oob=0) ─────────────────────────────────
static double compute_residual(const double *u, const double *src, int ni, double h2) {
    double max_res = 0.0;
    for (int k = 0; k < ni; k++) {
        for (int l = 0; l < ni; l++) {
            double above  = (k > 0)    ? u[(k-1)*ni+l]   : 0.0;
            double below  = (k < ni-1) ? u[(k+1)*ni+l]   : 0.0;
            double left   = (l > 0)    ? u[k*ni+(l-1)]   : 0.0;
            double right  = (l < ni-1) ? u[k*ni+(l+1)]   : 0.0;
            double lap    = above + below + left + right - 4.0*u[k*ni+l];
            double res    = fabs(lap/h2 + src[k*ni+l]);
            if (res > max_res) max_res = res;
        }
    }
    return max_res;
}

int main()
{
    const int sz = NI * NI;   // 3844

    printf("=== MDH Poisson Convergence Test ===\n");
    printf("Grid: %dx%d  Interior: %dx%d  EPS: %.0e  MaxIter: %d\n\n",
           NX, NX, NI, NI, EPS, N_ITER_MAX);

    // ── allocate host arrays ───────────────────────────────────────────────────
    double *h_source  = (double*)calloc(sz, sizeof(double));
    double *h_u_old   = (double*)calloc(sz, sizeof(double));
    double *h_u_new   = (double*)calloc(sz, sizeof(double));
    double *h_gpu_sol = (double*)calloc(sz, sizeof(double));

    // ── source term ───────────────────────────────────────────────────────────
    for (int k = 0; k < NI; k++)
        for (int l = 0; l < NI; l++) {
            double x = (l+1)*H, y = (k+1)*H;
            h_source[k*NI+l] = source_fn(x, y);
        }

    // ══════════════════════════════════════════════════════════════════════════
    // PART 1 - SERIAL SOLVER
    // ══════════════════════════════════════════════════════════════════════════
    printf("--- Serial Jacobi ---\n");

    // reset
    for (int i = 0; i < sz; i++) h_u_old[i] = h_u_new[i] = 0.0;

    int    serial_iter = 0;
    double serial_res  = 1.0;

    clock_t t_serial_start = clock();

    do {
        serial_iter++;

        // one Jacobi step
        for (int k = 0; k < NI; k++)
            for (int l = 0; l < NI; l++) {
                double above = (k > 0)    ? h_u_old[(k-1)*NI+l]   : 0.0;
                double below = (k < NI-1) ? h_u_old[(k+1)*NI+l]   : 0.0;
                double left  = (l > 0)    ? h_u_old[k*NI+(l-1)]   : 0.0;
                double right = (l < NI-1) ? h_u_old[k*NI+(l+1)]   : 0.0;
                h_u_new[k*NI+l] = 0.25*(above+below+left+right+h_source[k*NI+l]*H2);
            }

        // residual check every 100 iters
        if (serial_iter % 100 == 0)
            serial_res = compute_residual(h_u_new, h_source, NI, H2);

        // swap
        double *tmp = h_u_old; h_u_old = h_u_new; h_u_new = tmp;

        if (serial_iter % 50000 == 0)
            printf("  [serial] iter %7d  res = %e\n", serial_iter, serial_res);

    } while (serial_res > EPS && serial_iter < N_ITER_MAX);

    clock_t t_serial_end = clock();
    double time_serial = (double)(t_serial_end - t_serial_start) / CLOCKS_PER_SEC;

    // serial error vs analytical
    double serial_exact_err = 0.0;
    for (int k = 0; k < NI; k++)
        for (int l = 0; l < NI; l++) {
            double x = (l+1)*H, y = (k+1)*H;
            double exact = x*(1.0-x)*y*(1.0-y);
            serial_exact_err = fmax(serial_exact_err, fabs(h_u_old[k*NI+l]-exact));
        }

    printf("\nSerial converged!\n");
    printf("  Iterations        : %d\n", serial_iter);
    printf("  Final residual    : %e\n", serial_res);
    printf("  Time              : %.3f seconds\n", time_serial);
    printf("  Max error vs exact: %e\n\n", serial_exact_err);

    // ══════════════════════════════════════════════════════════════════════════
    // PART 2 - GPU SOLVER (MDH)
    // ══════════════════════════════════════════════════════════════════════════
    printf("--- GPU Jacobi (MDH-generated CUDA) ---\n");

    // GPU allocations
    double *d_U, *d_int_res, *d_res_g, *d_SOURCE;
    CUDA_CHECK(cudaMalloc(&d_U,       sz*sizeof(double)));
    CUDA_CHECK(cudaMalloc(&d_int_res, sz*sizeof(double)));
    CUDA_CHECK(cudaMalloc(&d_res_g,   sz*sizeof(double)));
    CUDA_CHECK(cudaMalloc(&d_SOURCE,  sz*sizeof(double)));

    CUDA_CHECK(cudaMemset(d_U,       0, sz*sizeof(double)));
    CUDA_CHECK(cudaMemset(d_int_res, 0, sz*sizeof(double)));
    CUDA_CHECK(cudaMemset(d_res_g,   0, sz*sizeof(double)));
    CUDA_CHECK(cudaMemcpy(d_SOURCE, h_source, sz*sizeof(double), cudaMemcpyHostToDevice));

    cudaEvent_t ev_start, ev_stop;
    CUDA_CHECK(cudaEventCreate(&ev_start));
    CUDA_CHECK(cudaEventCreate(&ev_stop));

    int    gpu_iter       = 0;
    double gpu_res        = 1.0;
    float  time_gpu_ms    = 0.0f;   // kernel-only time

    clock_t t_gpu_wall_start = clock();

    do {
        gpu_iter++;

        // time and launch kernel
        CUDA_CHECK(cudaEventRecord(ev_start));
        poisson_1<<<GRID1, BLOCK1>>>(d_U, d_SOURCE, H2, d_res_g, d_int_res);
        CUDA_CHECK(cudaEventRecord(ev_stop));
        CUDA_CHECK(cudaEventSynchronize(ev_stop));
        float ms = 0.0f;
        CUDA_CHECK(cudaEventElapsedTime(&ms, ev_start, ev_stop));
        time_gpu_ms += ms;

        // swap GPU pointers (ping-pong)
        double *tmp = d_U; d_U = d_int_res; d_int_res = tmp;

        // residual check every 100 iters (copy to CPU, same cadence as serial)
        if (gpu_iter % 100 == 0) {
            CUDA_CHECK(cudaMemcpy(h_gpu_sol, d_U, sz*sizeof(double), cudaMemcpyDeviceToHost));
            gpu_res = compute_residual(h_gpu_sol, h_source, NI, H2);
        }

        if (gpu_iter % 50000 == 0)
            printf("  [gpu]    iter %7d  res = %e\n", gpu_iter, gpu_res);

    } while (gpu_res > EPS && gpu_iter < N_ITER_MAX);

    clock_t t_gpu_wall_end = clock();
    double time_gpu_wall = (double)(t_gpu_wall_end - t_gpu_wall_start) / CLOCKS_PER_SEC;

    // copy final GPU solution to host
    CUDA_CHECK(cudaMemcpy(h_gpu_sol, d_U, sz*sizeof(double), cudaMemcpyDeviceToHost));

    // GPU error vs analytical
    double gpu_exact_err = 0.0;
    for (int k = 0; k < NI; k++)
        for (int l = 0; l < NI; l++) {
            double x = (l+1)*H, y = (k+1)*H;
            double exact = x*(1.0-x)*y*(1.0-y);
            gpu_exact_err = fmax(gpu_exact_err, fabs(h_gpu_sol[k*NI+l]-exact));
        }

    // max difference between GPU and serial solutions
    double max_diff = 0.0;
    for (int i = 0; i < sz; i++)
        max_diff = fmax(max_diff, fabs(h_gpu_sol[i] - h_u_old[i]));

    printf("\nGPU converged!\n");
    printf("  Iterations            : %d\n", gpu_iter);
    printf("  Final residual        : %e\n", gpu_res);
    printf("  GPU kernel time only  : %.3f seconds  (%.3f ms)\n",
           time_gpu_ms/1000.0f, time_gpu_ms);
    printf("  GPU total wall time   : %.3f seconds  (includes memcpy + residual)\n",
           time_gpu_wall);
    printf("  Max error vs exact    : %e\n\n", gpu_exact_err);

    // ══════════════════════════════════════════════════════════════════════════
    // PART 3 - COMPARISON TABLE
    // ══════════════════════════════════════════════════════════════════════════
    printf("=== Comparison ===\n");
    printf("  %-30s  %12s  %12s\n", "Metric", "Serial CPU", "MDH GPU");
    printf("  %-30s  %12d  %12d\n", "Iterations to converge",
           serial_iter, gpu_iter);
    printf("  %-30s  %12e  %12e\n", "Final residual",
           serial_res, gpu_res);
    printf("  %-30s  %12.3f  %12.3f\n", "Compute time (seconds)",
           time_serial, time_gpu_ms/1000.0f);
    printf("  %-30s  %12e  %12e\n", "Max error vs exact",
           serial_exact_err, gpu_exact_err);
    printf("  %-30s  %12s  %12e\n", "Max diff GPU vs serial",
           "---", max_diff);

    double speedup = time_serial / (time_gpu_ms/1000.0f);
    printf("\n  Speedup (serial / GPU kernel): %.2fx\n\n", speedup);

    if (max_diff < 1e-6)
        printf("Poisson (MDH) is SUCCESSFUL! Solutions match within 1e-6.\n");
    else
        printf("WARNING: GPU and serial solutions differ by %.3e\n", max_diff);

    // ── cleanup ────────────────────────────────────────────────────────────────
    cudaEventDestroy(ev_start);
    cudaEventDestroy(ev_stop);
    cudaFree(d_U); cudaFree(d_int_res);
    cudaFree(d_res_g); cudaFree(d_SOURCE);
    free(h_source); free(h_u_old);
    free(h_u_new);  free(h_gpu_sol);

    return (max_diff > 1e-6);
}
