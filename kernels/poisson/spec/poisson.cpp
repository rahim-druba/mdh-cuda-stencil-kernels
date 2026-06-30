#include "md_hom_generator.hpp"

/**
 * L1: row index (i)
 * L2: col index (j)
 * R dims: none (pure stencil, no reduction)
 *
 * Jacobi 2D 5-point stencil for the Poisson equation:
 * U_new[i][j] = 0.25 * (U[i-1][j] + U[i+1][j] + U[i][j-1] + U[i][j+1] + h2 * SOURCE[i][j])
 *
 * Neighborhood N(N(0,1,0), N(1,2,1), N(0,1,0)) is the exact 5-point cross:
 * top(-1,0), left(0,-1), center(0,0), right(0,+1), bottom(+1,0).
 * f() uses the 4 cardinal neighbors; center U_val is passed but unused.
 */
int main() {
    auto U = md_hom::input_stencil_buffer(
        "U",
        {md_hom::L(1), md_hom::L(2)},
        md_hom::N(md_hom::N(0,1,0), md_hom::N(1,2,1), md_hom::N(0,1,0)),
        md_hom::oob::ZERO
    );

    auto SOURCE = md_hom::input_buffer("SOURCE", {md_hom::L(1), md_hom::L(2)});
    auto h2     = md_hom::input_scalar("h2");
    auto result = md_hom::result_buffer("U_new", {md_hom::L(1), md_hom::L(2)});

    auto f = md_hom::scalar_function(
        "return 0.25f * (U_val_l1_m1 + U_val_l1_p1 + U_val_l2_m1 + U_val_l2_p1 + SOURCE_val * h2_val);"
    );
    auto g = md_hom::scalar_function("return res;");

    auto md_hom_poisson = md_hom::md_hom<2, 0>(
        "poisson",
        md_hom::inputs(U, SOURCE, h2),
        f, g,
        result,
        false, false
    );

    auto generator = md_hom::generator::cuda_generator(md_hom_poisson);
    std::ofstream kernel_file;

    kernel_file.open("poisson_1.cu", std::fstream::out | std::fstream::trunc);
    kernel_file << generator.kernel_1();
    kernel_file.close();

    kernel_file.open("poisson_2.cu", std::fstream::out | std::fstream::trunc);
    kernel_file << generator.kernel_2();
    kernel_file.close();
}
