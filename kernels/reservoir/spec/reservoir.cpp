#include "md_hom_generator.hpp"

/**
 * L1: row index (i), L2: col index (j), R dims: none
 *
 * Variable-coefficient 5-point stencil SpMV kernel extracted from GMRES
 * in Source.cpp (2-phase porous flow pressure solver).
 *
 * At each interior point (i,j):
 *   w[i,j] = COEFF_UP[i,j]     * V[i-1,j]
 *           + COEFF_LEFT[i,j]   * V[i,j-1]
 *           + COEFF_CENTER[i,j] * V[i,j]
 *           + COEFF_RIGHT[i,j]  * V[i,j+1]
 *           + COEFF_DOWN[i,j]   * V[i+1,j]
 *
 * Coefficient precomputation (done on CPU before calling this kernel):
 *   COEFF_UP[i,j]     = 0.5 * (Mx[i,j] + Mx[i-1,j])
 *   COEFF_LEFT[i,j]   = 0.5 * (My[i,j] + My[i,j-1])
 *   COEFF_RIGHT[i,j]  = 0.5 * (My[i,j+1] + My[i,j])
 *   COEFF_DOWN[i,j]   = 0.5 * (Mx[i+1,j] + Mx[i,j])
 *   COEFF_CENTER[i,j] = -(COEFF_UP + COEFF_LEFT + COEFF_RIGHT + COEFF_DOWN + h2/dt)
 *
 * V uses the same 5-point cross neighborhood as the Poisson stencil.
 * oob::ZERO handles zero Dirichlet boundary conditions on the ghost layer.
 */
int main() {
    // V: the vector being multiplied - 5-point cross stencil
    auto V = md_hom::input_stencil_buffer(
        "V",
        {md_hom::L(1), md_hom::L(2)},
        md_hom::N(md_hom::N(0,1,0), md_hom::N(1,2,1), md_hom::N(0,1,0)),
        md_hom::oob::ZERO
    );

    // Precomputed stencil coefficients - one value per interior grid point
    auto COEFF_UP     = md_hom::input_buffer("COEFF_UP",     {md_hom::L(1), md_hom::L(2)});
    auto COEFF_LEFT   = md_hom::input_buffer("COEFF_LEFT",   {md_hom::L(1), md_hom::L(2)});
    auto COEFF_CENTER = md_hom::input_buffer("COEFF_CENTER", {md_hom::L(1), md_hom::L(2)});
    auto COEFF_RIGHT  = md_hom::input_buffer("COEFF_RIGHT",  {md_hom::L(1), md_hom::L(2)});
    auto COEFF_DOWN   = md_hom::input_buffer("COEFF_DOWN",   {md_hom::L(1), md_hom::L(2)});

    auto result = md_hom::result_buffer("W", {md_hom::L(1), md_hom::L(2)});

    // f: weighted sum over 5 stencil points using local coefficients
    auto f = md_hom::scalar_function(
        "return COEFF_UP_val * V_val_l1_m1"
        " + COEFF_LEFT_val * V_val_l2_m1"
        " + COEFF_CENTER_val * V_val"
        " + COEFF_RIGHT_val * V_val_l2_p1"
        " + COEFF_DOWN_val * V_val_l1_p1;"
    );

    // g: identity - no reduction (R_DIMS = 0)
    auto g = md_hom::scalar_function("return res;");

    auto md_hom_reservoir = md_hom::md_hom<2, 0>(
        "reservoir",
        md_hom::inputs(V, COEFF_UP, COEFF_LEFT, COEFF_CENTER, COEFF_RIGHT, COEFF_DOWN),
        f, g,
        result,
        false, false
    );

    auto generator = md_hom::generator::cuda_generator(md_hom_reservoir);
    std::ofstream kernel_file;

    kernel_file.open("reservoir_1.cu", std::fstream::out | std::fstream::trunc);
    kernel_file << generator.kernel_1();
    kernel_file.close();

    kernel_file.open("reservoir_2.cu", std::fstream::out | std::fstream::trunc);
    kernel_file << generator.kernel_2();
    kernel_file.close();
}
