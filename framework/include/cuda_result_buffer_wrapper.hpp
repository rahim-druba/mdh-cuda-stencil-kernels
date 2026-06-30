//
// Created by   on 06.11.17.
//

#ifndef MDH_CUDA_RESULT_BUFFER_WRAPPER_HPP
#define MDH_CUDA_RESULT_BUFFER_WRAPPER_HPP

#include <sstream>
#include <iostream>

#include "result_buffer.hpp"
#include "macros.hpp"
#include "types.hpp"

namespace md_hom {
namespace generator {


template<unsigned int L_DIMS, unsigned int R_DIMS>
class cuda_result_buffer_wrapper {
public:
    cuda_result_buffer_wrapper(const result_buffer &result_buffer, const macros<L_DIMS, R_DIMS> &macros,
                          const std::vector<std::unique_ptr<input_wrapper>> &input_wrapper,
                          const bool apply_result_function, const bool keep_both_results)
            : _result_buffer(result_buffer), _macros(macros), _input_wrapper(input_wrapper),
              _apply_result_function(apply_result_function), _keep_both_results(keep_both_results) {
        _buffer_macro_name_pattern = "K%d_RES_%c_BUFFER";
        _buffer_def_macro_name_pattern = "K%d_RES_%c_BUFFER_DEF";
        _res_macro_name_pattern = "K%d_%c_CB_RES_DEST";
        _reduction_mem_macro_name_pattern = "K%d_%c_REDUCTION_MEM";
        _kernel_res_macro_name_pattern = "K%d_%c_KERNEL_RES";
        _kernel_res_orig_macro_name_pattern = "K%d_%c_KERNEL_RES_ORIG";

        for (LEVEL level : {LEVEL::PRIVATE, LEVEL::LOCAL, LEVEL::GLOBAL}) {
            std::vector<std::string> params;
            for (int i = 0; i < L_DIMS + sub_levels(level, true, ORDER::DESCENDING).size() * R_DIMS; ++i) {
                params.push_back("%s");
            }
            _buffer_macro_call_pattern[LEVEL_ID(level)] = stringf("%s(%s)", _buffer_macro_name_pattern, concat(params, ", "));
        }
        _res_macro_call_pattern = stringf("%s(", _res_macro_name_pattern);
        _reduction_mem_macro_call_pattern = _reduction_mem_macro_name_pattern + "(";
        _kernel_res_macro_call_pattern = _kernel_res_macro_name_pattern + "(";
        _kernel_res_orig_macro_call_pattern = _kernel_res_orig_macro_name_pattern + "(";
        for (int i = 0; i < L_DIMS; ++i) {
            if (i > 0) {
                _res_macro_call_pattern.append(", ");
                _reduction_mem_macro_call_pattern.append(", ");
                _kernel_res_macro_call_pattern.append(", ");
                _kernel_res_orig_macro_call_pattern.append(", ");
            }
            _res_macro_call_pattern.append("%s");
            _reduction_mem_macro_call_pattern.append("%s");
            _kernel_res_macro_call_pattern.append("%s");
            _kernel_res_orig_macro_call_pattern.append("%s");
        }
        for (int i = 0; i < R_DIMS; ++i) {
            if (L_DIMS > 0 || i > 0) _reduction_mem_macro_call_pattern.append(", ");
            _reduction_mem_macro_call_pattern.append("%s");
        }
        _res_macro_call_pattern.append(")");
        _reduction_mem_macro_call_pattern.append(")");
        _kernel_res_macro_call_pattern.append(")");
        _kernel_res_orig_macro_call_pattern.append(")");
    }

    const std::string name() const {
        return _result_buffer.name();
    }

    const std::string buffer_macro_name(unsigned int kernel, LEVEL level) const {
        return stringf(_buffer_macro_name_pattern, kernel, level);
    }

    const std::string buffer_def_macro_name(unsigned int kernel, LEVEL level) const {
        return stringf(_buffer_def_macro_name_pattern, kernel, level);
    }

    const std::string res_macro_name(unsigned int kernel, LEVEL level) const {
        return stringf(_res_macro_name_pattern, kernel, level);
    }

    const std::string reduction_mem_macro_name(unsigned int kernel, LEVEL level) const {
        return stringf(_reduction_mem_macro_name_pattern, kernel, level);
    }

    const std::string kernel_res_macro_name(unsigned int kernel, LEVEL level) const {
        return stringf(_kernel_res_macro_name_pattern, kernel, level);
    }

    const std::string kernel_res_orig_macro_name(unsigned int kernel, LEVEL level) const {
        return stringf(_kernel_res_orig_macro_name_pattern, kernel, level);
    }

    const std::string call_buffer_macro(unsigned int kernel, LEVEL level, std::vector<std::string> params) const {
        std::string call = stringf_p(_buffer_macro_call_pattern[LEVEL_ID(level)], kernel, level);
        for (const auto& param : params) {
            call = stringf_p(call, param);
        }
        return call;
    }

    const std::string call_res_macro(unsigned int kernel, LEVEL level, std::vector<std::string> params) const {
        std::string call = stringf_p(_res_macro_call_pattern, kernel, level);
        for (const auto& param : params) {
            call = stringf_p(call, param);
        }
        return call;
    }

    const std::string call_reduction_mem_macro(unsigned int kernel, LEVEL level, std::vector<std::string> params) const {
        std::string call = stringf_p(_reduction_mem_macro_call_pattern, kernel, level);
        for (const auto& param : params) {
            call = stringf_p(call, param);
        }
        return call;
    }

    const std::string call_kernel_res_macro(unsigned int kernel, LEVEL level, std::vector<std::string> params) const {
        std::string call = stringf_p(_kernel_res_macro_call_pattern, kernel, level);
        for (const auto& param : params) {
            call = stringf_p(call, param);
        }
        return call;
    }

    const std::string call_kernel_res_orig_macro(unsigned int kernel, LEVEL level, std::vector<std::string> params) const {
        std::string call = stringf_p(_kernel_res_orig_macro_call_pattern, kernel, level);
        for (const auto& param : params) {
            call = stringf_p(call, param);
        }
        return call;
    }

    const std::string definitions(unsigned int kernel) const {
        std::stringstream code;

        auto l_standard_indices = make_standard_indices(L_DIMS);

        code << "// -------------------- result buffer --------------------" << std::endl;
        code << std::endl << "// check which levels are used";
        for (LEVEL res_level : {LEVEL::PRIVATE, LEVEL::LOCAL, LEVEL::GLOBAL}) {
            code << std::endl << stringf("#if %s", concat(multi_stringf("%c_CB_RES_DEST_LEVEL == %s", V(LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE), V(long_level(res_level))), " || "));
            code << std::endl << stringf("#define K%d_%c_LEVEL_HAS_RESULTS", kernel, res_level);
            code << std::endl << "#endif";
        }
        code << std::endl;

        for (LEVEL res_level : {LEVEL::PRIVATE, LEVEL::LOCAL, LEVEL::GLOBAL}) {
            auto prefix_levels = parent_levels(res_level, false, ORDER::DESCENDING);
            auto suffix_levels = sub_levels(res_level, true, ORDER::ASCENDING);

            auto buffer_indices = make_standard_indices(
                    static_cast<unsigned int>(L_DIMS + suffix_levels.size() * R_DIMS));
            std::vector<std::string> buffer_concatenation(L_DIMS + R_DIMS);
            std::vector<std::string> buffer_def_concatenation(L_DIMS + R_DIMS);

            code << std::endl << stringf("// ------ %s ------", long_level(res_level));
            code << std::endl << stringf("#ifdef K%d_%c_LEVEL_HAS_RESULTS", kernel, res_level);

            if (L_DIMS > 0) {
                // prefix
                if (res_level != LEVEL::GLOBAL) {
                    code << std::endl << stringf("// construct prefix for res_%c", lower_case(res_level));
                }
                std::string current_prefix = "";
                std::string current_def_prefix = "";
                for (LEVEL prefix_level : prefix_levels) {
                    current_prefix = stringf_p("%s_%c_PREFIX_%c_%d()", buffer_macro_name(kernel, res_level),
                                               prefix_level);
                    current_def_prefix = stringf_p("%s_%c_PREFIX_%c_%d()", buffer_def_macro_name(kernel, res_level),
                                                   prefix_level);

                    code << std::endl << stringf("#if %c_CB_RES_DEST_LEVEL == %s", prefix_level, long_level(res_level));
                    for (const auto &dim : dim_range(L_DIMS, 0)) {
                        code << std::endl << stringf("#define %s %s",
                                                     stringf(current_prefix, dim.type, dim.nr),
                                                     stringf("[i_%c_cb_l_%d]",
                                                             lower_case(sub_level(prefix_level)),
                                                             dim.nr
                                                     ));
                    }
                    for (const auto &dim : dim_range(L_DIMS, 0)) {
                        code << std::endl << stringf("#define %s %s",
                                                     stringf(current_def_prefix, dim.type, dim.nr),
                                                     stringf("[%s + ((%s + %s) > 0)]",
                                                             _macros.num_cb(kernel, sub_level(prefix_level), dim),
                                                             _macros.num_extra_cb(kernel, sub_level(prefix_level), dim),
                                                             _macros.num_extra_elems(kernel, sub_level(prefix_level),
                                                                                     dim)));
                    }
                    code << std::endl << "#else";
                    for (const auto &dim : dim_range(L_DIMS, 0)) {
                        code << std::endl << stringf("#define %s", stringf(current_prefix, dim.type, dim.nr));
                    }
                    for (const auto &dim : dim_range(L_DIMS, 0)) {
                        code << std::endl << stringf("#define %s", stringf(current_def_prefix, dim.type, dim.nr));
                    }
                    code << std::endl << "#endif";

                    for (const auto dim : dim_range(L_DIMS, 0)) {
                        buffer_concatenation[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dim)] += stringf(current_prefix,
                                                                                                dim.type, dim.nr);
                        buffer_def_concatenation[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dim)] += stringf(current_def_prefix,
                                                                                                    dim.type, dim.nr);
                    }
                }

                // L-dim addressing & sizing
                for (const auto dim : dim_range(L_DIMS, 0)) {
                    buffer_concatenation[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dim)] += stringf("[%s]", buffer_indices[dim.nr - 1]);
                    buffer_def_concatenation[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dim)] += stringf("[%s]",
                                                                                                _macros.cb_size(kernel, res_level, dim));
                }
            }

            std::string suffix = stringf_p("%s_SUFFIX_%c_%d", buffer_macro_name(kernel, res_level));
            std::string def_suffix = stringf_p("%s_SUFFIX_%c_%d", buffer_def_macro_name(kernel, res_level));
            if (R_DIMS > 0) {
                // suffix
                code << std::endl << stringf("// construct suffix for res_%c", lower_case(res_level));
                auto params = make_standard_indices(static_cast<unsigned int>(suffix_levels.size()));
                for (LEVEL suffix_level : suffix_levels) {
                    code << std::endl << stringf("%s %c_CB_RES_DEST_LEVEL == %s",
                                                 suffix_level == LEVEL::PRIVATE ? "#if  " : "#elif",
                                                 suffix_level, long_level(res_level));
                    if (res_level == LEVEL::GLOBAL) {
                        for (const auto dim : dim_range(0, R_DIMS)) {
                            code << std::endl << stringf(
                                    "#define %s(%s) %s",
                                    stringf(suffix, dim.type, dim.nr),
                                    concat(params, ", "),
                                    make_flat_index(
                                            multi_stringf("(%s)", make_standard_indices(level_range(res_level, suffix_level).size())),
                                            multi_stringf("%s", _macros.num_fu(V(kernel),
                                                                               level_range(res_level, suffix_level),
                                                                               V(dim)))
                                    )
                            );
                        }

                        for (const auto dim : dim_range(0, R_DIMS)) {
                            code << std::endl << stringf(
                                    "#define %s %s",
                                    stringf(def_suffix, dim.type, dim.nr),
                                    concat(multi_stringf("%s",
                                                         _macros.num_fu(V(kernel), level_range(res_level, suffix_level),
                                                                        V(dim))), " * ")
                            );
                        }
                    } else {
                        for (const auto dim : dim_range(0, R_DIMS)) {
                            std::string indexing = "";
                            int i = 0;
                            for (LEVEL l : level_range(res_level, suffix_level)) {
                                indexing += stringf(
                                        "[(%s)]",
                                        params[i]);
                                ++i;
                            }
                            code << std::endl << stringf("#define %s(%s) %s",
                                                         stringf(suffix, dim.type, dim.nr),
                                                         concat(params, ", "),
                                                         indexing);
                        }
                        for (const auto dim : dim_range(0, R_DIMS)) {
                            std::string size = "";
                            int i = 0;
                            for (LEVEL l : level_range(res_level, suffix_level)) {
                                size += stringf(
                                        "[%s]",
                                        _macros.num_fu(kernel, l, dim));
                                ++i;
                            }
                            code << std::endl << stringf("#define %s %s",
                                                         stringf(def_suffix, dim.type, dim.nr),
                                                         size);
                        }
                    }
                }
                for (const auto dim : dim_range(0, R_DIMS)) {
                    buffer_concatenation[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dim)] += stringf("%s(%s)",
                                                                                            stringf(suffix, dim.type,
                                                                                                    dim.nr),
                                                                                            concat(std::vector<std::string>(
                                                                                                    buffer_indices.begin() +
                                                                                                    L_DIMS +
                                                                                                    (dim.nr - 1) *
                                                                                                    suffix_levels.size(),
                                                                                                    buffer_indices.begin() +
                                                                                                    L_DIMS + (dim.nr) *
                                                                                                             suffix_levels.size()),
                                                                                                   ", "));
                    buffer_def_concatenation[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dim)] += stringf(def_suffix, dim.type,
                                                                                                dim.nr);
                }
                code << std::endl << "#endif";
            }

            // buffer abstraction
            code << std::endl << stringf("// buffer abstraction for res_%c", lower_case(res_level));
            std::string buffer_abstraction;
            if (res_level == LEVEL::GLOBAL) {
                buffer_abstraction = "[FLAT_INDEX_IN_DESCENDING_OCL_ORDER(";
                for (const auto dim : dim_range(L_DIMS, 0)) {
                    buffer_abstraction += stringf(
                            "%s%s",
                            dim.nr > 1 ? ", " : "",
                            buffer_indices[dim.nr - 1]);
                }
                for (const auto dim : dim_range(0, R_DIMS)) {
                    buffer_abstraction += stringf("%s%s(%s)",
                                                  L_DIMS > 0 || dim.nr > 1 ? ", " : "",
                                                  stringf(suffix, dim.type, dim.nr),
                                                  concat(std::vector<std::string>(buffer_indices.begin() + L_DIMS + (dim.nr - 1) * suffix_levels.size(), buffer_indices.begin() + L_DIMS + (dim.nr) * suffix_levels.size()), ", "));
                }
                for (const auto dim : dim_range(L_DIMS, 0)) {
                    buffer_abstraction += stringf(", %s", _macros.cb_size(kernel, LEVEL::GLOBAL, dim));
                }
                for (const auto dim : dim_range(0, R_DIMS)) {
                    buffer_abstraction += stringf(", %s", stringf(def_suffix, dim.type, dim.nr));
                }
                buffer_abstraction += ")]";
                if (R_DIMS > 0) {
                    std::string res_g_condition;
                    for (LEVEL s_level : sub_levels(res_level, false)) {
                        if (s_level != sub_level(res_level)) res_g_condition.append(" || ");
                        res_g_condition.append(stringf(
                                "(%c_CB_RES_DEST_LEVEL == %s && %s > 1)",
                                s_level, long_level(res_level),
                                concat(_macros.num_fu(V(kernel), V(s_level), dim_range(0, R_DIMS)), " * ")
                        ));
                    }
                    code << std::endl << stringf("#if %s", res_g_condition);
                    code << std::endl << stringf("#define K%d_RES_%c_BUFFER_NAME() res_g",
                                                 kernel, res_level);
                    code << std::endl << stringf("#define %s K%d_RES_%c_BUFFER_NAME()%s",
                                                 call_buffer_macro(kernel, res_level, buffer_indices),
                                                 kernel, res_level,
                                                 buffer_abstraction);
                    code << std::endl << "#else";
                }
                code << std::endl << stringf("#define K%d_RES_%c_BUFFER_NAME() %s",
                                             kernel, res_level,
                                             kernel == 1 ? "int_res"
                                                         : name());

                if (kernel == 1 && R_DIMS > 0) {
                    code << std::endl << stringf("#if %s", concat(multi_stringf("%s == 1",
                                                                                _macros.num_fu(V(kernel), V(LEVEL::GLOBAL),
                                                                                               dim_range(0, R_DIMS))), " && "));
                    code << std::endl << "// if result is final result, use requested ordering of L-dimensions";
                }
                std::vector<std::string> offset_indices;
                for (const auto &dim : _result_buffer.dimension_order()) {
                    offset_indices.push_back(
                            make_standard_indices(V(dim), L_DIMS, R_DIMS).front()
                    );
                }
                code << std::endl << stringf(
                        "#define %s K%d_RES_%c_BUFFER_NAME()[%s]",
                        call_buffer_macro(kernel, res_level, buffer_indices),
                        kernel, res_level,
                        make_flat_index(
                                offset_indices,
                                _macros.cb_size(V(kernel), V(LEVEL::GLOBAL), _result_buffer.dimension_order())
                        ));
                if (kernel == 1 && R_DIMS > 0) {
                    code << std::endl << "#else";
                    code << std::endl << "// else order in descending OpenCL dimension order";
                    code << std::endl << stringf("#define %s K%d_RES_%c_BUFFER_NAME()%s",
                                                 call_buffer_macro(kernel, res_level, buffer_indices),
                                                 kernel, res_level,
                                                 buffer_abstraction);
                    code << std::endl << "#endif";
                }

                if (R_DIMS > 0) {
                    code << std::endl << "#endif";
                }
            } else {
                code << std::endl << stringf("#define K%d_RES_%c_BUFFER_NAME() res_%c",
                                             kernel, res_level, lower_case(res_level));
                code << std::endl << stringf("#define %s K%d_RES_%c_BUFFER_NAME()%s",
                                             buffer_def_macro_name(kernel, res_level),
                                             kernel, res_level,
                                             stringf("CONCAT_IN_DESCENDING_OCL_ORDER(%s)", concat(buffer_def_concatenation, ", ")));
                buffer_abstraction = stringf("CONCAT_IN_DESCENDING_OCL_ORDER(%s)", concat(buffer_concatenation, ", "));
                code << std::endl << stringf("#define %s K%d_RES_%c_BUFFER_NAME()%s",
                                             call_buffer_macro(kernel, res_level, buffer_indices),
                                             kernel, res_level,
                                             buffer_abstraction);
            }
            code << std::endl << "#endif";
            code << std::endl;
        }

        code << std::endl << "// determine memory destination for results";
        for (LEVEL res_level : {LEVEL::PRIVATE, LEVEL::LOCAL, LEVEL::GLOBAL}) {
            for (LEVEL dest_level : {LEVEL::PRIVATE, LEVEL::LOCAL, LEVEL::GLOBAL}) {
                auto l_standard_indices_filled = l_standard_indices;
                for (int i = 0; i < L_DIMS; ++i) {
                    for (auto sub : sub_levels(dest_level, false, ORDER::DESCENDING)) {
                        l_standard_indices_filled[i] = stringf("%s + (%s)", _macros.cb_offset(kernel, sub, L(i + 1)), l_standard_indices_filled[i]);
                    }
                }
                for (auto sub : sub_levels(dest_level, true, ORDER::DESCENDING)) {
                    for (int i = 1; i <= R_DIMS; ++i) {
                        l_standard_indices_filled.push_back(_macros.fu_id(kernel, sub, R(i)));
                    }
                }

                code << std::endl << stringf("%s %c_CB_RES_DEST_LEVEL == %s",
                                             dest_level == LEVEL::PRIVATE ? "#if  " : "#elif",
                                             res_level,
                                             long_level(dest_level)).c_str();
                code << std::endl << stringf("#define %s %s",
                                             call_res_macro(kernel, res_level, l_standard_indices),
                                             call_buffer_macro(kernel, dest_level, l_standard_indices_filled)).c_str();
            }
            code << std::endl << "#endif" << std::endl;
        }

        if (R_DIMS > 0) {
            code << std::endl << "// check if seperate memory for reduction is needed";
            code << std::endl << "// will only be checked for local level, because of constraints for OpenCL";
            auto params = make_standard_indices(L_DIMS + R_DIMS);
            for (LEVEL reduction_level : {LEVEL::LOCAL}) {
                code << std::endl
                     << stringf("#if %c_CB_RES_DEST_LEVEL < %s", reduction_level, long_level(reduction_level));
                code << std::endl
                     << stringf("#define %s_NAME() %c_reduction_mem", reduction_mem_macro_name(kernel, reduction_level),
                                lower_case(reduction_level));
                code << std::endl << stringf("#define %s %s_NAME()CONCAT_IN_DESCENDING_OCL_ORDER(",
                                             call_reduction_mem_macro(kernel, reduction_level,
                                                                      make_standard_indices(L_DIMS + R_DIMS)),
                                             reduction_mem_macro_name(kernel, reduction_level));
                for (unsigned int l_dim = 1; l_dim <= L_DIMS; ++l_dim) {
                    code << stringf("%s[%s]",
                                    l_dim > 1 ? ", " : "",
                                    params[l_dim - 1]);
                }
                for (unsigned int r_dim = 1; r_dim <= R_DIMS; ++r_dim) {
                    code << stringf("%s[%s]",
                                    L_DIMS > 0 || r_dim > 1 ? ", " : "",
                                    params[L_DIMS + r_dim - 1]);
                }
                code << ")";
                code << std::endl << "#endif";
            }
        }

        // kernel_res abstraction
        code << std::endl << std::endl << "// buffer abstraction for kernel_res buffer";
        if (kernel == 1 && R_DIMS > 0) {
            code << std::endl << stringf("#if %s", concat(multi_stringf("%s == 1",
                                                                        _macros.num_fu(V(kernel), V(LEVEL::GLOBAL),
                                                                                       dim_range(0, R_DIMS))), " && "));
            code << std::endl << "// if result is final result, use requested ordering of L-dimensions";
        }
        auto indices = make_standard_indices(_result_buffer.dimension_order(), L_DIMS, R_DIMS);
        auto sizes = _macros.cb_size(V(kernel), V(LEVEL::GLOBAL), _result_buffer.dimension_order());
        code << std::endl << stringf("#define K%d_KERNEL_RES_BUFFER(%s) %s[%s]",
                                     kernel,
                                     make_standard_params(L_DIMS + R_DIMS),
                                     kernel == 1 ? "int_res" : name(),
                                     make_flat_index(indices, sizes));
        if (kernel == 1 && R_DIMS > 0) {
            code << std::endl << "#else";
            code << std::endl << "// else order in descending OpenCL dimension order";
            code << std::endl << stringf("#define K%d_KERNEL_RES_BUFFER(%s) int_res[FLAT_INDEX_IN_DESCENDING_OCL_ORDER(%s, %s)]",
                                         kernel,
                                         make_standard_params(L_DIMS + R_DIMS),
                                         make_standard_params(L_DIMS + R_DIMS),
                                         concat(join(_macros.cb_size(V(kernel), V(LEVEL::GLOBAL), dim_range(L_DIMS, 0)), _macros.num_fu(V(kernel), V(LEVEL::GLOBAL), dim_range(0, R_DIMS))), ", "));
            code << std::endl << "#endif";
        }
        code << std::endl;
        for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
            auto offset_indices = make_standard_indices(L_DIMS, "%s + (%%val%%)", _macros.cb_offset(V(kernel), V(level), dim_range(L_DIMS, 0)));
            code << std::endl << stringf("#define %s %s(%s)",
                                         call_kernel_res_macro(kernel, level, make_standard_indices(L_DIMS)),
                                         level == LEVEL::GLOBAL
                                         ? stringf("K%d_KERNEL_RES_BUFFER", kernel)
                                         : kernel_res_macro_name(kernel, parent_level(level)),
                                         level == LEVEL::GLOBAL
                                         ? concat(join(offset_indices, _macros.fu_id(V(kernel), V(LEVEL::GLOBAL), dim_range(0, R_DIMS))), ", ")
                                         : concat(offset_indices, ", "));

        }
        if (_keep_both_results) {
            code << std::endl;
            code << std::endl << std::endl << "// buffer abstraction for res_orig buffer";
            code << std::endl << stringf("#define K%d_KERNEL_RES_ORIG_BUFFER(%s) %s_orig[%s]",
                                         kernel,
                                         make_standard_params(L_DIMS),
                                         name(),
                                         make_flat_index(
                                                 make_standard_indices(_result_buffer.dimension_order(), L_DIMS, 0),
                                                 _macros.cb_size(V(kernel), V(LEVEL::GLOBAL), _result_buffer.dimension_order())));
            for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                auto offset_indices = make_standard_indices(L_DIMS, "%s + (%%val%%)", _macros.cb_offset(V(kernel), V(level), dim_range(L_DIMS, 0)));
                code << std::endl << stringf("#define %s %s(%s)",
                                             call_kernel_res_orig_macro(kernel, level, make_standard_indices(L_DIMS)),
                                             level == LEVEL::GLOBAL
                                             ? stringf("K%d_KERNEL_RES_ORIG_BUFFER", kernel)
                                             : kernel_res_orig_macro_name(kernel, parent_level(level)),
                                             concat(offset_indices, ", "));

            }
        }
        return code.str();
    }

    const std::string variable_declarations(unsigned int kernel) const {
        std::stringstream code;
        code << "// declare variables for result memory";
        for (LEVEL res_level : {LEVEL::LOCAL, LEVEL::PRIVATE}) {
            code << std::endl << stringf("// ------ %s ------", long_level(res_level));

            auto params = cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, 0));
            for (int i = 0; i < sub_levels(res_level, true, ORDER::DESCENDING).size() * R_DIMS; ++i) {
                params.push_back("0");
            }

            code << std::endl << stringf("#ifdef K%d_%c_LEVEL_HAS_RESULTS", kernel, res_level);
            code << std::endl << stringf("%s TYPE_TS %s;", res_level == LEVEL::LOCAL ? "__shared__" : "", buffer_def_macro_name(kernel, res_level));
            code << std::endl << "#endif";
        }
        if (R_DIMS > 0) {
            code << std::endl << std::endl << "// declare variables for reduction memory (if needed)";
            for (LEVEL reduction_level : {LEVEL::LOCAL}) {
                code << std::endl << stringf("#if %c_CB_RES_DEST_LEVEL < %s%s",
                                             reduction_level, long_level(reduction_level),
                                             stringf(" && (%s)", concat(multi_stringf("%s > 1",
                                                                                      _macros.num_fu(V(kernel),
                                                                                                     V(reduction_level),
                                                                                                     dim_range(0,
                                                                                                               R_DIMS))),
                                                                        " || ")));
                code << std::endl << stringf("__shared__ TYPE_TS %s_NAME()CONCAT_IN_DESCENDING_OCL_ORDER(",
                                             reduction_mem_macro_name(kernel, reduction_level));
                for (unsigned int l_dim = 1; l_dim <= L_DIMS; ++l_dim) {
                    code << (l_dim > 1 ? ", " : "") << "[" << _macros.cb_size(kernel, reduction_level, L(l_dim)) << "]";
                }
                for (unsigned int r_dim = 1; r_dim <= R_DIMS; ++r_dim) {
                    code << (L_DIMS > 0 || r_dim > 1 ? ", " : "") << "["
                         << _macros.num_fu(kernel, reduction_level, R(r_dim)) << "]";
                }
                code << ");";
                code << std::endl << "#endif";
            }
        }
        return code.str();
    }

    // TODO incorrect for multiple R-dimensions
    const std::string write_back_code(unsigned int kernel, LEVEL level, bool *dim_needs_guard, bool *first_iteration) const {
        std::stringstream code;
        code << "// move results upwards in memory hierarchy" << std::endl;
        code << stringf("#if %c_CB_RES_DEST_LEVEL > %c_CB_RES_DEST_LEVEL%s",
                        parent_level(level), level,
                        R_DIMS > 0 ? stringf(" || %s > 1", _macros.num_fu(kernel, level, R(1))) : "").c_str();
        std::string inner_code;
        auto params = cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, 0));
        auto write_back_code = [&] (auto inner_level, const auto &inner_dimension, bool *inner_dim_needs_guard, bool *first_iteration) -> std::string {
            if (L_DIMS == 0 && level == LEVEL::LOCAL) {
                return wrap_in_cb_processing_loop<L_DIMS, R_DIMS>(
                        kernel, LEVEL::PRIVATE, dim_range(L_DIMS, 0),
                        stringf("#if %c_CB_RES_DEST_LEVEL < %s%s\n%s OPERATOR %s;\n#else\n%s OPERATOR %s;\n#endif",
                                level, upper_case(long_level(level)),
                                R_DIMS > 0 ? stringf(" && %s > 1", _macros.num_fu(kernel, level, R(1))) : "",
                                this->call_res_macro(kernel, parent_level(level), params),
                                this->call_reduction_mem_macro(kernel, level, join(multi_stringf("%s + %s", _macros.cb_offset(V(kernel), V(LEVEL::PRIVATE), dim_range(L_DIMS, 0)), params), _macros.fu_id(V(kernel), V(level), dim_range(0, R_DIMS)))),
                                this->call_res_macro(kernel, parent_level(level), params),
                                this->call_res_macro(kernel, level, params)),
                        false, inner_dim_needs_guard);
            } else {
                return wrap_in_cb_processing_loop<L_DIMS, R_DIMS>(
                        kernel, LEVEL::PRIVATE, dim_range(L_DIMS, 0),
                        stringf("%s OPERATOR %s;",
                                this->call_res_macro(kernel, parent_level(level), params),
                                this->call_res_macro(kernel, level, params)),
                        true, inner_dim_needs_guard);
            }
        };
        if (level != LEVEL::PRIVATE && L_DIMS > 0) {
            auto loops = loop_generator<L_DIMS, R_DIMS>(_macros, V(LEVEL::LOCAL, LEVEL::PRIVATE), dim_range(L_DIMS, 0));
            inner_code = loops.from(kernel, sub_level(level), L(1), write_back_code, true, dim_needs_guard);
        } else {
            inner_code = write_back_code(level, L(1) /*will not be used*/, dim_needs_guard, first_iteration);
        }

        // TODO reorder FUs
//        code << stringf("if (%s < (%s)) {",
//                        _macros.flat_wi_id(kernel, level),
//                        concat(_macros.num_fu(V(kernel), V(level), _result_buffer.dimension_order()), " * ")
//        ) << std::endl;

        std::string result_inputs;
        std::string flat_result_inputs;
        for (const auto &input : _input_wrapper) {
            if (input->result_input()) {
                result_inputs += ", " + input->call_macro(kernel, LEVEL::PRIVATE, cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, R_DIMS)));
                flat_result_inputs += ", " + input->call_macro(kernel, level, multi_stringf("%c_index_%c_%d", V(lower_case(level)), lower_case(dim_range_types(L_DIMS, R_DIMS)), dim_range_nrs(L_DIMS, R_DIMS)));
            }
        }
        if (L_DIMS > 0 && level == LEVEL::LOCAL) {
            code << std::endl << stringf("#if %c_CB_RES_DEST_LEVEL < %s%s",
                                         level, upper_case(long_level(level)),
                                         R_DIMS == 0 ? "" : stringf(" && %s == 1", _macros.num_fu(kernel, level, R(1)))
            );
        }
        if (R_DIMS == 0) {
            if (_apply_result_function) {
                code << std::endl << stringf("#if %c_CB_RES_DEST_LEVEL == GLOBAL", parent_level(level)).c_str();
                code << std::endl << indent(search_and_replace(
                        search_and_replace(inner_code, "OPERATOR", "="),
                        this->call_res_macro(kernel, level, params),
                        stringf("g(%s%s);%s", this->call_res_macro(kernel, level, params), result_inputs,
                                _keep_both_results ? stringf(" K%d_P_KERNEL_RES_ORIG(%s) = %s", kernel, concat(params, ", "), this->call_res_macro(kernel, level, params)) : ""
                        )
                ), 0).c_str();
                code << std::endl << "#else";
            }
            code << std::endl << indent(search_and_replace(inner_code, "OPERATOR", "="), 0).c_str();
            if (_apply_result_function) {
                code << std::endl << "#endif";
            }
        } else {
//            code << indent(stringf("#if %s > 1", _macros.num_fu(kernel, level, R(1))), 0) << std::endl;
//            code << indent(resolve_flat_index(
//                    _macros.flat_wi_id(kernel, level),
//                    _macros.fu_id(V(kernel), V(level), _result_buffer.dimension_order()),
//                    _macros.num_fu(V(kernel), V(level), _result_buffer.dimension_order())
//            ), 0) << std::endl;
            if (first_iteration[LEVEL_ID(parent_level(level))]) {
                auto first_level_not_in_first_iter = get_first_parent_level_not_in_first_iteration(
                        parent_level(level), first_iteration);
                if (first_level_not_in_first_iter != parent_level(level)) {
                    code << std::endl << indent(stringf("#if %c_CB_RES_DEST_LEVEL > %c_CB_RES_DEST_LEVEL\n",
                                           first_level_not_in_first_iter, parent_level(level)), 0).c_str();
                }
                if (_apply_result_function) {
                    code << std::endl << stringf("#if %c_CB_RES_DEST_LEVEL == GLOBAL && %s == 1%s", parent_level(level), _macros.num_fu(kernel, LEVEL::GLOBAL, R(1)), level == LEVEL::LOCAL ? "" : stringf(" && %s == 1", _macros.num_fu(kernel, LEVEL::LOCAL, R(1)))).c_str();
                    code << std::endl << indent(wrap_in_active_condition(
                            kernel,
                            level,
                            dim_needs_guard,
                            search_and_replace(
                                    search_and_replace(inner_code, "OPERATOR", "="),
                                    this->call_res_macro(kernel, level, params),
                                    stringf("g(%s%s);%s",
                                            this->call_res_macro(kernel, level, params), result_inputs,
                                            _keep_both_results ? stringf(" K%d_P_KERNEL_RES_ORIG(%s) = %s", kernel, concat(params, ", "), this->call_res_macro(kernel, level, params)) : ""
                                    )
                            ),
                            level != LEVEL::PRIVATE ? stringf("%s == 0", _macros.fu_id(kernel, level, R(1))) : ""
                    ), 0).c_str();
                    code << std::endl << "#else";
                }
                code << std::endl << indent(wrap_in_active_condition(
                        kernel,
                        level,
                        dim_needs_guard,
                        search_and_replace(inner_code, "OPERATOR", "="),
                        level != LEVEL::PRIVATE ? stringf("%s == 0", _macros.fu_id(kernel, level, R(1))) : ""
                ), 0).c_str();
                if (_apply_result_function) {
                    code << std::endl << "#endif";
                }
                if (first_level_not_in_first_iter != parent_level(level)) {
                    code << std::endl << indent("#else", 0);
                    code << std::endl << indent(wrap_in_active_condition(
                            kernel,
                            level,
                            dim_needs_guard,
                            search_and_replace(inner_code, "OPERATOR", "+="),
                            level != LEVEL::PRIVATE ? stringf("%s == 0", _macros.fu_id(kernel, level, R(1))) : ""
                    ), 0).c_str();
                    code << std::endl << indent("#endif", 0);
                }
            } else {
                code << std::endl << indent(wrap_in_active_condition(
                        kernel,
                        level,
                        dim_needs_guard,
                        search_and_replace(inner_code, "OPERATOR", "+="),
                        level != LEVEL::PRIVATE ? stringf("%s == 0", _macros.fu_id(kernel, level, R(1))) : ""
                ), 0).c_str();
            }
//            code << indent("#endif", 0) << std::endl;
        }
//        code << "}" << std::endl;
        if (L_DIMS > 0 && level == LEVEL::LOCAL) {
            code << std::endl << "#else";
            if (R_DIMS > 0)
                code << std::endl << stringf("#if %s == 1", _macros.num_fu(kernel, level, R(1)));
            code << std::endl << indent(stringf("#if %c_CB_RES_DEST_LEVEL <= %s && CACHE_%c_CB == 0", level, long_level(level), level), R_DIMS > 0);
            code << std::endl << indent((level == LEVEL::LOCAL ? "__syncthreads();" : "__threadfence();"), R_DIMS > 0);
            for (LEVEL p_level : parent_levels(level, false, ORDER::ASCENDING)) {
                code << std::endl << indent(stringf("#elif %c_CB_RES_DEST_LEVEL == %s", level, long_level(p_level)), R_DIMS > 0);
                code << std::endl << indent((p_level == LEVEL::LOCAL ? "__syncthreads();" : "__threadfence();"), R_DIMS > 0);
            }
            code << std::endl << indent("#endif", R_DIMS > 0); // for #if of barrier
            if (R_DIMS > 0)
                code << std::endl << "#endif";
            auto l_indices = multi_stringf("%c_index_%c_%d", V(lower_case(level)),
                                           lower_case(dim_range_types(L_DIMS, 0)),
                                           dim_range_nrs(L_DIMS, 0));
            auto reduction_mem_indices = l_indices;
            for (int i = 0; i < R_DIMS; ++i) reduction_mem_indices.push_back("0");
            auto res_l_indices = l_indices;
            for (int i = 0; i < R_DIMS * 2; ++i) res_l_indices.push_back("0");
            auto res_g_indices = multi_stringf("K%d_%c_CB_OFFSET_%c_%d + %s",
                                               V(kernel),
                                               V(level),
                                               dim_range_types(L_DIMS, 0),
                                               dim_range_nrs(L_DIMS, 0),
                                               l_indices);
            for (int i = 0; i < R_DIMS; ++i) res_g_indices.push_back(std::string("i_wg_r_") + std::to_string(i + 1));
            for (int i = 0; i < R_DIMS * 2; ++i) res_g_indices.push_back("0");
            auto indices = multi_stringf("%c_index_%c_%d", V(lower_case(level)),
                                         lower_case(split_dim_range(_result_buffer.dimension_order()).first),
                                         split_dim_range(_result_buffer.dimension_order()).second);
            auto indices_descending = multi_stringf("DESCENDING_L_DIMS_%d(%s)",
                                                    uint_range(_result_buffer.dimension_order().size() - 1, 0),
                                                    V(concat(multi_stringf("%c_index_%c_%d", V(lower_case(level)),
                                                                           lower_case(dim_range_types(L_DIMS, 0)),
                                                                           dim_range_nrs(L_DIMS, 0)), ", "))
            );
            std::vector<std::string> dim_sizes;
            for (const auto &dim : _result_buffer.dimension_order()) {
                dim_sizes.push_back(_macros.cb_size(kernel, level, dim, dim_needs_guard));
            }
            std::vector<std::string> dim_sizes_descending = multi_stringf("DESCENDING_L_DIMS_%d(%s)",
                                                                          uint_range(L_DIMS - 1, 0),
                                                                          V(concat(_macros.cb_size(V(kernel), V(level), dim_range(L_DIMS, 0), V(dim_needs_guard)), ", "))
            );
            code << std::endl << stringf(R"(#if (%s) / %s > 0
for (size_t step = 0; step < (%s) / %s; ++step) {
  const size_t flat_index = %s + step * %s;
  #if %s == 1
%s
  #else
%s
  #endif
  %s =
  #if L_CB_RES_DEST_LEVEL < LOCAL
    %s
  #elif L_CB_RES_DEST_LEVEL == LOCAL
    %s
  #elif L_CB_RES_DEST_LEVEL == GLOBAL
    %s
  #endif
}
#endif
#if (%s) %% %s > 0
if (%s < (%s) %% %s) {
  const size_t flat_index = %s + ((%s) / %s) * %s;
  #if %s == 1
%s
  #else
%s
  #endif
  %s =
  #if L_CB_RES_DEST_LEVEL < LOCAL
    %s
  #elif L_CB_RES_DEST_LEVEL == LOCAL
    %s
  #elif L_CB_RES_DEST_LEVEL == GLOBAL
    %s
  #endif
}
#endif)",
                                         concat(_macros.cb_size(V(kernel), V(level), dim_range(L_DIMS, 0), V(dim_needs_guard)), " * "),
                                         _macros.flat_num_wi(kernel, level),
                                         concat(_macros.cb_size(V(kernel), V(level), dim_range(L_DIMS, 0), V(dim_needs_guard)), " * "),
                                         _macros.flat_num_wi(kernel, level),
                                         _macros.flat_wi_id(kernel, level), _macros.flat_num_wi(kernel, level),
                                         _macros.num_fu(kernel, parent_level(level), R(1)),
                                         indent(resolve_flat_index("flat_index", indices, dim_sizes), 1),
                                         indent(resolve_flat_index("flat_index", indices_descending, dim_sizes_descending), 1),
                                         call_kernel_res_macro(kernel, level, l_indices),
                                         _apply_result_function ? stringf("#if %c_CB_RES_DEST_LEVEL == GLOBAL%s\n    g(%s%s);\n    #else\n    %s;\n    #endif",
                                                                          parent_level(level), R_DIMS > 0 ? stringf(" && %s == 1", _macros.num_fu(kernel, parent_level(level), R(1))) : "",
                                                                          _keep_both_results ? stringf("K%d_L_KERNEL_RES_ORIG(%s) = %s", kernel, concat(l_indices, ", "), call_reduction_mem_macro(kernel, level, reduction_mem_indices)) : call_reduction_mem_macro(kernel, level, reduction_mem_indices), flat_result_inputs,
                                                                          call_reduction_mem_macro(kernel, level, reduction_mem_indices))
                                                                : call_reduction_mem_macro(kernel, level, reduction_mem_indices) + ";",
                                         _apply_result_function ? stringf("#if %c_CB_RES_DEST_LEVEL == GLOBAL%s\n    g(%s%s);\n    #else\n    %s;\n    #endif",
                                                                          parent_level(level), R_DIMS > 0 ? stringf(" && %s == 1", _macros.num_fu(kernel, parent_level(level), R(1))) : "",
                                                                          _keep_both_results ? stringf("K%d_L_KERNEL_RES_ORIG(%s) = %s", kernel, concat(l_indices, ", "), call_buffer_macro(kernel, level, res_l_indices)) : call_buffer_macro(kernel, level, res_l_indices), flat_result_inputs,
                                                                          call_buffer_macro(kernel, level, res_l_indices))
                                                                : call_buffer_macro(kernel, level, res_l_indices) + ";",
                                         _apply_result_function ? stringf("#if %c_CB_RES_DEST_LEVEL == GLOBAL%s\n    g(%s%s);\n    #else\n    %s;\n    #endif",
                                                                          parent_level(level), R_DIMS > 0 ? stringf(" && %s == 1", _macros.num_fu(kernel, parent_level(level), R(1))) : "",
                                                                          _keep_both_results ? stringf("K%d_L_KERNEL_RES_ORIG(%s) = %s", kernel, concat(l_indices, ", "), call_buffer_macro(kernel, parent_level(level), res_g_indices)) : call_buffer_macro(kernel, parent_level(level), res_g_indices), flat_result_inputs,
                                                                          call_buffer_macro(kernel, parent_level(level), res_g_indices))
                                                                : call_buffer_macro(kernel, parent_level(level), res_g_indices) + ";",
                                         concat(_macros.cb_size(V(kernel), V(level), dim_range(L_DIMS, 0), V(dim_needs_guard)), " * "),
                                         _macros.flat_num_wi(kernel, level),
                                         _macros.flat_wi_id(kernel, level),
                                         concat(_macros.cb_size(V(kernel), V(level), dim_range(L_DIMS, 0), V(dim_needs_guard)), " * "),
                                         _macros.flat_num_wi(kernel, level),
                                         _macros.flat_wi_id(kernel, level),
                                         concat(_macros.cb_size(V(kernel), V(level), dim_range(L_DIMS, 0), V(dim_needs_guard)), " * "),
                                         _macros.flat_num_wi(kernel, level),
                                         _macros.flat_num_wi(kernel, level),
                                         _macros.num_fu(kernel, parent_level(level), R(1)),
                                         indent(resolve_flat_index("flat_index", indices, dim_sizes), 1),
                                         indent(resolve_flat_index("flat_index", indices_descending, dim_sizes_descending), 1),
                                         call_kernel_res_macro(kernel, level, l_indices),
                                         _apply_result_function ? stringf("#if %c_CB_RES_DEST_LEVEL == GLOBAL%s\n    g(%s%s);\n    #else\n    %s;\n    #endif",
                                                                          parent_level(level), R_DIMS > 0 ? stringf(" && %s == 1", _macros.num_fu(kernel, parent_level(level), R(1))) : "",
                                                                          _keep_both_results ? stringf("K%d_L_KERNEL_RES_ORIG(%s) = %s", kernel, concat(l_indices, ", "), call_reduction_mem_macro(kernel, level, reduction_mem_indices)) : call_reduction_mem_macro(kernel, level, reduction_mem_indices), flat_result_inputs,
                                                                          call_reduction_mem_macro(kernel, level, reduction_mem_indices))
                                                                : call_reduction_mem_macro(kernel, level, reduction_mem_indices) + ";",
                                         _apply_result_function ? stringf("#if %c_CB_RES_DEST_LEVEL == GLOBAL%s\n    g(%s%s);\n    #else\n    %s;\n    #endif",
                                                                          parent_level(level), R_DIMS > 0 ? stringf(" && %s == 1", _macros.num_fu(kernel, parent_level(level), R(1))) : "",
                                                                          _keep_both_results ? stringf("K%d_L_KERNEL_RES_ORIG(%s) = %s", kernel, concat(l_indices, ", "), call_buffer_macro(kernel, level, res_l_indices)) : call_buffer_macro(kernel, level, res_l_indices), flat_result_inputs,
                                                                          call_buffer_macro(kernel, level, res_l_indices))
                                                                : call_buffer_macro(kernel, level, res_l_indices) + ";",
                                         _apply_result_function ? stringf("#if %c_CB_RES_DEST_LEVEL == GLOBAL%s\n    g(%s%s);\n    #else\n    %s;\n    #endif",
                                                                          parent_level(level), R_DIMS > 0 ? stringf(" && %s == 1", _macros.num_fu(kernel, parent_level(level), R(1))) : "",
                                                                          _keep_both_results ? stringf("K%d_L_KERNEL_RES_ORIG(%s) = %s", kernel, concat(l_indices, ", "), call_buffer_macro(kernel, parent_level(level), res_g_indices)) : call_buffer_macro(kernel, parent_level(level), res_g_indices), flat_result_inputs,
                                                                          call_buffer_macro(kernel, parent_level(level), res_g_indices))
                                                                : call_buffer_macro(kernel, parent_level(level), res_g_indices) + ";");
            code << std::endl << stringf("#if %c_CB_RES_DEST_LEVEL <= %s", level, long_level(level));
            code << std::endl << (level == LEVEL::LOCAL ? "__syncthreads();" : "__threadfence();");
            for (LEVEL p_level : parent_levels(level, false, ORDER::ASCENDING)) {
                code << std::endl << stringf("#elif %c_CB_RES_DEST_LEVEL == %s", level, long_level(p_level));
                code << std::endl << (p_level == LEVEL::LOCAL ? "__syncthreads();" : "__threadfence();");
            }
            code << std::endl << "#endif"; // for #if of barrier
            code << std::endl << "#endif";
        }
        code << std::endl << "#endif";
        return code.str();
    }

    const std::string reduction(unsigned int kernel, LEVEL level, bool *dim_needs_guard, const std::vector<std::string> &input_names) const {
        std::stringstream code;

        if (level == LEVEL::PRIVATE) {
            code << "// ---------- reduction --------------------";
            code << std::endl << "// will never be necessary on this level";
            code << std::endl << stringf("// because K%d_P_NUM_FU_R == 1 for OpenCL", kernel);
            code << std::endl << "// ---------- end of reduction -------------";
        } else {
            code << stringf("#if %s",
                            concat(multi_stringf("%s > 1",
                                                 _macros.num_fu(V(kernel), V(level), dim_range(0, R_DIMS))
                            ), " || ")) << std::endl;
            code << wrap_in_active_condition(kernel,
                                             level,
                                             dim_needs_guard,
                                             reduction_impl(kernel, level, dim_needs_guard, input_names)
            ).c_str() << std::endl;
            code << "#endif";
        }
        return code.str();
    }

    // TODO incorrect for multiple R-dimensions
    const std::string reset_reduction_memory_space(unsigned int kernel) const {
        std::string code = stringf("%s = 0;\n", call_res_macro(kernel, LEVEL::LOCAL, cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, 0))));

        if (L_DIMS > 0) {
            auto loops = loop_generator<L_DIMS, R_DIMS>(_macros, V(LEVEL::LOCAL, LEVEL::PRIVATE), dim_range(L_DIMS, 0));
            code = loops.from(kernel, LEVEL::LOCAL, L(1), [&](LEVEL level, const dimension_t &dimension, bool *dim_needs_guard, bool *first_iteration) {
                return wrap_in_cb_processing_loop<L_DIMS, R_DIMS>(kernel, LEVEL::PRIVATE, dim_range(L_DIMS, 0), code, true, dim_needs_guard);
            });
        }

        code = stringf(
#ifdef RUNTIME_INPUT_SIZE
                R"(size_t num_active_wi_r_1;
if (%s > 0) {
  // all active WGs process whole local cache blocks
  #if %s > 0
  num_active_wi_r_1 = %s;
  #else
  num_active_wi_r_1 = %s + (%s > 0);
  #endif
} else {
  // not all active WGs process whole cache blocks
  if (%s < %s) {
    #if %s > 0
    num_active_wi_r_1 = %s;
    #else
    num_active_wi_r_1 = %s + (%s > 0);
    #endif
  } else {
    if (%s > 0) {
      num_active_wi_r_1 = %s;
    } else {
      num_active_wi_r_1 = %s + (%s > 0);
    }
  }
}

if (%s >= num_active_wi_r_1) {
%s
})",
#else
                R"(#if %s > 0
// all active WGs process whole local cache blocks
#if %s > 0
const size_t num_active_wi_r_1 = %s;
#else
const size_t num_active_wi_r_1 = %s + (%s > 0);
#endif
#else
// not all active WGs process whole cache blocks
size_t num_active_wi_r_1;
if (%s < %s) {
  #if %s > 0
  num_active_wi_r_1 = %s;
  #else
  num_active_wi_r_1 = %s + (%s > 0);
  #endif
} else {
  #if %s > 0
  num_active_wi_r_1 = %s;
  #else
  num_active_wi_r_1 = %s + (%s > 0);
  #endif
}
#endif

if (%s >= num_active_wi_r_1) {
%s
})",
#endif
                       _macros.num_cb(kernel, LEVEL::LOCAL, R(1)),
                       _macros.num_cb(kernel, LEVEL::PRIVATE, R(1)),
                       _macros.num_fu(kernel, LEVEL::LOCAL, R(1)),
                       _macros.num_extra_cb(kernel, LEVEL::PRIVATE, R(1)),
                       _macros.num_extra_elems(kernel, LEVEL::PRIVATE, R(1)),


                       _macros.fu_id(kernel, LEVEL::GLOBAL, R(1)),
                       _macros.num_extra_cb(kernel, LEVEL::LOCAL, R(1)),
                       _macros.num_cb(kernel, LEVEL::PRIVATE, R(1)),
                       _macros.num_fu(kernel, LEVEL::LOCAL, R(1)),
                       _macros.num_extra_cb(kernel, LEVEL::PRIVATE, R(1)),
                       _macros.num_extra_elems(kernel, LEVEL::PRIVATE, R(1)),

                       _macros.num_cb_in_incomplete_cb(kernel, LEVEL::PRIVATE, R(1), LEVEL::GLOBAL),
                       _macros.num_fu(kernel, LEVEL::LOCAL, R(1)),
                       _macros.num_extra_cb_in_incomplete_cb(kernel, LEVEL::PRIVATE, R(1), LEVEL::GLOBAL),
                       _macros.num_extra_elems_in_incomplete_cb(kernel, LEVEL::PRIVATE, R(1), LEVEL::GLOBAL),

                       _macros.fu_id(kernel, LEVEL::LOCAL, R(1)),
                       indent(code, 1));
        return stringf(R"(// reset memory where reduction takes place for WIs that do not
// calculate intermediate results, so that reduction can always
// be done with all WIs
#if %s > 1
%s
#endif)",
                       _macros.num_fu(kernel, LEVEL::LOCAL, R(1)),
                       wrap_in_active_condition(kernel, LEVEL::LOCAL, nullptr, code));
    }
private:
    const result_buffer &_result_buffer;
    const macros<L_DIMS, R_DIMS> &_macros;

    std::string _buffer_macro_name_pattern;
    std::string _buffer_def_macro_name_pattern;
    std::string _res_macro_name_pattern;
    std::string _buffer_macro_call_pattern[3]; // per level
    std::string _res_macro_call_pattern;

    std::string _reduction_mem_macro_name_pattern;
    std::string _reduction_mem_macro_call_pattern;

    std::string _kernel_res_macro_name_pattern;
    std::string _kernel_res_orig_macro_name_pattern;
    std::string _kernel_res_macro_call_pattern;
    std::string _kernel_res_orig_macro_call_pattern;

    const std::vector<std::unique_ptr<input_wrapper>> &_input_wrapper;
    const bool _apply_result_function;
    const bool _keep_both_results;

    // TODO incorrect for multiple R-dimensions
    std::string wrap_in_active_condition(unsigned int kernel, LEVEL level, bool *dim_needs_guard,
                                         const std::string &body, const std::string &precondition = "") const {
        std::stringstream code;
        std::string active_condition;
//        if (dim_needs_guard[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(level, R(1))]) {
        active_condition = concat(multi_stringf(
                "%s < (%s + (%s > 0))",
                _macros.fu_id(V(kernel), V(parent_level(level)), dim_range(0, R_DIMS)),
                _macros.num_extra_cb(V(kernel), V(level), dim_range(0, R_DIMS), V(dim_needs_guard)),
                _macros.num_extra_elems(V(kernel), V(level), dim_range(0, R_DIMS), V(dim_needs_guard))
        ), " && ");
//        } else {
//            active_condition = concat(multi_stringf(
//                    "%s == %s %% %s",
//                    _macros.fu_id(V(kernel), V(parent_level(level)), dim_range(0, R_DIMS)),
//                    _macros.num_extra_cb(V(kernel), V(level), dim_range(0, R_DIMS), V(dim_needs_guard)),
//                    _macros.num_fu(V(kernel), V(parent_level(level)), dim_range(0, R_DIMS))
//            ), " && ");
//        }
#ifdef RUNTIME_INPUT_SIZE
        if (level == LEVEL::LOCAL || dim_needs_guard[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(parent_level(level), R(1))]) {
            if (precondition.empty()) {
                code << stringf("if (!(%s == 0 && (%s + (%s > 0)) < %s) || %s) {",
                                _macros.num_cb(kernel, level, R(1), dim_needs_guard),
                                _macros.num_extra_cb(kernel, level, R(1), dim_needs_guard),
                                _macros.num_extra_elems(kernel, level, R(1), dim_needs_guard),
                                _macros.num_fu(kernel, parent_level(level), R(1)),
                                active_condition
                ).c_str() << std::endl;
                code << indent(body, 1).c_str() << std::endl;
                code << "}";
            } else {
                code << stringf("if (%s && (!(%s == 0 && (%s + (%s > 0)) < %s) || %s)) {",
                                precondition,
                                _macros.num_cb(kernel, level, R(1), dim_needs_guard),
                                _macros.num_extra_cb(kernel, level, R(1), dim_needs_guard),
                                _macros.num_extra_elems(kernel, level, R(1), dim_needs_guard),
                                _macros.num_fu(kernel, parent_level(level), R(1)),
                                active_condition
                ).c_str() << std::endl;
                code << indent(body, 1).c_str() << std::endl;
                code << "}";
            }
        } else {
#endif
            if (precondition.empty()) {
                code << stringf("#if %s == 0 && (%s + (%s > 0)) < %s",
                                _macros.num_cb(kernel, level, R(1), dim_needs_guard),
                                _macros.num_extra_cb(kernel, level, R(1), dim_needs_guard),
                                _macros.num_extra_elems(kernel, level, R(1), dim_needs_guard),
                                _macros.num_fu(kernel, parent_level(level), R(1))
                ).c_str() << std::endl;
                code << stringf("if (%s)", active_condition).c_str() << std::endl;
                code << "#endif" << std::endl;
                code << "{" << std::endl;
                code << indent(body, 1).c_str() << std::endl;
                code << "}";
            } else {
                code << stringf("#if %s == 0 && (%s + (%s > 0)) < %s",
                                _macros.num_cb(kernel, level, R(1), dim_needs_guard),
                                _macros.num_extra_cb(kernel, level, R(1), dim_needs_guard),
                                _macros.num_extra_elems(kernel, level, R(1), dim_needs_guard),
                                _macros.num_fu(kernel, parent_level(level), R(1))
                ).c_str() << std::endl;
                code << stringf("if (%s && %s) {", precondition, active_condition).c_str() << std::endl;
                code << "#else" << std::endl;
                code << stringf("if (%s) {", precondition).c_str() << std::endl;
                code << "#endif" << std::endl;
                code << indent(body, 1).c_str() << std::endl;
                code << "}";
            }
#ifdef RUNTIME_INPUT_SIZE
        }
#endif
        return code.str();
    }

    // TODO incorrect for multiple R-dimensions
    const std::string reduction_impl(unsigned int kernel, LEVEL level, bool *dim_needs_guard, const std::vector<std::string> &input_names) const {
        std::stringstream code;

        auto l_params = cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, 0));
        auto params_reduction_mem = multi_stringf("%s + %s", _macros.cb_offset(V(kernel), V(LEVEL::PRIVATE), dim_range(L_DIMS, 0)), l_params);
        auto params_reduction_mem_strided = params_reduction_mem;
        for (int i = 0; i < R_DIMS; ++i) {
            params_reduction_mem.push_back(_macros.fu_id(kernel, level, R(i + 1)));
            params_reduction_mem_strided.push_back(stringf("%s + stride", _macros.fu_id(kernel, level, R(i + 1))));
        }

        code << "// wait for all FUs to finish computation";
        for (LEVEL p_level : parent_levels(level, true, ORDER::ASCENDING)) {
            code << std::endl << stringf("%s %c_CB_RES_DEST_LEVEL == %s%s",
                                         p_level == level ? "#if  " : "#elif",
                                         level, long_level(p_level),
                                         p_level == level ? stringf(
                                                 " && %s",
#ifdef INDIVIDUAL_INPUT_CACHING
                                                 concat(multi_stringf("%s_CACHE_%c_CB == 0", upper_case(input_names), V(p_level)), " && ")
#else
                                                 stringf("CACHE_%c_CB == 0", p_level)
#endif
                                         ): "");

            code << std::endl << (p_level == LEVEL::LOCAL ? "__syncthreads();" : "__threadfence();");
        }
        code << std::endl << "#endif"; // for #if of barrier
        code << std::endl << stringf("#if %c_CB_RES_DEST_LEVEL < %s", level, long_level(level));
        std::string copy_body = stringf(
                "%s = %s;",
                call_reduction_mem_macro(kernel, level, params_reduction_mem),
                call_res_macro(kernel, level, l_params));
        if (L_DIMS > 0) {
            auto loops = loop_generator<L_DIMS, R_DIMS>(_macros,
                                                        sub_levels(level, false, ORDER::DESCENDING),
                                                        dim_range(L_DIMS, 0));
            copy_body = loops.from(
                    kernel, sub_level(level), L(1),
                    [&] (auto inner_level, const auto &inner_dimension, bool *inner_dim_needs_guard, bool *first_iteration) -> std::string {
                        return wrap_in_cb_processing_loop<L_DIMS, R_DIMS>(kernel, LEVEL::PRIVATE, dim_range(L_DIMS, 0), copy_body, true, inner_dim_needs_guard);
                    }, true, dim_needs_guard);
        }
        code << std::endl << copy_body;
        code << std::endl << "// wait for all values to be copied into reduction memory";
        code << std::endl << (level == LEVEL::LOCAL ? "__syncthreads();" : "__threadfence();");
        code << std::endl << "#endif"; // for #if of copying into reduction memory
        for (unsigned int r_dim = 1; r_dim <= R_DIMS; ++r_dim) {
            std::string result_inputs;
            for (const auto &input : _input_wrapper) {
                if (input->result_input()) {
                    result_inputs += ", " + input->call_macro(kernel, LEVEL::PRIVATE, cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, R_DIMS)));
                }
            }
            std::string loop_body;
            loop_body.append(stringf("#if %c_CB_RES_DEST_LEVEL < %s\n", level, long_level(level)));
            loop_body.append("// reduction in separate memory location\n");
            loop_body.append(stringf(
                    "%s += %s;\n",
                    call_reduction_mem_macro(kernel, level, params_reduction_mem),
                    call_reduction_mem_macro(kernel, level, params_reduction_mem_strided)));
            unsigned int p_level_nr = 0;
            for (LEVEL p_level : parent_levels(level, true, ORDER::ASCENDING)) {
                auto params = cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, 0));
                auto params_strided = params;
                for (int i = 0; i < L_DIMS; ++i) {
                    for (auto sub : sub_levels(p_level, false, ASCENDING)) {
                        params[i] = stringf("%s + %s", _macros.cb_offset(kernel, sub, L(i + 1)), params[i]);
                        params_strided[i] = stringf("%s + %s", _macros.cb_offset(kernel, sub, L(i + 1)), params_strided[i]);
                    }
                }
                if (p_level != level) {
                    for (auto l : level_range(p_level, parent_level(level))) {
                        for (int i = 1; i <= R_DIMS; ++i) {
                            params.push_back(_macros.fu_id(kernel, l, R(i)));
                            params_strided.push_back(_macros.fu_id(kernel, l, R(i)));
                        }
                    }
                }
                for (int i = 1; i <= R_DIMS; ++i) {
                    params.push_back(_macros.fu_id(kernel, level, R(i)));
                    if (i == r_dim) {
                        params_strided.push_back(stringf("%s + stride", _macros.fu_id(kernel, level, R(i))));
                    } else {
                        params_strided.push_back(_macros.fu_id(kernel, level, R(i)));
                    }
                }
                for (auto l : level_range(sub_level(level), LEVEL::PRIVATE)) {
                    for (int i = 1; i <= R_DIMS; ++i) {
                        params.push_back(_macros.fu_id(kernel, l, R(i)));
                        params_strided.push_back(_macros.fu_id(kernel, l, R(i)));
                    }
                }
                loop_body.append(stringf("#elif %c_CB_RES_DEST_LEVEL == %s\n", level, long_level(p_level)));
                loop_body.append(stringf("%s += %s;\n",
                                         call_buffer_macro(kernel, p_level, params),
                                         call_buffer_macro(kernel, p_level, params_strided)));
                ++p_level_nr;
            }
            loop_body.append("#endif"); // for #if of reduction memory selection
            if (L_DIMS > 0) {
                auto loops = loop_generator<L_DIMS, R_DIMS>(_macros,
                                                            sub_levels(level, false, ORDER::DESCENDING),
                                                            dim_range(L_DIMS, 0));
                loop_body = loops.from(
                        kernel, sub_level(level), L(1),
                        [&] (auto inner_level, const auto &inner_dimension, bool *inner_dim_needs_guard, bool *first_iteration) -> std::string {
                            return wrap_in_cb_processing_loop<L_DIMS, R_DIMS>(kernel, LEVEL::PRIVATE, dim_range(L_DIMS, 0), loop_body, true, inner_dim_needs_guard);
                        }, true, dim_needs_guard);
            }

            code << std::endl << stringf("#if (%s & (%s - 1)) != 0",
                                         _macros.num_fu(kernel, level, R(r_dim)),
                                         _macros.num_fu(kernel, level, R(r_dim)));

            code << std::endl << "// pre processing: reduce number of values to largest possible power of 2";
            code << std::endl << stringf("size_t stride = pow(2.0f, floor(log2((float) %s)));",
                                         _macros.num_fu(kernel, level, R(r_dim)));
            code << std::endl << stringf("if (%s < stride && %s + stride < %s) {",
                                         _macros.fu_id(kernel, level, R(r_dim)),
                                         _macros.fu_id(kernel, level, R(r_dim)),
                                         _macros.num_fu(kernel, level, R(r_dim)));
            code << std::endl << indent(loop_body, 1);
            code << std::endl << "}";
            code << std::endl << "stride /= 2;";
            code << std::endl << stringf("#if %c_CB_RES_DEST_LEVEL <= %s", level, long_level(level));
            code << std::endl << (level == LEVEL::LOCAL ? "__syncthreads();" : "__threadfence();");
            for (LEVEL p_level : parent_levels(level, false, ORDER::ASCENDING)) {
                code << std::endl << stringf("#elif %c_CB_RES_DEST_LEVEL == %s", level, long_level(p_level));
                code << std::endl << (p_level == LEVEL::LOCAL ? "__syncthreads();" : "__threadfence();");
            }
            code << std::endl << "#endif"; // for #if of barrier
            code << std::endl << "#else";
            code << std::endl << stringf("size_t stride = %s / 2;",
                                         _macros.num_fu(kernel, level, R(r_dim)));
            code << std::endl << "#endif"; // for #if number of values to reduce is a power of 2

            code << std::endl << "// perform reduction";
            code << std::endl << "for (; stride > 0; stride /= 2) {";
            code << std::endl << indent(stringf("if (%s < stride) {", _macros.fu_id(kernel, level, R(r_dim))), 1);
            code << std::endl << indent(loop_body, 2);
            code << std::endl << indent("}", 1);
            code << std::endl << indent(stringf("#if %c_CB_RES_DEST_LEVEL <= %s", level, long_level(level)), 1);
            code << std::endl << indent((level == LEVEL::LOCAL ? "__syncthreads();" : "__threadfence();"), 1);
            for (LEVEL p_level : parent_levels(level, false, ORDER::ASCENDING)) {
                code << std::endl << indent(stringf("#elif %c_CB_RES_DEST_LEVEL == %s", level, long_level(p_level)), 1);
                code << std::endl << indent((p_level == LEVEL::LOCAL ? "__syncthreads();" : "__threadfence();"), 1);
            }
            code << std::endl << indent("#endif", 1); // for #if of barrier
            code << std::endl << "}";

            code << std::endl << stringf("#if %c_CB_RES_DEST_LEVEL <= %s", level, long_level(level));
            code << std::endl << (level == LEVEL::LOCAL ? "__syncthreads();" : "__threadfence();");
            for (LEVEL p_level : parent_levels(level, false, ORDER::ASCENDING)) {
                code << std::endl << stringf("#elif %c_CB_RES_DEST_LEVEL == %s", level, long_level(p_level));
                code << std::endl << (p_level == LEVEL::LOCAL ? "__syncthreads();" : "__threadfence();");
            }
            code << std::endl << "#endif"; // for #if of barrier
        }

        return code.str();
    }

};

}
}

#endif //MDH_CUDA_RESULT_BUFFER_WRAPPER_HPP
