//
// Created by   on 06.11.17.
//

#ifndef MD_BLAS_INPUT_BUFFER_WRAPPER_HPP
#define MD_BLAS_INPUT_BUFFER_WRAPPER_HPP

#include "input_wrapper.hpp"
#include "macros.hpp"
#include "types.hpp"

namespace md_hom {
namespace generator {


template<unsigned int L_DIMS, unsigned int R_DIMS>
class input_buffer_wrapper : public input_wrapper {
public:
    input_buffer_wrapper(const input_buffer_class &input_buffer, const macros <L_DIMS, R_DIMS> &macros)
            : _input_buffer(input_buffer), _macros(macros) {
        _macro_name_pattern = "K%d_%c_MEM_";
        _macro_name_pattern.append(upper_case(input_buffer.name()));

        _macro_call_pattern = _macro_name_pattern;
        _macro_call_pattern.append("(");
        for (int i = 0; i < L_DIMS + R_DIMS; ++i) {
            if (i > 0) _macro_call_pattern.append(", ");
            _macro_call_pattern.append("%s");
        }
        _macro_call_pattern.append(")");

        _cb_size_pattern = "K1_%c_CB_SIZE_";
        _cb_size_pattern.append(upper_case(input_buffer.name()));
    }

    std::string input_name() const {
        return _input_buffer.name();
    }

    bool result_input() const {
        return _input_buffer.result_input();
    }

    std::string macro_name(unsigned int kernel, LEVEL level) const {
        return stringf(_macro_name_pattern, kernel, level);
    }

    std::string call_macro(unsigned int kernel, LEVEL level,
                           const std::vector<std::string> &parameters) const {
        std::string call = stringf_p(_macro_call_pattern, kernel, level);
        for (auto &param : parameters) {
            call = stringf_p(call, param);
        }
        return call;
    }

    bool has_definitions(unsigned int kernel) const {
        return true;
    }

    std::string definitions(unsigned int kernel) const {
        std::stringstream ss;
        ss << stringf("// -------------------- buffer %s --------------------",
                      upper_case(_input_buffer.name())).c_str() << std::endl;
        ss << std::endl;
        ss << "// buffer abstraction" << std::endl;
        ss << descending_ocl_dimension_order<L_DIMS, R_DIMS>(
                _input_buffer.used_dimensions(),
                [&](dimension_t dim, unsigned int order_nr) -> std::string {
                    std::string inner_code = stringf(
                            "#define BUFFER_%s_INDEX_%d(%s) %s",
                            upper_case(_input_buffer.name()), order_nr,
                            make_standard_params_retain_order<L_DIMS, R_DIMS>(_input_buffer.used_dimensions()),
                            make_standard_indices(V(dim), L_DIMS, R_DIMS).front());
                    for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                        inner_code.append(stringf(
                                "\n#define BUFFER_%s_%c_SIZE_%d %s",
                                upper_case(_input_buffer.name()), level, order_nr,
                                kernel == 2 && level == LEVEL::GLOBAL && dim.type == DIM_TYPE::R ? stringf("NUM_WG_R_%d", dim.nr) : _macros.cb_size(kernel, level, dim)));
                    }
                    return inner_code;
                }).c_str() << std::endl;
        for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
            ss << "#define "
               << stringf("K%d_%c_BUFFER_%s(%s)", kernel, level, upper_case(_input_buffer.name()),
                          make_standard_params_retain_order<L_DIMS, R_DIMS>(_input_buffer.used_dimensions()))
               << " " << (level == LEVEL::GLOBAL ? _input_buffer.name() : stringf("cb_%c_%s", lower_case(level),
                                                                                  _input_buffer.name()));

            if (kernel == 1 && level == LEVEL::GLOBAL) {
                ss << wrap({make_flat_index(make_standard_indices(_input_buffer.used_dimensions(), L_DIMS, R_DIMS), _macros.cb_size(V(kernel), V(LEVEL::GLOBAL), _input_buffer.used_dimensions()))}, "[", "]")[0];
            } else if (kernel == 2 && level == LEVEL::GLOBAL) {
                ss << wrap({make_flat_index(multi_stringf("BUFFER_%s_INDEX_%d(%s)", V(upper_case(_input_buffer.name())), uint_range(_input_buffer.used_dimensions().size() - 1, 0), V(make_standard_params_retain_order<L_DIMS, R_DIMS>(_input_buffer.used_dimensions()))),
                                            multi_stringf("BUFFER_%s_%c_SIZE_%d", V(upper_case(_input_buffer.name())), V(level), uint_range(_input_buffer.used_dimensions().size() - 1, 0))
                )}, "[", "]")[0];
            } else {
                ss << stringf(concat(wrap(multi_stringf("BUFFER_%s_INDEX_%d(%s)", V(upper_case(_input_buffer.name())), uint_range(_input_buffer.used_dimensions().size() - 1, 0), V(make_standard_params_retain_order<L_DIMS, R_DIMS>(_input_buffer.used_dimensions()))), "[(", ")]")));
            }
            ss << std::endl;
        }
        ss << std::endl;
        ss << "// partitioning and cache usage" << std::endl;
        for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
            if (_input_buffer.result_input()) {
                if (level == LEVEL::GLOBAL) {
                    ss << stringf("#define K%d_%c_MEM_%s(%s) ", kernel, level, upper_case(_input_buffer.name()),
                                  make_standard_params(L_DIMS + R_DIMS)).c_str()
                       << stringf("K%d_%c_BUFFER_%s(%s)", kernel, level, upper_case(_input_buffer.name()),
                                  make_standard_params_retain_order<L_DIMS, R_DIMS>(_input_buffer.used_dimensions()))
                       << std::endl;
                } else {
                    auto complete_dim_range = dim_range(L_DIMS, R_DIMS);
                    ss << stringf("#define K%d_%c_MEM_%s(%s) ", kernel, level, upper_case(_input_buffer.name()),
                                  make_standard_params(L_DIMS + R_DIMS)).c_str()
                       << stringf("K%d_%c_MEM_%s(%s)", kernel, parent_level(level), upper_case(_input_buffer.name()),
                                  make_standard_params(L_DIMS + R_DIMS, "K%d_%c_CB_OFFSET_%c_%d + (%%val%%)",
                                                       V(kernel),
                                                       V(level),
                                                       split_dim_range(complete_dim_range).first,
                                                       split_dim_range(complete_dim_range).second))
                       << std::endl;
                }
            } else {
                if (level != LEVEL::GLOBAL) {
#ifdef INDIVIDUAL_INPUT_CACHING
                    ss << stringf("#if %s_CACHE_%c_CB != 0", upper_case(input_name()), level).c_str() << std::endl;
#else
                    ss << stringf("#if CACHE_%c_CB != 0", level).c_str() << std::endl;
#endif
                }
                ss << stringf("#define K%d_%c_MEM_%s(%s) ", kernel, level, upper_case(_input_buffer.name()),
                              make_standard_params(L_DIMS + R_DIMS)).c_str()
                   << stringf("K%d_%c_BUFFER_%s(%s)", kernel, level, upper_case(_input_buffer.name()),
                              make_standard_params_retain_order<L_DIMS, R_DIMS>(_input_buffer.used_dimensions()))
                   << std::endl;
                if (level != LEVEL::GLOBAL) {
                    ss << "#else" << std::endl;
                    auto complete_dim_range = dim_range(L_DIMS, R_DIMS);
                    ss << stringf("#define K%d_%c_MEM_%s(%s) ", kernel, level, upper_case(_input_buffer.name()),
                                  make_standard_params(L_DIMS + R_DIMS)).c_str()
                       << stringf("K%d_%c_MEM_%s(%s)", kernel, parent_level(level), upper_case(_input_buffer.name()),
                                  make_standard_params(L_DIMS + R_DIMS, "K%d_%c_CB_OFFSET_%c_%d + (%%val%%)",
                                                       V(kernel),
                                                       V(level),
                                                       split_dim_range(complete_dim_range).first,
                                                       split_dim_range(complete_dim_range).second))
                       << std::endl;
                    ss << "#endif" << std::endl;
                }
            }
        }
        ss << std::endl;
        ss << "// cache block sizes" << std::endl;
        for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
            ss << stringf("#define K%d_%c_CB_SIZE_%s (%s)", kernel, level, upper_case(_input_buffer.name()),
                          concat(_macros.cb_size(V(kernel), V(level),_input_buffer.used_dimensions()), " * ")).c_str()
               << std::endl;
        }

        // erase last new line character
        return ss.str().erase(ss.str().length() - 1);
    }

    bool supports_caching() const {
        return !_input_buffer.result_input();
    }

    std::string caching_variable_declaration(unsigned int kernel, LEVEL level) const {
        return stringf(
                "%s TYPE_T cb_%c_%s%s;\n",
                level == LEVEL::LOCAL ? "__local" : "__private",
                lower_case(level),
                _input_buffer.name(),
                concat(wrap(multi_stringf("BUFFER_%s_%c_SIZE_%d", V(upper_case(_input_buffer.name())), V(level), uint_range(_input_buffer.used_dimensions().size() - 1, 0)), "[", "]"))
        );
    }

    std::string caching_copy_code(unsigned int kernel, LEVEL level, bool *dim_needs_guards) const {
        // TODO order WIs by OpenCL dimensions?
        auto indices = cb_processing_loop_variable(level, _input_buffer.used_dimensions());
        std::vector<std::string> descending_indices;
        std::string params;
        for (const auto &dim : dim_range(L_DIMS, R_DIMS)) {
            for (const auto &used_dim : _input_buffer.used_dimensions()) {
                if (dim.type == used_dim.type && dim.nr == used_dim.nr) {
                    params.append(stringf(
                            "%si_%c_elem_%c_%d",
                            params.empty() ? "" : ", ",
                            lower_case(level),
                            lower_case(dim.type),
                            dim.nr
                    ));
                }
            }
        }
        for (int nr = _input_buffer.used_dimensions().size() - 1; nr >= 0; --nr) {
            descending_indices.push_back(stringf("BUFFER_%s_INDEX_%d(%s)",
                                                 upper_case(_input_buffer.name()),
                                                 nr,
                                                 params));
        }
        auto filled_indices = cb_processing_loop_variable(level, _input_buffer.used_dimensions(), L_DIMS, R_DIMS);
        auto filled_offset_indices = multi_stringf("%s + %s", _macros.cb_offset(V(kernel),
                                                                                V(level),
                                                                                dim_range(L_DIMS, R_DIMS)),
                                                   filled_indices);
        std::vector<std::string> dim_sizes;
        for (const auto &dim : _input_buffer.used_dimensions()) {
            dim_sizes.push_back(cb_size<L_DIMS, R_DIMS>(kernel, level, dim, dim_needs_guards));
        }
        auto descending_dim_sizes = multi_stringf("BUFFER_%s_%c_SIZE_%d",
                                                  V(upper_case(_input_buffer.name())),
                                                  V(level),
                                                  uint_range(_input_buffer.used_dimensions().size() - 1, 0)
        );
        std::string cache_block_size;
        bool incomplete_used_dimension = false;
        for (const auto &dim : _input_buffer.used_dimensions()) {
            if (dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(level, dim)]) {
                cache_block_size += stringf(" / %s * %s",
                                            cb_size<L_DIMS, R_DIMS>(kernel, level, dim, nullptr),           // standard size
                                            cb_size<L_DIMS, R_DIMS>(kernel, level, dim, dim_needs_guards)); // reduced size
                incomplete_used_dimension = true;
            }
        }
        if (!cache_block_size.empty()) {
            cache_block_size = stringf("(K%d_%c_CB_SIZE_%s", kernel, level, upper_case(_input_buffer.name())) + cache_block_size + ")";
        } else {
            cache_block_size = stringf("K%d_%c_CB_SIZE_%s", kernel, level, upper_case(_input_buffer.name()));
        }

#ifdef RUNTIME_INPUT_SIZE
        if (incomplete_used_dimension) {
            return stringf(R"(for (size_t step = 0; step < %s / %s; ++step) {
  const size_t index = %s + step * %s;
%s
%s = %s;
}
if (%s < %s %% %s) {
  const size_t index = %s + (%s / %s) * %s;
%s
%s = %s;
}
)",
                           cache_block_size, _macros.flat_num_wi(kernel, level),
                           _macros.flat_wi_id(kernel, level), _macros.flat_num_wi(kernel, level),
                           level == LEVEL::LOCAL
                           ? indent(resolve_flat_index("index", indices, dim_sizes), 1)
                           : stringf(R"(  #if %s
%s
  #else
%s
  #endif)",
                                     concat(multi_stringf("CACHE_%c_CB != 0", level_range(LEVEL::LOCAL, parent_level(level))), " || "),
                                     indent(resolve_flat_index("index", descending_indices, descending_dim_sizes), 1),
                                     indent(resolve_flat_index("index", indices, dim_sizes), 1)),
                           indent(call_macro(kernel, level, filled_indices), 1),
                           call_macro(kernel, parent_level(level), filled_offset_indices),
                           _macros.flat_wi_id(kernel, level), cache_block_size, _macros.flat_num_wi(kernel, level),
                           _macros.flat_wi_id(kernel, level), cache_block_size, _macros.flat_num_wi(kernel, level),
                           _macros.flat_num_wi(kernel, level),
                           level == LEVEL::LOCAL
                           ? indent(resolve_flat_index("index", indices, dim_sizes), 1)
                           : stringf(R"(  #if %s
%s
  #else
%s
  #endif)",
                                     concat(multi_stringf("CACHE_%c_CB != 0", level_range(LEVEL::LOCAL, parent_level(level))), " || "),
                                     indent(resolve_flat_index("index", descending_indices, descending_dim_sizes), 1),
                                     indent(resolve_flat_index("index", indices, dim_sizes), 1)),
                           indent(call_macro(kernel, level, filled_indices), 1),
                           call_macro(kernel, parent_level(level), filled_offset_indices));
        } else {
#endif
            return stringf(R"(#if %s / %s > 0
for (size_t step = 0; step < %s / %s; ++step) {
  const size_t index = %s + step * %s;
%s
%s = %s;
}
#endif
#if %s %% %s > 0
if (%s < %s %% %s) {
  const size_t index = %s + (%s / %s) * %s;
%s
%s = %s;
}
#endif
)",
                           cache_block_size, _macros.flat_num_wi(kernel, level),
                           cache_block_size, _macros.flat_num_wi(kernel, level),
                           _macros.flat_wi_id(kernel, level), _macros.flat_num_wi(kernel, level),
                           level == LEVEL::LOCAL
                           ? indent(resolve_flat_index("index", indices, dim_sizes), 1)
                           : stringf(R"(  #if %s
%s
  #else
%s
  #endif)",
                                     concat(multi_stringf("CACHE_%c_CB != 0", level_range(LEVEL::LOCAL, parent_level(level))), " || "),
                                     indent(resolve_flat_index("index", descending_indices, descending_dim_sizes), 1),
                                     indent(resolve_flat_index("index", indices, dim_sizes), 1)),
                           indent(call_macro(kernel, level, filled_indices), 1),
                           call_macro(kernel, parent_level(level), filled_offset_indices),
                           cache_block_size, _macros.flat_num_wi(kernel, level),
                           _macros.flat_wi_id(kernel, level), cache_block_size, _macros.flat_num_wi(kernel, level),
                           _macros.flat_wi_id(kernel, level), cache_block_size, _macros.flat_num_wi(kernel, level),
                           _macros.flat_num_wi(kernel, level),
                           level == LEVEL::LOCAL
                           ? indent(resolve_flat_index("index", indices, dim_sizes), 1)
                           : stringf(R"(  #if %s
%s
  #else
%s
  #endif)",
                                     concat(multi_stringf("CACHE_%c_CB != 0", level_range(LEVEL::LOCAL, parent_level(level))), " || "),
                                     indent(resolve_flat_index("index", descending_indices, descending_dim_sizes), 1),
                                     indent(resolve_flat_index("index", indices, dim_sizes), 1)),
                           indent(call_macro(kernel, level, filled_indices), 1),
                           call_macro(kernel, parent_level(level), filled_offset_indices));
#ifdef RUNTIME_INPUT_SIZE
        }
#endif
    }

    std::string scalar_function_parameter_definition() const {
        return stringf("const TYPE_T %s_val", _input_buffer.name());
    }
    std::vector<std::string> scalar_function_parameter_values() const {
        return {call_macro(1, LEVEL::PRIVATE, cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, R_DIMS)))};
    }

    std::vector<dimension_t> used_dimensions() const {
        return _input_buffer.used_dimensions();
    }

private:
    const input_buffer_class &_input_buffer;
    const macros <L_DIMS, R_DIMS> &_macros;

    std::string _macro_name_pattern;
    std::string _macro_call_pattern;
    std::string _cb_size_pattern;
};

}
}

#endif //MD_BLAS_INPUT_BUFFER_WRAPPER_HPP
