//
// Created by   on 06.11.17.
//

#ifndef MD_BLAS_INPUT_STENCIL_BUFFER_WRAPPER_HPP
#define MD_BLAS_INPUT_STENCIL_BUFFER_WRAPPER_HPP

#include "input_wrapper.hpp"
#include "macros.hpp"
#include "types.hpp"

namespace md_hom {
namespace generator {


template<typename NT, unsigned int L_DIMS, unsigned int R_DIMS>
class input_stencil_buffer_wrapper : public input_wrapper {
public:
    input_stencil_buffer_wrapper(const input_stencil_buffer_class<NT> &input_stencil_buffer, const macros <L_DIMS, R_DIMS> &macros)
            : _input_stencil_buffer(input_stencil_buffer), _macros(macros), _scalar_function_parameter_values() {
        _macro_name_pattern = "K%d_%c_MEM_";
        _macro_name_pattern.append(upper_case(input_stencil_buffer.name()));

        _macro_call_pattern = _macro_name_pattern;
        _macro_call_pattern.append("(");
        for (int i = 0; i < L_DIMS + R_DIMS; ++i) {
            if (i > 0) _macro_call_pattern.append(", ");
            _macro_call_pattern.append("%s");
        }
        _macro_call_pattern.append(")");

        _cb_size_pattern = "K1_%c_CB_SIZE_";
        _cb_size_pattern.append(upper_case(input_stencil_buffer.name()));

        iterate_over_stencil_points([&] (const std::vector<int> &offsets) {
            if (!_scalar_function_parameter_definition.empty())
                _scalar_function_parameter_definition += ", ";
            std::vector<char> dim_types;
            std::vector<int> dim_nrs;
            std::vector<char> offset_signs;
            std::vector<int>  abs_offsets;
            for (int i = 0; i < offsets.size(); ++i) {
                if (offsets[i] < 0) {
                    dim_types.push_back(lower_case(_input_stencil_buffer.used_dimensions(true)[i].type));
                    dim_nrs.push_back(_input_stencil_buffer.used_dimensions(true)[i].nr);
                    offset_signs.push_back('m');
                    abs_offsets.push_back(-(offsets[i]));
                } else if (offsets[i] > 0) {
                    dim_types.push_back(lower_case(_input_stencil_buffer.used_dimensions(true)[i].type));
                    dim_nrs.push_back(_input_stencil_buffer.used_dimensions(true)[i].nr);
                    offset_signs.push_back('p');
                    abs_offsets.push_back(offsets[i]);
                }
            }
            _scalar_function_parameter_definition +=
                    stringf("const TYPE_T %s_val%s",
                            _input_stencil_buffer.name(),
                            concat(multi_stringf("_%c%d_%c%d",
                                                 dim_types,
                                                 dim_nrs,
                                                 offset_signs,
                                                 abs_offsets)));
            _scalar_function_parameter_values.push_back(
                    call_macro(1, LEVEL::PRIVATE,
                               multi_stringf("%s + %d",
                                             cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, R_DIMS)),
                                             offsets)));
        }, true);
    }

    std::string input_name() const {
        return _input_stencil_buffer.name();
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
                      upper_case(_input_stencil_buffer.name())).c_str() << std::endl;
        ss << std::endl;
        ss << "// buffer abstraction" << std::endl;
        ss << descending_ocl_dimension_order<L_DIMS, R_DIMS>(
                _input_stencil_buffer.used_dimensions(),
                [&](dimension_t dim, unsigned int order_nr) -> std::string {
                    std::string inner_code = stringf(
                            "#define BUFFER_%s_INDEX_%d(%s) %s",
                            upper_case(_input_stencil_buffer.name()), order_nr,
                            make_standard_params_retain_order<L_DIMS, R_DIMS>(_input_stencil_buffer.used_dimensions()),
                            make_standard_indices(V(dim), L_DIMS, R_DIMS).front());
                    for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                        if (level != LEVEL::GLOBAL) {
                            inner_code.append(stringf(
                                    "\n#define BUFFER_%s_%c_SIZE_%d (%d + %s + %d)",
                                    upper_case(_input_stencil_buffer.name()), level, order_nr,
                                    _input_stencil_buffer.neighborhood_dimension_size(dim).first,
                                    _macros.cb_size(kernel, level, dim),
                                    _input_stencil_buffer.neighborhood_dimension_size(dim).second));
                        } else {
                            inner_code.append(stringf(
                                    "\n#define BUFFER_%s_%c_SIZE_%d %s",
                                    upper_case(_input_stencil_buffer.name()), level, order_nr,
                                    kernel == 2 && dim.type == DIM_TYPE::R ? stringf("NUM_WG_R_%d", dim.nr) : _macros.cb_size(kernel, level, dim)));
                        }
                    }
                    return inner_code;
                }).c_str() << std::endl;
        for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
            ss << "#define "
               << stringf("K%d_%c_BUFFER_%s(%s)", kernel, level, upper_case(_input_stencil_buffer.name()),
                          make_standard_params_retain_order<L_DIMS, R_DIMS>(_input_stencil_buffer.used_dimensions()))
               << " ";

            if (level == LEVEL::GLOBAL) {
                switch (_input_stencil_buffer.oob_strategy()) {
                    case NONE:
                        ss << stringf("%s[%s]",
                                      _input_stencil_buffer.name(),
                                      make_flat_index(
                                              multi_stringf("(%d + %s)",
                                                            _input_stencil_buffer.neighborhood_dimension_front_sizes(),
                                                            make_standard_indices(_input_stencil_buffer.used_dimensions(), L_DIMS, R_DIMS)),
                                              multi_stringf("(%d + %s + %d)",
                                                            _input_stencil_buffer.neighborhood_dimension_front_sizes(),
                                                            _macros.cb_size(V(kernel), V(LEVEL::GLOBAL), _input_stencil_buffer.used_dimensions()),
                                                            _input_stencil_buffer.neighborhood_dimension_back_sizes())
                                      )
                        );
                        break;
                    case ZERO:
                        ss << stringf("((%s) ? 0 : %s%s)",
                                      concat(multi_stringf("%s < 0 || %s >= %s",
                                                           make_standard_indices(_input_stencil_buffer.used_dimensions(), L_DIMS, R_DIMS),
                                                           make_standard_indices(_input_stencil_buffer.used_dimensions(), L_DIMS, R_DIMS),
                                                           _macros.cb_size(V(kernel), V(level), _input_stencil_buffer.used_dimensions())),
                                             " || "),
                                      _input_stencil_buffer.name(),
                                      wrap({make_flat_index(make_standard_indices(_input_stencil_buffer.used_dimensions(), L_DIMS, R_DIMS),
                                                            _macros.cb_size(V(kernel), V(LEVEL::GLOBAL), _input_stencil_buffer.used_dimensions()))},
                                           "[", "]")[0]);
                        break;
                }
            } else {
                ss << stringf("cb_%c_%s", lower_case(level), _input_stencil_buffer.name());
                ss << stringf(concat(wrap(multi_stringf("BUFFER_%s_INDEX_%d(%s)",
                                                        V(upper_case(_input_stencil_buffer.name())),
                                                        uint_range(_input_stencil_buffer.used_dimensions().size() - 1, 0),
                                                        V(concat(multi_stringf("%s + %d",
                                                                               make_standard_indices_retain_order<L_DIMS, R_DIMS>(_input_stencil_buffer.used_dimensions()),
                                                                               _input_stencil_buffer.neighborhood_dimension_front_sizes(true)),
                                                                 ", "))
                ), "[(", ")]")));
            }

            ss << std::endl;
        }
        ss << std::endl;
        ss << "// partitioning and cache usage" << std::endl;
        for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
            if (level != LEVEL::GLOBAL) {
#ifdef INDIVIDUAL_INPUT_CACHING
                ss << stringf("#if %s_CACHE_%c_CB != 0", upper_case(input_name()), level).c_str() << std::endl;
#else
                ss << stringf("#if CACHE_%c_CB != 0", level).c_str() << std::endl;
#endif
            }
            ss << stringf("#define K%d_%c_MEM_%s(%s) ", kernel, level, upper_case(_input_stencil_buffer.name()),
                          make_standard_params(L_DIMS + R_DIMS)).c_str()
               << stringf("K%d_%c_BUFFER_%s(%s)", kernel, level, upper_case(_input_stencil_buffer.name()),
                          make_standard_params_retain_order<L_DIMS, R_DIMS>(_input_stencil_buffer.used_dimensions()))
               << std::endl;
            if (level != LEVEL::GLOBAL) {
                ss << "#else" << std::endl;
                auto complete_dim_range = dim_range(L_DIMS, R_DIMS);
                ss << stringf("#define K%d_%c_MEM_%s(%s) ", kernel, level, upper_case(_input_stencil_buffer.name()),
                              make_standard_params(L_DIMS + R_DIMS)).c_str()
                   << stringf("K%d_%c_MEM_%s(%s)", kernel, parent_level(level), upper_case(_input_stencil_buffer.name()),
                              make_standard_params(L_DIMS + R_DIMS, "K%d_%c_CB_OFFSET_%c_%d + (%%val%%)",
                                                   V(kernel),
                                                   V(level),
                                                   split_dim_range(complete_dim_range).first,
                                                   split_dim_range(complete_dim_range).second))
                   << std::endl;
                ss << "#endif" << std::endl;
            }
        }
        ss << std::endl;
        ss << "// cache block sizes" << std::endl;
        for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
            if (level == LEVEL::GLOBAL) {
                ss << stringf("#define K%d_%c_CB_SIZE_%s (%s)", kernel, level, upper_case(_input_stencil_buffer.name()),
                              concat(_macros.cb_size(V(kernel), V(level), _input_stencil_buffer.used_dimensions()), " * ")).c_str()
                   << std::endl;
            } else {
                ss << stringf("#define K%d_%c_CB_SIZE_%s (%s)", kernel, level, upper_case(_input_stencil_buffer.name()),
                              concat(multi_stringf("(%d + %s + %d)",
                                                   _input_stencil_buffer.neighborhood_dimension_front_sizes(),
                                                   _macros.cb_size(V(kernel), V(level), _input_stencil_buffer.used_dimensions()),
                                                   _input_stencil_buffer.neighborhood_dimension_back_sizes()
                              ), " * ")).c_str()
                   << std::endl;
            }
        }

        // erase last new line character
        return ss.str().erase(ss.str().length() - 1);
    }

    bool supports_caching() const {
        return true;
    }

    std::string caching_variable_declaration(unsigned int kernel, LEVEL level) const {
        return stringf(
                "%s TYPE_T cb_%c_%s%s;\n",
                level == LEVEL::LOCAL ? "__local" : "__private",
                lower_case(level),
                _input_stencil_buffer.name(),
                concat(wrap(multi_stringf("BUFFER_%s_%c_SIZE_%d", V(upper_case(_input_stencil_buffer.name())), V(level), uint_range(_input_stencil_buffer.used_dimensions().size() - 1, 0)), "[", "]"))
        );
    }

    std::string caching_copy_code(unsigned int kernel, LEVEL level, bool *dim_needs_guards) const {
        // TODO order WIs by OpenCL dimensions?
        auto indices = cb_processing_loop_variable(level, _input_stencil_buffer.used_dimensions());
        auto filled_indices = multi_stringf("%s - %d",
                                            cb_processing_loop_variable(level, _input_stencil_buffer.used_dimensions(true), L_DIMS, R_DIMS),
                                            _input_stencil_buffer.neighborhood_dimension_front_sizes(true));
        auto filled_offset_indices = multi_stringf("%s + %s", _macros.cb_offset(V(kernel),
                                                                                V(level),
                                                                                dim_range(L_DIMS, R_DIMS)),
                                                   filled_indices);
        std::vector<std::string> dim_sizes = multi_stringf(
                "(%d + %s + %d)",
                _input_stencil_buffer.neighborhood_dimension_front_sizes(),
                _macros.cb_size(V(kernel), V(level), _input_stencil_buffer.used_dimensions(), V(dim_needs_guards)),
                _input_stencil_buffer.neighborhood_dimension_back_sizes());
        std::string cache_block_size;
        bool incomplete_used_dimension = false;
        for (const auto &dim : _input_stencil_buffer.used_dimensions()) {
            if (dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(level, dim)]) {
                cache_block_size += stringf(" / %s * %s",
                                            stringf("(%d + %s + %d)", _input_stencil_buffer.neighborhood_dimension_size(dim).first, _macros.cb_size(kernel, level, dim, nullptr),          _input_stencil_buffer.neighborhood_dimension_size(dim).second),  // standard size
                                            stringf("(%d + %s + %d)", _input_stencil_buffer.neighborhood_dimension_size(dim).first, _macros.cb_size(kernel, level, dim, dim_needs_guards), _input_stencil_buffer.neighborhood_dimension_size(dim).second)); // reduced size
                incomplete_used_dimension = true;
            }
        }
        if (!cache_block_size.empty()) {
            cache_block_size = stringf("(K%d_%c_CB_SIZE_%s", kernel, level, upper_case(_input_stencil_buffer.name())) + cache_block_size + ")";
        } else {
            cache_block_size = stringf("K%d_%c_CB_SIZE_%s", kernel, level, upper_case(_input_stencil_buffer.name()));
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
                           indent(resolve_flat_index("index", indices, dim_sizes), 1), // TODO only resolve index if buffer has more than 1 dimension
                           indent(call_macro(kernel, level, filled_indices), 1),
                           call_macro(kernel, parent_level(level), filled_offset_indices),
                           _macros.flat_wi_id(kernel, level), cache_block_size, _macros.flat_num_wi(kernel, level),
                           _macros.flat_wi_id(kernel, level), cache_block_size, _macros.flat_num_wi(kernel, level), _macros.flat_num_wi(kernel, level),
                           indent(resolve_flat_index("index", indices, dim_sizes), 1), // TODO only resolve index if buffer has more than 1 dimension
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
                           indent(resolve_flat_index("index", indices, dim_sizes), 1), // TODO only resolve index if buffer has more than 1 dimension
                           indent(call_macro(kernel, level, filled_indices), 1),
                           call_macro(kernel, parent_level(level), filled_offset_indices),
                           cache_block_size, _macros.flat_num_wi(kernel, level),
                           _macros.flat_wi_id(kernel, level), cache_block_size, _macros.flat_num_wi(kernel, level),
                           _macros.flat_wi_id(kernel, level), cache_block_size, _macros.flat_num_wi(kernel, level), _macros.flat_num_wi(kernel, level),
                           indent(resolve_flat_index("index", indices, dim_sizes), 1), // TODO only resolve index if buffer has more than 1 dimension
                           indent(call_macro(kernel, level, filled_indices), 1),
                           call_macro(kernel, parent_level(level), filled_offset_indices));
#ifdef RUNTIME_INPUT_SIZE
        }
#endif
    }

    std::string scalar_function_parameter_definition() const {
        return _scalar_function_parameter_definition;
    }
    std::vector<std::string> scalar_function_parameter_values() const {
        return _scalar_function_parameter_values;
    }

    void iterate_over_stencil_points(const std::function<void(const std::vector<int> &)> &callback, bool sort_offsets = false) {
        std::vector<int> offsets(_input_stencil_buffer.neighborhood_dimensionality());
        iterate_over_stencil_points_impl(0, offsets, _input_stencil_buffer.neighborhood(), callback, sort_offsets);
    }

    std::vector<dimension_t> used_dimensions() const {
        return _input_stencil_buffer.used_dimensions();
    }

private:
    const input_stencil_buffer_class<NT> &_input_stencil_buffer;
    const macros <L_DIMS, R_DIMS> &_macros;

    std::string _macro_name_pattern;
    std::string _macro_call_pattern;
    std::string _cb_size_pattern;
    std::string _scalar_function_parameter_definition;
    std::vector<std::string> _scalar_function_parameter_values;

    template <typename T>
    void iterate_over_stencil_points_impl(size_t dimension, std::vector<int> &offsets,
                                          const std::vector<T> &neighborhood,
                                          const std::function<void(const std::vector<int> &)> &callback,
                                          bool sort_offsets) {
        for (int n = 0; n < neighborhood.size(); ++n) {
            int offset_index;
            if (sort_offsets) {
                offset_index = 0;
                for (const auto &dim : _input_stencil_buffer.used_dimensions(true)) {
                    if (dim.type == _input_stencil_buffer.used_dimensions()[dimension].type
                            && dim.nr == _input_stencil_buffer.used_dimensions()[dimension].nr) {
                        break;
                    }
                    ++offset_index;
                }
            } else {
                offset_index = dimension;
            }
            offsets[offset_index] = n - neighborhood.size() / 2;
            iterate_over_stencil_points_impl(dimension + 1, offsets, neighborhood[n], callback, sort_offsets);
        }
    }
    void iterate_over_stencil_points_impl(size_t dimension, std::vector<int> &offsets,
                                          int neighbor,
                                          const std::function<void(const std::vector<int> &)> &callback,
                                          bool sort_offsets) {
        if (neighbor != 0) callback(offsets);
    }
};

}
}

#endif //MD_BLAS_INPUT_STENCIL_BUFFER_WRAPPER_HPP
