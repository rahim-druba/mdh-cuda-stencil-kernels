#include "md_hom_generator.hpp"

/**
 * L1: Batch size (number of samples)
 * L2: Number of output features (columns of W)
 * R1: Number of input features (columns of Z / rows of W)
 *
 * Computes: S[L1, L2] = sum_R1( Z[L1, R1] * W[L2, R1] )
 */
int main() {
    auto Z = md_hom::input_buffer("Z", {md_hom::L(1), md_hom::R(1)});
    auto W = md_hom::input_buffer("W", {md_hom::L(1), md_hom::L(2), md_hom::R(1)});
    auto result = md_hom::result_buffer("S", {md_hom::L(1), md_hom::L(2)});

    auto f = md_hom::scalar_function("return Z_val * W_val;");
    auto g = md_hom::scalar_function("return res;");  // identity — no activation

    auto md_hom_matmul = md_hom::md_hom<2, 1>(
        "matmul",
        md_hom::inputs(Z, W),
        f, g,
        result,
        true, true
    );

    auto generator = md_hom::generator::cuda_generator(md_hom_matmul);
    std::ofstream kernel_file;

    kernel_file.open("matmul_1.cu", std::fstream::out | std::fstream::trunc);
    kernel_file << generator.kernel_1();
    kernel_file.close();

    kernel_file.open("matmul_2.cu", std::fstream::out | std::fstream::trunc);
    kernel_file << generator.kernel_2();
    kernel_file.close();
}
