//
// CUDA kernel generator for the MDH (Multi-Dimensional Homomorphism) framework.
// Modeled directly on ocl_generator.hpp — only CUDA-specific syntax differs.
//

#ifndef MDH_CUDA_GENERATOR_HPP
#define MDH_CUDA_GENERATOR_HPP

#include <memory>

#include "helper.hpp"
#include "md_hom.hpp"
#include "input_buffer.hpp"
#include "cuda_input_buffer_wrapper.hpp"
#include "cuda_input_stencil_buffer_wrapper.hpp"
#include "input_wrapper.hpp"
#include "loop_generator.hpp"
#include "macros.hpp"
#include "result_buffer.hpp"
#include "types.hpp"
#include "cuda_result_buffer_wrapper.hpp"
#include "input_scalar_wrapper.hpp"

namespace md_hom {
namespace generator {

// ============================================================================
// CUDA thread-ID macro emission
//
// OpenCL uses get_global_id(N) where N is an integer dimension index.
// CUDA uses named axes: blockIdx.x/y/z, threadIdx.x/y/z, etc.
//
// Each MDH dimension is assigned a hardware axis via OCL_DIM_L_1, OCL_DIM_R_1
// etc. (integers 0/1/2).  We emit #if chains so the correct axis name is
// selected at kernel compile time, preserving the same OCL_DIM_* interface
// the rest of the framework already expects.
// ============================================================================

template <unsigned int L_DIMS, unsigned int R_DIMS>
static std::string emit_cuda_function_macros() {
    static const char* axes[3] = {"x", "y", "z"};
    std::stringstream ss;

    if (L_DIMS + R_DIMS <= 3) {
        for (const auto &dim : dim_range(L_DIMS, R_DIMS)) {
            ss << "\n";
            for (int d = 0; d < 3; ++d) {
                const char* ax = axes[d];
                ss << stringf("%s OCL_DIM_%c_%d == %d\n",
                              d == 0 ? "#if  " : "#elif",
                              dim.type, dim.nr, d);
                ss << stringf("#define GET_GLOBAL_ID_%c_%d   (blockIdx.%s * blockDim.%s + threadIdx.%s)\n",
                              dim.type, dim.nr, ax, ax, ax);
                ss << stringf("#define GET_LOCAL_ID_%c_%d    (threadIdx.%s)\n",
                              dim.type, dim.nr, ax);
                ss << stringf("#define GET_GROUP_ID_%c_%d    (blockIdx.%s)\n",
                              dim.type, dim.nr, ax);
                ss << stringf("#define GET_GLOBAL_SIZE_%c_%d (gridDim.%s * blockDim.%s)\n",
                              dim.type, dim.nr, ax, ax);
                ss << stringf("#define GET_LOCAL_SIZE_%c_%d  (blockDim.%s)\n",
                              dim.type, dim.nr, ax);
            }
            ss << "#endif";
        }
    } else {
        // >3 MDH dimensions: pack extra dimensions into CUDA's z axis using
        // stride arithmetic (mirrors the OpenCL >3 path in ocl_generator.hpp).
        for (unsigned int cuda_dim = 2; cuda_dim < L_DIMS + R_DIMS; ++cuda_dim) {
            bool first = true;
            for (const auto &dim : dim_range(L_DIMS, R_DIMS)) {
                ss << std::endl << stringf(
                        "%s OCL_DIM_%c_%d == %d\n#define NUM_WG_%d NUM_WG_%c_%d\n#define NUM_WI_%d NUM_WI_%c_%d",
                        first ? "#if  " : "#elif",
                        dim.type, dim.nr,
                        cuda_dim,
                        cuda_dim, dim.type, dim.nr,
                        cuda_dim, dim.type, dim.nr
                );
                first = false;
            }
            ss << std::endl << "#endif";
        }
        for (const auto &dim : dim_range(L_DIMS, R_DIMS)) {
            ss << stringf("\n#if OCL_DIM_%c_%d < 2\n", dim.type, dim.nr);
            for (int d = 0; d < 2; ++d) {
                const char* ax = axes[d];
                ss << stringf("%s OCL_DIM_%c_%d == %d\n",
                              d == 0 ? "#if  " : "#elif",
                              dim.type, dim.nr, d);
                ss << stringf("#define GET_GLOBAL_ID_%c_%d   (blockIdx.%s * blockDim.%s + threadIdx.%s)\n",
                              dim.type, dim.nr, ax, ax, ax);
                ss << stringf("#define GET_LOCAL_ID_%c_%d    (threadIdx.%s)\n",
                              dim.type, dim.nr, ax);
                ss << stringf("#define GET_GROUP_ID_%c_%d    (blockIdx.%s)\n",
                              dim.type, dim.nr, ax);
                ss << stringf("#define GET_GLOBAL_SIZE_%c_%d (gridDim.%s * blockDim.%s)\n",
                              dim.type, dim.nr, ax, ax);
                ss << stringf("#define GET_LOCAL_SIZE_%c_%d  (blockDim.%s)\n",
                              dim.type, dim.nr, ax);
            }
            ss << "#endif\n";
            ss << "#else\n";
            ss << stringf("#define GET_GLOBAL_SIZE_%c_%d (NUM_WG_%c_%d * NUM_WI_%c_%d)\n",
                          dim.type, dim.nr, dim.type, dim.nr, dim.type, dim.nr);
            ss << stringf("#define GET_LOCAL_SIZE_%c_%d  NUM_WI_%c_%d\n",
                          dim.type, dim.nr, dim.type, dim.nr);
            bool first = true;
            for (unsigned int cuda_dim = 2; cuda_dim < L_DIMS + R_DIMS; ++cuda_dim) {
                ss << std::endl << stringf(
                        "%s OCL_DIM_%c_%d == %d",
                        first ? "#if  " : "#elif",
                        dim.type, dim.nr,
                        cuda_dim
                );
                ss << stringf("\n#define GET_GLOBAL_ID_%c_%d    (blockIdx.z%s%s)",
                              dim.type, dim.nr,
                              cuda_dim > 2 ? stringf(" %% (%s)", concat(multi_stringf("NUM_WI_%d * NUM_WG_%d", uint_range(cuda_dim, L_DIMS + R_DIMS - 1), uint_range(cuda_dim, L_DIMS + R_DIMS - 1)), " * ")) : "",
                              cuda_dim < L_DIMS + R_DIMS - 1 ? stringf(" / (%s)", concat(multi_stringf("NUM_WI_%d * NUM_WG_%d", uint_range(cuda_dim + 1, L_DIMS + R_DIMS - 1), uint_range(cuda_dim + 1, L_DIMS + R_DIMS - 1)), " * ")) : ""
                );
                ss << stringf("\n#define GET_LOCAL_ID_%c_%d    (threadIdx.z%s%s)",
                              dim.type, dim.nr,
                              cuda_dim > 2 ? stringf(" %% (%s)", concat(multi_stringf("NUM_WI_%d", uint_range(cuda_dim, L_DIMS + R_DIMS - 1)), " * ")) : "",
                              cuda_dim < L_DIMS + R_DIMS - 1 ? stringf(" / (%s)", concat(multi_stringf("NUM_WI_%d", uint_range(cuda_dim + 1, L_DIMS + R_DIMS - 1)), " * ")) : ""
                );
                ss << stringf("\n#define GET_GROUP_ID_%c_%d    (blockIdx.z%s%s)",
                              dim.type, dim.nr,
                              cuda_dim > 2 ? stringf(" %% (%s)", concat(multi_stringf("NUM_WG_%d", uint_range(cuda_dim, L_DIMS + R_DIMS - 1)), " * ")) : "",
                              cuda_dim < L_DIMS + R_DIMS - 1 ? stringf(" / (%s)", concat(multi_stringf("NUM_WG_%d", uint_range(cuda_dim + 1, L_DIMS + R_DIMS - 1)), " * ")) : ""
                );
                first = false;
            }
            ss << "\n#endif";
            ss << "\n#endif";
        }
    }

    return ss.str();
}

// ============================================================================
// cuda_generator_class
// Mirrors ocl_generator_class with three targeted changes:
//   1. emit_cuda_function_macros() instead of ocl_function_macros stringstream
//   2. __global__ / __restrict__ kernel signatures instead of __kernel / restrict
//   3. __syncthreads() / __threadfence() barriers (delegated to cuda_result_buffer_wrapper)
//   4. cuda_input_buffer_wrapper / cuda_result_buffer_wrapper instead of OCL wrappers
// ============================================================================

template <unsigned int L_DIMS, unsigned int R_DIMS, typename T, typename... Ts>
class cuda_generator_class {
public:
    explicit cuda_generator_class(const md_hom_class<L_DIMS, R_DIMS, T, Ts...> &md_hom)
            : _md_hom(md_hom), _macros(), _input_wrappers(), _input_names(), _result_input_names(),
              _scalar_function_wrapper(), _result_wrapper(md_hom.result(), _macros, _input_wrappers, md_hom.apply_result_function(), md_hom.keep_both_results()) {
        wrap_inputs(md_hom.inputs(), std::make_index_sequence<sizeof...(Ts)>());
        get_input_names_and_definitions(md_hom.inputs(), std::make_index_sequence<sizeof...(Ts)>());

        _scalar_function_wrapper = std::make_unique<scalar_function_wrapper>(md_hom.get_scalar_function(), _input_wrappers, false);
        _result_function_wrapper = std::make_unique<scalar_function_wrapper>(md_hom.get_result_function(), _input_wrappers, true);

        auto second_kernel_input_buffer = input_buffer("int_res", join(_md_hom.result().dimension_order(), dim_range(0, R_DIMS)));
        cuda_input_buffer_wrapper<L_DIMS, R_DIMS> second_kernel_input_wrapper(second_kernel_input_buffer, _macros);

        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            auto& kernel_source = kernel == 1 ? _kernel_1 : _kernel_2;

            kernel_source = stringf(
                    R"(// helper macros
%s
%s

%s
%s

%s%s
%s

#define PRIVATE 0
#define LOCAL   1
#define GLOBAL  2)",
                    descending_ocl_dimension_order<L_DIMS, R_DIMS>(dim_range(L_DIMS, R_DIMS), [&](dimension_t dim, unsigned int order_nr) -> std::string {
                        return stringf("#define CONCAT_IN_DESCENDING_OCL_ORDER_%d(%s) %s",
                                       order_nr,
                                       make_standard_params(L_DIMS + R_DIMS),
                                       make_standard_indices(V(dim), L_DIMS, R_DIMS).front()
                        );
                    }),
                    stringf("#define CONCAT_IN_DESCENDING_OCL_ORDER(%s) %s",
                            make_standard_params(L_DIMS + R_DIMS),
                            concat(multi_stringf("CONCAT_IN_DESCENDING_OCL_ORDER_%d(%s)",
                                                 uint_range(L_DIMS + R_DIMS - 1, 0),
                                                 V(make_standard_params(L_DIMS + R_DIMS))
                            ))),
                    descending_ocl_dimension_order<L_DIMS, R_DIMS>(dim_range(L_DIMS, R_DIMS), [&](dimension_t dim, unsigned int order_nr) -> std::string {
                        return stringf("#define FLAT_INDEX_IN_DESCENDING_OCL_ORDER_%d(%s, %s) %s",
                                       order_nr,
                                       make_standard_params(L_DIMS + R_DIMS, "%%val%%_id"),
                                       make_standard_params(L_DIMS + R_DIMS, "%%val%%_size"),
                                       order_nr < L_DIMS + R_DIMS - 1
                                       ? stringf("(FLAT_INDEX_IN_DESCENDING_OCL_ORDER_%d(%s, %s)) * (%s) + (%s)",
                                                 order_nr + 1,
                                                 make_standard_params(L_DIMS + R_DIMS, "%%val%%_id"),
                                                 make_standard_params(L_DIMS + R_DIMS, "%%val%%_size"),
                                                 make_standard_indices(V(dim), L_DIMS, R_DIMS, "%%val%%_size").front(),
                                                 make_standard_indices(V(dim), L_DIMS, R_DIMS, "%%val%%_id").front())
                                       : make_standard_indices(V(dim), L_DIMS, R_DIMS, "%%val%%_id").front()
                        );
                    }),
                    stringf("#define FLAT_INDEX_IN_DESCENDING_OCL_ORDER(%s, %s) FLAT_INDEX_IN_DESCENDING_OCL_ORDER_0(%s, %s)",
                            make_standard_params(L_DIMS + R_DIMS, "%%val%%_id"),
                            make_standard_params(L_DIMS + R_DIMS, "%%val%%_size"),
                            make_standard_params(L_DIMS + R_DIMS, "%%val%%_id"),
                            make_standard_params(L_DIMS + R_DIMS, "%%val%%_size")),
                    L_DIMS == 0 ? "" : descending_ocl_dimension_order<L_DIMS, R_DIMS>(dim_range(L_DIMS, 0), [&](dimension_t dim, unsigned int order_nr) -> std::string {

                        return stringf("#define DESCENDING_L_DIMS_%d(%s) %s",

                                       order_nr,

                                       make_standard_params(L_DIMS),

                                       make_standard_indices(V(dim), L_DIMS, 0).front()

                        );

                    }),
                    kernel == 1 ? "" : "\n\n#define CEIL(x,y) (((x) + (y) - 1) / (y))\n",
                    emit_cuda_function_macros<L_DIMS, R_DIMS>()   // replaces ocl_function_macros
            );
            kernel_source.append(stringf(R"(

// =============== macro definitions per dimension ============================
%s
// =============== end of macro definitions per dimension =====================)",
                                         _macros.definitions(kernel)
            ));
            kernel_source.append("\n\n// =============== macro definitions per buffer ===============================\n");
            if (kernel == 1) {
                for (const auto &wrapper : _input_wrappers) {
                    if (wrapper->has_definitions(kernel)) {
                        kernel_source.append(wrapper->definitions(kernel));
                        kernel_source.append("\n\n\n");
                    }
                }
            } else {
                kernel_source.append(second_kernel_input_wrapper.definitions(kernel));
                kernel_source.append("\n\n\n");
                for (const auto &wrapper : _input_wrappers) {
                    if (wrapper->result_input() && wrapper->has_definitions(kernel)) {
                        kernel_source.append(wrapper->definitions(kernel));
                        kernel_source.append("\n\n\n");
                    }
                }
            }
            kernel_source.append(_result_wrapper.definitions(kernel));
            kernel_source.append("\n// =============== end of macro definitions per buffer ========================");
            // __device__ is required so f/g can be called from a __global__ kernel.
            // scalar_function_wrapper emits plain "inline", which defaults to __host__.
            auto cuda_device_fn = [](const std::string &def) {
                return search_and_replace(def, "inline TYPE_TS ", "__device__ inline TYPE_TS ");
            };
            if (kernel == 1) {
                kernel_source.append("\n\n// =============== scalar function ============================================\n");
                kernel_source.append(cuda_device_fn(_scalar_function_wrapper->definition()));
                kernel_source.append("\n// =============== end of scalar function =====================================");
            }
            kernel_source.append("\n\n// =============== result function ============================================\n");
            kernel_source.append(search_and_replace(cuda_device_fn(_result_function_wrapper->definition()), "f(", "g("));
            kernel_source.append("\n// =============== end of result function =====================================");
            kernel_source.append(stringf("\n\n// =============== kernel %d ===================================================\n", kernel));
            kernel_source.append(stringf(
                    "__global__ void %s_%d(%s%s, TYPE_TS * const __restrict__ res_g, TYPE_TS * const __restrict__ %s%s) {\n",
                    md_hom.routine_name(), kernel,
#ifdef RUNTIME_INPUT_SIZE
                    concat(multi_stringf("const size_t %s", _macros.cb_size(V(kernel), V(LEVEL::GLOBAL), dim_range(L_DIMS, R_DIMS))), ", ") + ", ",
#else
                    "",
#endif
                    concat((kernel == 1 ? _input_parameter_definitions : join(std::string("TYPE_T const * const __restrict__ int_res"), _result_input_parameter_definitions)), ", "),
                    (kernel == 1 ? "int_res" : _result_wrapper.name()),
                    _md_hom.keep_both_results() ? stringf(", TYPE_TS * const __restrict__ %s_orig", _result_wrapper.name()) : ""
            ));
            // define variables
            kernel_source.append(indent("// map md_hom dimensions to CUDA dimensions\n", 1));
            for (const auto &dim : dim_range(L_DIMS, R_DIMS)) {
                kernel_source.append(indent(stringf("const size_t i_wg_%c_%d = GET_GROUP_ID_%c_%d;\n", lower_case(dim.type), dim.nr, dim.type, dim.nr), 1));
                kernel_source.append(indent(stringf("const size_t i_wi_%c_%d = GET_LOCAL_ID_%c_%d;\n", lower_case(dim.type), dim.nr, dim.type, dim.nr), 1));
                kernel_source.append(indent("\n", 1));
            }
            kernel_source.append(indent("// declare variables for caching inputs\n", 1));
            for (auto level : {LEVEL::LOCAL, LEVEL::PRIVATE}) {
#ifndef INDIVIDUAL_INPUT_CACHING
                kernel_source.append(indent(stringf("#if CACHE_%c_CB != 0\n", level), 1));
#endif
                if (kernel == 1) {
                    for (const auto &input : _input_wrappers) {
                        if (input->supports_caching()) {
#ifdef INDIVIDUAL_INPUT_CACHING
                            kernel_source.append(indent(stringf("#if %s_CACHE_%c_CB != 0\n", upper_case(input->input_name()), level), 1));
#endif
                            kernel_source.append(indent(input->caching_variable_declaration(kernel, level), 1));
#ifdef INDIVIDUAL_INPUT_CACHING
                            kernel_source.append(indent("#endif\n", 1));
#endif
                        }
                    }
                } else {
                    if (second_kernel_input_wrapper.supports_caching()) {
#ifdef INDIVIDUAL_INPUT_CACHING
                        kernel_source.append(indent(stringf("#if %s_CACHE_%c_CB != 0\n", upper_case(second_kernel_input_wrapper.input_name()), level), 1));
#endif
                        kernel_source.append(indent(second_kernel_input_wrapper.caching_variable_declaration(kernel, level), 1));
#ifdef INDIVIDUAL_INPUT_CACHING
                        kernel_source.append(indent("#endif\n", 1));
#endif
                    }
                }
#ifndef INDIVIDUAL_INPUT_CACHING
                kernel_source.append(indent("#endif\n", 1));
#endif
            }
            kernel_source.append("\n");
            kernel_source.append(indent(_result_wrapper.variable_declarations(kernel), 1));
            kernel_source.append("\n");
            kernel_source.append("\n");
            if (R_DIMS > 0) {
                kernel_source.append(indent(_result_wrapper.reset_reduction_memory_space(kernel), 1));
                kernel_source.append("\n");
                kernel_source.append("\n");
            }

            // generate loops
            auto loops = loop_generator<L_DIMS, R_DIMS>(_macros, V(LEVEL::LOCAL, LEVEL::PRIVATE),
                                                        dim_range(L_DIMS, R_DIMS));
            // any level, last dimension
            loops.add_code({ANY, ANY, TOTAL_LAST}, INNER_PREPEND,
                           [&](auto level, const auto &dimension, bool *dim_needs_guard, bool *first_iteration) -> std::string {
                               std::string caching_code;
                               caching_code.append(stringf("// ---------- %c caching --------------------\n", level));
#ifndef INDIVIDUAL_INPUT_CACHING
                               caching_code.append(stringf("#if CACHE_%c_CB != 0\n", level));
#endif
                               if (kernel == 1) {
                                   for (const auto &input : _input_wrappers) {
                                       if (input->supports_caching()) {
#ifdef INDIVIDUAL_INPUT_CACHING
                                           caching_code.append(stringf("#if %s_CACHE_%c_CB != 0\n", upper_case(input->input_name()), level));
#endif
                                           caching_code.append(input->caching_copy_code(kernel, level, dim_needs_guard));
#ifdef INDIVIDUAL_INPUT_CACHING
                                           caching_code.append("#endif\n");
#endif
                                       }
                                   }
                               } else {
                                   if (second_kernel_input_wrapper.supports_caching()) {
#ifdef INDIVIDUAL_INPUT_CACHING
                                       caching_code.append(stringf("#if %s_CACHE_%c_CB != 0\n", upper_case(second_kernel_input_wrapper.input_name()), level));
#endif
                                       caching_code.append(second_kernel_input_wrapper.caching_copy_code(kernel, level, dim_needs_guard));
#ifdef INDIVIDUAL_INPUT_CACHING
                                       caching_code.append("#endif\n");
#endif
                                   }
                               }
                               if (level == LEVEL::LOCAL) {
                                   caching_code.append("__syncthreads();\n");   // CUDA barrier
                               }
#ifndef INDIVIDUAL_INPUT_CACHING
                               caching_code.append("#endif\n");
#endif
                               caching_code.append(stringf("// ---------- end of %c caching -------------", level));
                               return caching_code;
                           });
            // local level, last dimension
            loops.add_code({FIRST, ANY, TOTAL_LAST}, INNER_APPEND,
                           [&](auto level, const auto &dimension, bool *dim_needs_guard, bool *first_iteration) -> std::string {
                               std::string code = "// wait for all threads to finish computation on shared cache block\n";
#ifdef INDIVIDUAL_INPUT_CACHING
                               code.append("#if ");
                               bool first = true;
                               for (const auto &input_wrapper : _input_wrappers) {
                                   if (!first) code.append(" || ");
                                   code.append(stringf("%s_CACHE_L_CB != 0", upper_case(input_wrapper->input_name())));
                                   first = false;
                               }
                               code.append("\n");
#else
                               code.append("#if CACHE_L_CB != 0\n");
#endif
                               code.append("__syncthreads();\n");   // CUDA barrier
                               code.append("#endif");
                               return code;
                           });
            // any level, first r dimension
            loops.add_code({ANY, LAST, FIRST}, APPEND,
                           [&](auto level, const auto &dimension, bool *dim_needs_guard, bool *first_iteration) -> std::string {
                               return _result_wrapper.reduction(kernel, level, dim_needs_guard, _input_names);
                           });
            // any level, between l and r dimensions
            loops.add_code({ANY, R_DIMS > 0 ? LAST : FIRST, R_DIMS > 0 ? FIRST : LAST}, R_DIMS > 0 ? APPEND : INNER_APPEND,
                           [&](auto level, const auto &dimension, bool *dim_needs_guard, bool *first_iteration) -> std::string {
                               return _result_wrapper.write_back_code(kernel, level, dim_needs_guard, first_iteration);
                           });
            if (R_DIMS > 0) {
                // apply g to final results in global memory if it could not be done before
                loops.add_code({FIRST, LAST, FIRST}, APPEND,
                               [&](auto level, const auto &dimension, bool *dim_needs_guard, bool *first_iteration) -> std::string {
                                   std::string code;
                                   if (_md_hom.apply_result_function()) {
                                       std::string result_inputs;
                                       for (const auto &input : _input_wrappers) {
                                           if (input->result_input()) {
                                               result_inputs += ", " + input->call_macro(kernel, LEVEL::PRIVATE, cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, R_DIMS)));
                                           }
                                       }
                                       std::string inner_code = stringf("K%d_G_CB_RES_DEST(%s) = g(K%d_G_CB_RES_DEST(%s)%s);",
                                                                        kernel, concat(cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, 0)), ", "),
                                                                        kernel, concat(cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, 0)), ", "), result_inputs
                                       );
                                       if (_md_hom.keep_both_results()) {
                                           inner_code.append(stringf("\nK%d_P_KERNEL_RES_ORIG(%s) = K%d_G_CB_RES_DEST(%s);",
                                                                     kernel, concat(cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, 0)), ", "),
                                                                     kernel, concat(cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, 0)), ", ")));
                                       }
                                       if (L_DIMS > 0) {
                                           auto l_loops = loop_generator<L_DIMS, R_DIMS>(_macros, V(LEVEL::LOCAL, LEVEL::PRIVATE), dim_range(L_DIMS, 0));
                                           inner_code = l_loops.from(kernel, sub_level(level), L(1),
                                                                     [&] (auto inner_level, const auto &inner_dimension, bool *inner_dim_needs_guard, bool *first_iteration) -> std::string {
                                                                         return wrap_in_cb_processing_loop<L_DIMS, R_DIMS>(kernel, LEVEL::PRIVATE, dim_range(L_DIMS, 0), inner_code, true, inner_dim_needs_guard);
                                                                     }, true, dim_needs_guard);
                                       }

                                       code.append("// apply g to final result if not done before\n");
                                       code.append(stringf("#if P_CB_RES_DEST_LEVEL == GLOBAL && %s == 1 && %s == 1\n", _macros.num_fu(kernel, LEVEL::GLOBAL, R(1)), _macros.num_fu(kernel, LEVEL::LOCAL, R(1))));
                                       code.append(inner_code);
                                       code.append("\n#endif");
                                   }
                                   return code;
                               });
            }
            kernel_source.append(indent(loops.from(
                    kernel, LEVEL::LOCAL, L_DIMS > 0 ? L(1) : R(1),
                    [&](auto level, const auto &dimension, bool *dim_needs_guard, bool *first_iteration) -> std::string {
                        std::string loop_body = "// process one mda element\n";
                        std::string function_call;
                        if (kernel == 1) {
                            std::vector<std::string> scalar_function_inputs;
                            for (const auto &input : _input_wrappers) {
                                if (!input->result_input()) {
                                    for (const auto &param : input->scalar_function_parameter_values()) {
                                        scalar_function_inputs.push_back(param);
                                    }
                                }
                            }
                            loop_body.append(stringf("%s %s %s;\n",
                                                     _result_wrapper.call_res_macro(kernel, LEVEL::PRIVATE, cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, 0))),
                                                     "%s",
                                                     _scalar_function_wrapper->call(scalar_function_inputs)));
                            function_call = _scalar_function_wrapper->call(scalar_function_inputs);
                        } else {
                            loop_body.append(stringf("%s %s %s;\n",
                                                     _result_wrapper.call_res_macro(kernel, LEVEL::PRIVATE, cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, 0))),
                                                     "%s",
                                                     second_kernel_input_wrapper.call_macro(kernel, LEVEL::PRIVATE, cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, R_DIMS)))));
                            function_call = second_kernel_input_wrapper.call_macro(kernel, LEVEL::PRIVATE, cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, R_DIMS)));
                        }

                        std::string code;
                        if (dimension.type == DIM_TYPE::L) {
                            std::string result_inputs;
                            for (const auto &input : _input_wrappers) {
                                if (input->result_input()) {
                                    result_inputs += ", " + input->call_macro(kernel, LEVEL::PRIVATE, cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, R_DIMS)));
                                }
                            }

                            if (_md_hom.apply_result_function()) {
                                code.append(stringf("#if %c_CB_RES_DEST_LEVEL == GLOBAL\n", level));
                                code.append(wrap_in_cb_processing_loop<L_DIMS, R_DIMS>(
                                        kernel,
                                        LEVEL::PRIVATE,
                                        dim_range(L_DIMS, R_DIMS),
                                        search_and_replace(stringf(loop_body, "="),
                                                           function_call,
                                                           stringf("g(%s%s);%s",
                                                                   function_call,
                                                                   result_inputs,
                                                                   _md_hom.keep_both_results()
                                                                   ? stringf("\nK%d_P_KERNEL_RES_ORIG(%s) = %s", kernel, concat(cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, 0)), ", "), function_call)
                                                                   : "")),
                                        true,
                                        dim_needs_guard));
                                code.append("\n#else\n");
                            }
                            code.append(wrap_in_cb_processing_loop<L_DIMS, R_DIMS>(
                                    kernel,
                                    LEVEL::PRIVATE,
                                    dim_range(L_DIMS, R_DIMS),
                                    stringf(loop_body, "="),
                                    true,
                                    dim_needs_guard));
                            if (_md_hom.apply_result_function()) {
                                code.append("\n#endif\n");
                            }
                        } else {
                            if (first_iteration[LEVEL_ID(level)]) {
                                auto first_level_not_in_first_iter = get_first_parent_level_not_in_first_iteration(level, first_iteration);
                                if (first_level_not_in_first_iter != level) {
                                    code.append(stringf("#if %c_CB_RES_DEST_LEVEL > P_CB_RES_DEST_LEVEL\n", first_level_not_in_first_iter));
                                }
#ifdef RUNTIME_INPUT_SIZE
                                if (dim_needs_guard[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(level, dimension)]) {
                                    code.append(wrap_in_cb_processing_loop<L_DIMS, R_DIMS>(
                                            kernel, LEVEL::PRIVATE, dim_range(L_DIMS, 0),
                                            stringf(R"(size_t %s = 0;
%s
for (%s = 1; %s < %s; ++%s) {
%s
}
)",
                                                    cb_processing_loop_variable(LEVEL::PRIVATE, R(1)),
                                                    stringf(loop_body, "="),
                                                    cb_processing_loop_variable(LEVEL::PRIVATE, R(1)),
                                                    cb_processing_loop_variable(LEVEL::PRIVATE, R(1)),
                                                    _macros.cb_size(kernel, LEVEL::PRIVATE, R(1), dim_needs_guard),
                                                    cb_processing_loop_variable(LEVEL::PRIVATE, R(1)),
                                                    indent(stringf(loop_body, "+="), 1)
                                            ),
                                            true, dim_needs_guard)
                                    );
                                } else {
#endif
                                    code.append(wrap_in_cb_processing_loop<L_DIMS, R_DIMS>(
                                            kernel, LEVEL::PRIVATE, dim_range(L_DIMS, 0),
                                            stringf(R"(size_t %s = 0;
%s
#if %s > 1
for (%s = 1; %s < %s; ++%s) {
%s
}
#endif
)",
                                                    cb_processing_loop_variable(LEVEL::PRIVATE, R(1)),
                                                    stringf(loop_body, "="),
                                                    _macros.cb_size(kernel, LEVEL::PRIVATE, R(1), dim_needs_guard),
                                                    cb_processing_loop_variable(LEVEL::PRIVATE, R(1)),
                                                    cb_processing_loop_variable(LEVEL::PRIVATE, R(1)),
                                                    _macros.cb_size(kernel, LEVEL::PRIVATE, R(1), dim_needs_guard),
                                                    cb_processing_loop_variable(LEVEL::PRIVATE, R(1)),
                                                    indent(stringf(loop_body, "+="), 1)
                                            ),
                                            true, dim_needs_guard)
                                    );
#ifdef RUNTIME_INPUT_SIZE
                                }
#endif

                                if (first_level_not_in_first_iter != level) {
                                    code.append("\n#else\n");
                                    code.append(wrap_in_cb_processing_loop<L_DIMS, R_DIMS>(
                                            kernel,
                                            LEVEL::PRIVATE,
                                            dim_range(L_DIMS, R_DIMS),
                                            stringf(loop_body, "+="),
                                            true,
                                            dim_needs_guard));
                                    code.append("\n#endif");
                                }
                            } else {
                                code.append(wrap_in_cb_processing_loop<L_DIMS, R_DIMS>(
                                        kernel,
                                        LEVEL::PRIVATE,
                                        dim_range(L_DIMS, R_DIMS),
                                        stringf(loop_body, "+="),
                                        true,
                                        dim_needs_guard));
                            }
                        }
                        return code;
                    }), 1));

            kernel_source.append("\n}\n");
            kernel_source.append(stringf("// =============== end of kernel %d ============================================", kernel));
        }
    }

    std::string kernel_1() const {
        return _kernel_1;
    }

    std::string kernel_2() const {
        return _kernel_2;
    }

private:
    const md_hom_class<L_DIMS, R_DIMS, T, Ts...>        &_md_hom;

    const macros<L_DIMS, R_DIMS>                          _macros;
    std::vector<std::unique_ptr<input_wrapper>>           _input_wrappers;
    std::vector<std::string>                              _input_names;
    std::vector<std::string>                              _result_input_names;
    std::vector<std::string>                              _input_parameter_definitions;
    std::vector<std::string>                              _result_input_parameter_definitions;
    std::unique_ptr<scalar_function_wrapper>              _scalar_function_wrapper;
    std::unique_ptr<scalar_function_wrapper>              _result_function_wrapper;
    const cuda_result_buffer_wrapper<L_DIMS, R_DIMS>      _result_wrapper;

    std::string _kernel_1;
    std::string _kernel_2;

    template< size_t... Is >
    void wrap_inputs(const std::tuple<Ts...> &inputs, std::index_sequence<Is...>) {
        wrap_inputs_impl(std::get<Is>(inputs)...);
    }
    template<typename... ARGS>
    void wrap_inputs_impl(const input_buffer_class &buffer, ARGS&... args) {
        _input_wrappers.emplace_back(new cuda_input_buffer_wrapper<L_DIMS, R_DIMS>(buffer, _macros));
        wrap_inputs_impl(args...);
    }
    template<typename... ARGS>
    void wrap_inputs_impl(const input_scalar_class &scalar, ARGS&... args) {
        _input_wrappers.emplace_back(new input_scalar_wrapper<L_DIMS, R_DIMS>(scalar, _macros));
        wrap_inputs_impl(args...);
    }
    template<typename NT, typename... ARGS>
    void wrap_inputs_impl(const input_stencil_buffer_class<NT> &buffer, ARGS&... args) {
        _input_wrappers.emplace_back(new cuda_input_stencil_buffer_wrapper<NT, L_DIMS, R_DIMS>(buffer, _macros));
        wrap_inputs_impl(args...);
    }
    void wrap_inputs_impl() {}

    template< size_t... Is >
    void get_input_names_and_definitions(const std::tuple<Ts...> &inputs, std::index_sequence<Is...>) {
        get_input_names_and_definitions_impl(std::get<Is>(inputs)...);
    }
    template<typename... ARGS>
    void get_input_names_and_definitions_impl(const input_buffer_class &buffer, ARGS&... args) {
        if (buffer.result_input()) {
            _result_input_names.push_back(buffer.name());
            _result_input_parameter_definitions.push_back("TYPE_T const * const __restrict__ " + buffer.name());
        }
        _input_names.push_back(buffer.name());
        _input_parameter_definitions.push_back("TYPE_T const * const __restrict__ " + buffer.name());
        get_input_names_and_definitions_impl(args...);
    }
    template<typename... ARGS>
    void get_input_names_and_definitions_impl(const input_scalar_class &scalar, ARGS&... args) {
        _input_names.push_back(scalar.name());
        _input_parameter_definitions.push_back("const TYPE_T " + scalar.name());
        get_input_names_and_definitions_impl(args...);
    }
    template<typename NT, typename... ARGS>
    void get_input_names_and_definitions_impl(const input_stencil_buffer_class<NT> &buffer, ARGS&... args) {
        _input_names.push_back(buffer.name());
        _input_parameter_definitions.push_back("TYPE_T const * const __restrict__ " + buffer.name());
        get_input_names_and_definitions_impl(args...);
    }
    void get_input_names_and_definitions_impl() {}
};

template <unsigned int L_DIMS, unsigned int R_DIMS, typename T, typename... Ts>
auto cuda_generator(const md_hom_class<L_DIMS, R_DIMS, T, Ts...> &md_hom) {
    return cuda_generator_class<L_DIMS, R_DIMS, T, Ts...>(md_hom);
}

}
}

#endif // MDH_CUDA_GENERATOR_HPP
