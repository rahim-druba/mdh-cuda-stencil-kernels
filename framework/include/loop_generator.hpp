//
// Created by   on 02.11.17.
//

#ifndef MD_BLAS_LOOP_GENERATOR_HPP
#define MD_BLAS_LOOP_GENERATOR_HPP

#include <string>
#include <tuple>
#include <functional>
#include <utility>
#include <iostream>

#include "macros.hpp"
#include "types.hpp"
#include "helper.hpp"

namespace md_hom {
namespace generator {

enum POSITIONING {
    PREPEND,
    INNER_PREPEND,
    INNER_APPEND,
    APPEND
};
enum FILTER_VALUE {
    FIRST,
    TOTAL_FIRST,
    LAST,
    TOTAL_LAST,
    ANY
};
template<unsigned int L_DIMS, unsigned int R_DIMS>
class loop_generator {
public:
    typedef std::function<std::string(LEVEL, const dimension_t&, bool*, bool*)> generator;
    typedef struct {
        FILTER_VALUE level_filter;
        FILTER_VALUE dimension_type_filter;
        FILTER_VALUE dimension_nr_filter;
    } filter_t;

    loop_generator(const macros<L_DIMS, R_DIMS> &macros, const std::vector<LEVEL> &level,
                   const std::vector<dimension_t> &dimensions)
            : _macros(macros), _level(level), _dimensions(dimensions) {
    }

    void add_code(const filter_t &filter, POSITIONING positioning, const generator &code_gen) {
        switch (positioning) {
            case PREPEND:
                _additional_code_prepend.emplace_back(filter, code_gen);
                break;
            case INNER_PREPEND:
                _additional_code_inner_prepend.emplace_back(filter, code_gen);
                break;
            case INNER_APPEND:
                _additional_code_inner_append.emplace_back(filter, code_gen);
                break;
            case APPEND:
                _additional_code_append.emplace_back(filter, code_gen);
                break;
        }
    }

    std::string from(unsigned int kernel, LEVEL start_level, const dimension_t &start_dimension,
                     const generator &body,
                     bool parentheses = true, bool *dim_needs_guards = nullptr) {
        unsigned int level_nr = 0;
        unsigned int dimension_nr = 0;
        for (unsigned int i = 0; i < _level.size(); ++i) {
            if (start_level == _level[i]) {
                level_nr = i;
            }
        }
        for (unsigned int i = 0; i < _dimensions.size(); ++i) {
            if (start_dimension == _dimensions[i]) {
                dimension_nr = i;
            }
        }
        bool dng[3 * (L_DIMS + R_DIMS)];
        for (int i = 0; i < 3 * (L_DIMS + R_DIMS); ++i) {
            dng[i] = dim_needs_guards != nullptr && dim_needs_guards[i];
        }
        bool fi[3];
        fi[2] = true; // GLOBAL is always in first iteration
        for (int i = 0; i < 2; ++i) {
            fi[i] = false;
        }
        return from_impl(kernel, level_nr, dimension_nr, body, parentheses, dng, fi);
    }

    static std::string loop_variable(LEVEL level, const dimension_t &dimension) {
        return stringf("i_%c_cb_%c_%d", lower_case(level), lower_case(dimension.type), dimension.nr);
    }

    static std::vector<std::string> loop_variable(const std::vector<LEVEL> &level, const std::vector<dimension_t> &dimension) {
        return multi_stringf("i_%c_cb_%c_%d",
                             lower_case(level),
                             lower_case(split_dim_range(dimension).first),
                             split_dim_range(dimension).second);
    }

private:
    typedef std::vector<std::pair<filter_t, generator>> generator_vector;

    const macros<L_DIMS, R_DIMS>  &_macros;
    const std::vector<LEVEL>       _level;
    const std::vector<dimension_t> _dimensions;
    generator_vector               _additional_code_prepend;
    generator_vector               _additional_code_inner_prepend;
    generator_vector               _additional_code_inner_append;
    generator_vector               _additional_code_append;

    // helper for matching a filter struct with actual levels and dimensions
    bool matches(const filter_t &filter, LEVEL level, const dimension_t & dimension) {
        return (filter.level_filter == ANY
                || (filter.level_filter == FIRST && level == _level.front())
                || (filter.level_filter == LAST && level == _level.back()))
               && (filter.dimension_type_filter == ANY
                   || (filter.dimension_type_filter == FIRST && dimension.type == DIM_TYPE::L)
                   || (filter.dimension_type_filter == LAST && dimension.type == DIM_TYPE::R))
               && (filter.dimension_nr_filter == ANY
                   || (filter.dimension_nr_filter == FIRST && dimension.nr == _dimensions.front().nr)
                   || (filter.dimension_nr_filter == LAST && dimension.nr == _dimensions.back().nr)
                   || (filter.dimension_nr_filter == TOTAL_FIRST && CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dimension) == CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(_dimensions.front()))
                   || (filter.dimension_nr_filter == TOTAL_LAST && CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dimension) == CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(_dimensions.back())));
    }

    std::string from_impl(unsigned int kernel, unsigned int level_nr, unsigned int dimension_nr,
                          const generator &body, bool parentheses, bool *dim_needs_guards, bool *first_iteration) {
        std::string result;
        const auto& level = _level[level_nr];
        const auto& dimension = _dimensions[dimension_nr];

        bool first_iteration_adapted[3];
        for (int i = 0; i < 3; ++i) {
            first_iteration_adapted[i] = first_iteration[i];
        }
        if (dimension.type == DIM_TYPE::R) {
            first_iteration_adapted[LEVEL_ID(level)] = true;
        }

        for (const auto &code_gen : _additional_code_prepend) {
            if (matches(code_gen.first, level, dimension)) {
                result.append(code_gen.second(level, dimension, dim_needs_guards, first_iteration));
                result.append("\n\n");
            }
        }

        std::string loop_body_first;
        std::string loop_body_rest;
        for (const auto &code_gen : _additional_code_inner_prepend) {
            if (matches(code_gen.first, level, dimension)) {
                if (dimension.type == DIM_TYPE::R) {
                    loop_body_first.append(code_gen.second(level, dimension, dim_needs_guards, first_iteration_adapted));
                    loop_body_first.append("\n\n");
                }
                loop_body_rest.append(code_gen.second(level, dimension, dim_needs_guards, first_iteration));
                loop_body_rest.append("\n\n");
            }
        }
        unsigned int sub_level_nr = level_nr;
        unsigned int sub_dimension_nr = dimension_nr;
        if (level_nr == _level.size() - 1 && dimension_nr == _dimensions.size() - 1) {
            // reached end of hierarchy
#ifdef ADD_CB_INFO_COMMENTS
            for (const auto& dim : _dimensions) {
                for (const auto& l : _level) {
                    loop_body.append(stringf("// %c_cb_%c_%d: %s\n",
                                             l, dim.type, dim.nr,
                                             dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(l, dim)] ? "incomplete" : "complete"));
                }
            }
#endif
            if (dimension.type == DIM_TYPE::R) {
                loop_body_first.append(body(level, dimension, dim_needs_guards, first_iteration_adapted));
            }
            loop_body_rest.append(body(level, dimension, dim_needs_guards, first_iteration));
        } else {
            if (dimension_nr == _dimensions.size() - 1) {
                ++sub_level_nr;
                sub_dimension_nr = 0;
            } else {
                ++sub_dimension_nr;
            }
            if (dimension.type == DIM_TYPE::R) {
                loop_body_first.append(from_impl(kernel, sub_level_nr, sub_dimension_nr, body, parentheses, dim_needs_guards, first_iteration_adapted));
            }
            loop_body_rest.append(from_impl(kernel, sub_level_nr, sub_dimension_nr, body, parentheses, dim_needs_guards, first_iteration));
        }
        for (const auto &code_gen : _additional_code_inner_append) {
            if (matches(code_gen.first, level, dimension)) {
                if (dimension.type == DIM_TYPE::R) {
                    loop_body_first.append("\n\n");
                    loop_body_first.append(code_gen.second(level, dimension, dim_needs_guards, first_iteration_adapted));
                }
                loop_body_rest.append("\n\n");
                loop_body_rest.append(code_gen.second(level, dimension, dim_needs_guards, first_iteration));
            }
        }

        // process regular cache blocks
#ifdef RUNTIME_INPUT_SIZE
        if (level == LEVEL::LOCAL || dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(parent_level(level), dimension)]) {
            if (dimension.type == DIM_TYPE::R) {
                result.append(stringf("if (%s > 0) {\n", _macros.num_cb(kernel, level, dimension, dim_needs_guards)));
                result.append(indent(stringf("size_t %s = 0;\n", loop_variable(level, dimension)), 1));
                result.append(indent(loop_body_first, 1));
                result.append(indent(stringf("\nfor (%s = 1; %s < %s; ++%s)%s%s",
                                      loop_variable(level, dimension), loop_variable(level, dimension),
                                      _macros.num_cb(kernel, level, dimension, dim_needs_guards),
                                      loop_variable(level, dimension),
                                      parentheses ? " {" : "",
                                      loop_body_rest.empty() && level_nr * _dimensions.size() + dimension_nr ==
                                                                (_level.size() * _dimensions.size() - 1) ? "" : "\n"), 1));
                result.append(indent(loop_body_rest, 2));
                if (parentheses) {
                    result.append(indent(stringf("\n} // end of \"%s\"-loop", loop_variable(level, dimension)), 1));
                }
                result.append("\n}");
            } else {
                result.append(stringf("for (size_t %s = 0; %s < %s; ++%s)%s%s",
                                      loop_variable(level, dimension), loop_variable(level, dimension),
                                      _macros.num_cb(kernel, level, dimension, dim_needs_guards),
                                      loop_variable(level, dimension),
                                      parentheses ? " {" : "",
                                      loop_body_rest.empty() && level_nr * _dimensions.size() + dimension_nr ==
                                                                (_level.size() * _dimensions.size() - 1) ? "" : "\n"));
                result.append(indent(loop_body_rest, 1));
                if (parentheses) {
                    result.append(stringf("\n} // end of \"%s\"-loop", loop_variable(level, dimension)));
                }
            }
        } else {
#endif
            result.append(stringf("#if %s > 0\n", _macros.num_cb(kernel, level, dimension, dim_needs_guards)));
            if (dimension.type == DIM_TYPE::R) {
                result.append(stringf("size_t %s = 0;\n", loop_variable(level, dimension)));
                result.append(loop_body_first);
                result.append(stringf("\n#if %s > 1\n", _macros.num_cb(kernel, level, dimension, dim_needs_guards)));
                result.append(stringf("for (%s = 1; %s < %s; ++%s)%s%s",
                                      loop_variable(level, dimension), loop_variable(level, dimension),
                                      _macros.num_cb(kernel, level, dimension, dim_needs_guards),
                                      loop_variable(level, dimension),
                                      parentheses ? " {" : "",
                                      loop_body_rest.empty() && level_nr * _dimensions.size() + dimension_nr ==
                                                                (_level.size() * _dimensions.size() - 1) ? "" : "\n"));
                result.append(indent(loop_body_rest, 1));
                if (parentheses) {
                    result.append(stringf("\n} // end of \"%s\"-loop", loop_variable(level, dimension)));
                }
                result.append("\n#endif");
            } else {
                result.append(stringf("for (size_t %s = 0; %s < %s; ++%s)%s%s",
                                      loop_variable(level, dimension), loop_variable(level, dimension),
                                      _macros.num_cb(kernel, level, dimension, dim_needs_guards),
                                      loop_variable(level, dimension),
                                      parentheses ? " {" : "",
                                      loop_body_rest.empty() && level_nr * _dimensions.size() + dimension_nr ==
                                                                (_level.size() * _dimensions.size() - 1) ? "" : "\n"));
                result.append(indent(loop_body_rest, 1));
                if (parentheses) {
                    result.append(stringf("\n} // end of \"%s\"-loop", loop_variable(level, dimension)));
                }
            }
            result.append("\n#endif");
#ifdef RUNTIME_INPUT_SIZE
        }
#endif

        // post process whole extra cache blocks
#ifdef POST_PROCESSING_1
        result.append(stringf("\n// post process whole extra cache blocks in dimension %c_%d", dimension.type, dimension.nr));
#ifdef RUNTIME_INPUT_SIZE
        if (level == LEVEL::LOCAL || dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(parent_level(level), dimension)]) {
            result.append(stringf("\nif (%s < %s) {", _macros.fu_id(kernel, parent_level(level), dimension), _macros.num_extra_cb(kernel, level, dimension, dim_needs_guards)));
            result.append(indent(stringf("\nconst size_t %s = %s;\n",
                                         loop_generator<L_DIMS, R_DIMS>::loop_variable(level, dimension),
                                         _macros.num_cb(kernel, level, dimension, dim_needs_guards)), 1));
            if (dimension.type == DIM_TYPE::R) {
                result.append(indent(stringf("\nif (%s == 0) {\n", _macros.num_cb(kernel, level, dimension, dim_needs_guards)), 1));
                result.append(indent(loop_body_first, 2));
                result.append(indent("\n} else {\n", 1));
                result.append(indent(loop_body_rest, 2));
                result.append(indent("\n}", 1));
            } else {
                result.append(indent(loop_body_rest, 1));
            }
            result.append(stringf("\n} // end of post processing whole extra cache blocks in dimension %c_%d", dimension.type, dimension.nr));
        } else {
#endif
            result.append(stringf("\n#if %s > 0", _macros.num_extra_cb(kernel, level, dimension, dim_needs_guards)));
            result.append(stringf("\nif (%s < %s) {", _macros.fu_id(kernel, parent_level(level), dimension), _macros.num_extra_cb(kernel, level, dimension, dim_needs_guards)));
            result.append(indent(stringf("\nconst size_t %s = %s;\n",
                                         loop_generator<L_DIMS, R_DIMS>::loop_variable(level, dimension),
                                         _macros.num_cb(kernel, level, dimension, dim_needs_guards)), 1));
            if (dimension.type == DIM_TYPE::R) {
                result.append(indent(stringf("\n#if %s == 0\n", _macros.num_cb(kernel, level, dimension, dim_needs_guards)), 1));
                result.append(indent(loop_body_first, 1));
                result.append(indent("\n#else\n", 1));
            }
            result.append(indent(loop_body_rest, 1));
            if (dimension.type == DIM_TYPE::R) {
                result.append(indent("\n#endif", 1));
            }
            result.append(stringf("\n} // end of post processing whole extra cache blocks in dimension %c_%d", dimension.type, dimension.nr));
            result.append("\n#endif");
#ifdef RUNTIME_INPUT_SIZE
        }
#endif
#endif

        // post process single extra incomplete cache block
#ifdef POST_PROCESSING_2
        std::string pp2_body_first;
        std::string pp2_body_rest;
        dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(level, dimension)] = true;
        for (const auto &code_gen : _additional_code_inner_prepend) {
            if (matches(code_gen.first, level, dimension)) {
                if (dimension.type == DIM_TYPE::R) {
                    pp2_body_first.append(code_gen.second(level, dimension, dim_needs_guards, first_iteration_adapted));
                    pp2_body_first.append("\n\n");
                }
                pp2_body_rest.append(code_gen.second(level, dimension, dim_needs_guards, first_iteration));
                pp2_body_rest.append("\n\n");
            }
        }
        if (level_nr == _level.size() - 1 && dimension_nr == _dimensions.size() - 1) {
            // reached end of hierarchy
#ifdef ADD_CB_INFO_COMMENTS
            for (const auto& dim : _dimensions) {
                for (const auto& l : _level) {
                    loop_body.append(stringf("// %c_cb_%c_%d: %s\n",
                                             l, dim.type, dim.nr,
                                             dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(l, dim)] ? "incomplete" : "complete"));
                }
            }
#endif
            if (dimension.type == DIM_TYPE::R) {
                pp2_body_first.append(body(level, dimension, dim_needs_guards, first_iteration_adapted));
            }
            pp2_body_rest.append(body(level, dimension, dim_needs_guards, first_iteration));
        } else {
            if (dimension.type == DIM_TYPE::R) {
                pp2_body_first.append(from_impl(kernel, sub_level_nr, sub_dimension_nr, body, parentheses, dim_needs_guards, first_iteration_adapted));
            }
            pp2_body_rest.append(from_impl(kernel, sub_level_nr, sub_dimension_nr, body, parentheses, dim_needs_guards, first_iteration));
        }
        for (const auto &code_gen : _additional_code_inner_append) {
            if (matches(code_gen.first, level, dimension)) {
                if (dimension.type == DIM_TYPE::R) {
                    pp2_body_first.append("\n\n");
                    pp2_body_first.append(code_gen.second(level, dimension, dim_needs_guards, first_iteration_adapted));
                }
                pp2_body_rest.append("\n\n");
                pp2_body_rest.append(code_gen.second(level, dimension, dim_needs_guards, first_iteration));
            }
        }
        dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(level, dimension)] = false;

        result.append(stringf("\n// post process single extra incomplete cache block in dimension %c_%d", dimension.type, dimension.nr));
#ifdef RUNTIME_INPUT_SIZE
        if (level == LEVEL::LOCAL || dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(parent_level(level), dimension)]) {
            result.append(stringf("\nif (%s > 0 && (%s == %s %% %s)) {",
                                  _macros.num_extra_elems(kernel, level, dimension, dim_needs_guards),
                                  _macros.fu_id(kernel, parent_level(level), dimension),
                                  _macros.num_extra_cb(kernel, level, dimension, dim_needs_guards),
                                  _macros.num_fu(kernel, parent_level(level), dimension)));
            result.append(indent(stringf("\nconst size_t %s = %s;\n",
                                         loop_generator<L_DIMS, R_DIMS>::loop_variable(level, dimension),
                                         _macros.num_cb(kernel, level, dimension, dim_needs_guards)), 1));
            if (dimension.type == DIM_TYPE::R) {
                result.append(indent(stringf("\nif (%s == 0) {\n", _macros.num_cb(kernel, level, dimension, dim_needs_guards)), 1));
                result.append(indent(pp2_body_first, 2));
                result.append(indent("\n} else {\n", 1));
                result.append(indent(pp2_body_rest, 2));
                result.append(indent("\n}", 1));
            } else {
                result.append(indent(pp2_body_rest, 1));
            }
            result.append(stringf("\n}// end of post process single extra incomplete cache block in dimension %c_%d", dimension.type, dimension.nr));
        } else {
#endif
            result.append(stringf("\n#if %s > 0", _macros.num_extra_elems(kernel, level, dimension, dim_needs_guards)));
            result.append(stringf("\nif (%s == %s %% %s) {",
                                  _macros.fu_id(kernel, parent_level(level), dimension),
                                  _macros.num_extra_cb(kernel, level, dimension, dim_needs_guards),
                                  _macros.num_fu(kernel, parent_level(level), dimension)));
            result.append(indent(stringf("\nconst size_t %s = %s;\n",
                                         loop_generator<L_DIMS, R_DIMS>::loop_variable(level, dimension),
                                         _macros.num_cb(kernel, level, dimension, dim_needs_guards)), 1));
            if (dimension.type == DIM_TYPE::R) {
                result.append(
                        indent(stringf("\n#if %s == 0\n", _macros.num_cb(kernel, level, dimension, dim_needs_guards)), 1));
                result.append(indent(pp2_body_first, 1));
                result.append(indent("\n#else\n", 1));
            }
            result.append(indent(pp2_body_rest, 1));
            if (dimension.type == DIM_TYPE::R) {
                result.append(indent("\n#endif", 1));
            }
            result.append(stringf("\n}// end of post process single extra incomplete cache block in dimension %c_%d", dimension.type, dimension.nr));
            result.append("\n#endif");
#ifdef RUNTIME_INPUT_SIZE
        }
#endif
#endif

        for (const auto &code_gen : _additional_code_append) {
            if (matches(code_gen.first, level, dimension)) {
                result.append("\n\n");
                result.append(code_gen.second(level, dimension, dim_needs_guards, first_iteration));
            }
        }
        return result;
    }
};

}
}

#endif //MD_BLAS_LOOP_GENERATOR_HPP
