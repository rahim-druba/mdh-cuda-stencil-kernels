//
// Created by   on 30.10.2017.
//

#ifndef MD_BLAS_HELPER_HPP
#define MD_BLAS_HELPER_HPP

#include <memory>
#include <cassert>
#include <regex>
#include <string>
#include <vector>

#include "types.hpp"

//#define ADD_CB_INFO_COMMENTS

std::string search_and_replace(const std::string& str,
                               const std::string& oldStr,
                               const std::string& newStr);
std::string concat(const std::vector<std::string> &elements, const std::string &concatenator = "");
std::vector<std::string> wrap(const std::vector<std::string> &elements, const std::string &wrapper_prefix, const std::string &wrapper_suffix = "");
std::string indent(const std::string &text, unsigned int indentation);
template<typename T>
std::vector<T> join(const std::vector<T> &v1, const std::vector<T> &v2) {
    std::vector<T> joined;
    joined.reserve(v1.size() + v2.size());
    joined.insert(joined.end(), v1.begin(), v1.end());
    joined.insert(joined.end(), v2.begin(), v2.end());
    return joined;
}
template<typename T>
std::vector<T> join(const std::vector<T> &v1, const T &v2) {
    std::vector<T> joined;
    joined.reserve(v1.size() + 1);
    joined.insert(joined.end(), v1.begin(), v1.end());
    joined.insert(joined.end(), v2);
    return joined;
}
template<typename T>
std::vector<T> join(const T &v1, const std::vector<T> &v2) {
    std::vector<T> joined;
    joined.reserve(1 + v2.size());
    joined.insert(joined.end(), v1);
    joined.insert(joined.end(), v2.begin(), v2.end());
    return joined;
}

namespace md_hom {

template <typename... Ts>
std::tuple<Ts...> inputs(Ts... inputs) {
    return std::tuple<Ts...>(inputs...);
}

dimension_t L(unsigned int nr);
dimension_t R(unsigned int nr);

bool operator==(const dimension_t& lhs, const dimension_t& rhs);

namespace generator {

template<typename T>
T max(const T &val) {
    return val;
}
template<typename T, typename... Ts>
T max(const T &val, const Ts&... values) {
    return std::max(val, max(values...));
}
template<typename T>
T min(const T &val) {
    return val;
}
template<typename T, typename... Ts>
T min(const T &val, const Ts&... values) {
    return std::min(val, min(values...));
}

template<typename T>
const T& stringf_std_string_helper(const T& arg) {
    return arg;
}
const char* stringf_std_string_helper(const std::string& arg);
template<typename ... Args>
std::string stringf_p(const std::string& format, const Args&... args) {
    std::string suffix = format;
    std::regex e("([^%]|^)%[diuoxXfFeEgGaAcspn]");
    std::smatch sm;
    unsigned int match_count = 0;
    while (std::regex_search(suffix, sm, e) && match_count < sizeof...(args)) {
        ++match_count;
        suffix = sm.suffix().str();
    }
    assert(match_count == sizeof...(args)); // assert that number of placeholder and argument matches
    std::string prefix = format.substr(0, format.length() - suffix.length());
    auto size = static_cast<size_t>(snprintf(nullptr, 0, prefix.c_str(), stringf_std_string_helper(args)... ) + 1);
    assert(size >= 0); // assert that memory can be allocated
    std::unique_ptr<char[]> buf( new char[ size ] );
    snprintf( buf.get(), size, prefix.c_str(), stringf_std_string_helper(args)... );
    return std::string( buf.get(), buf.get() + size - 1 ).append(suffix);
}
template<typename ... Args>
std::string stringf(const std::string& format, const Args&... args) {
    std::string s = format;
    std::regex e("([^%]|^)%[diuoxXfFeEgGaAcspn]");
    std::smatch sm;
    unsigned int match_count = 0;
    while (std::regex_search(s, sm, e)) {
        ++match_count;
        s = sm.suffix().str();
    }
    assert(match_count == sizeof...(args)); // assert that number of placeholder and argument matches
    auto size = static_cast<size_t>(snprintf(nullptr, 0, format.c_str(), stringf_std_string_helper(args)... ) + 1);
    assert(size >= 0); // assert that memory can be allocated
    std::unique_ptr<char[]> buf( new char[ size ] );
    snprintf( buf.get(), size, format.c_str(), stringf_std_string_helper(args)... );
    return std::string( buf.get(), buf.get() + size - 1 );
}
template<typename ... Args>
std::vector<std::string> multi_stringf_p(const std::string& format, const std::vector<Args>&... args) {
    if (min(args.size()...) == 0) return {};
    std::string suffix = format;
    std::regex e("([^%]|^)?%[diuoxXfFeEgGaAcspn]");
    std::smatch sm;
    unsigned int match_count = 0;
    while (std::regex_search(suffix, sm, e) && match_count < sizeof...(args)) {
        ++match_count;
        suffix = sm.suffix().str();
    }
    assert(match_count == sizeof...(args)); // assert that number of placeholder and argument matches
    std::string prefix = format.substr(0, format.length() - suffix.length());

    std::vector<std::string> formatted_strings;
    for (int i = 0; i < max(args.size()...); ++i) {
        auto size = static_cast<size_t>(snprintf(nullptr, 0, prefix.c_str(), stringf_std_string_helper(args[std::min(size_t(i), args.size() - 1)])...) + 1);
        assert(size >= 0); // assert that memory can be allocated
        std::unique_ptr<char[]> buf(new char[size]);
        snprintf(buf.get(), size, prefix.c_str(), stringf_std_string_helper(args[std::min(size_t(i), args.size() - 1)])...);
        formatted_strings.emplace_back(buf.get(), buf.get() + size - 1);
        formatted_strings.back().append(suffix);
    }
    return formatted_strings;
}
template<typename... Args>
std::vector<std::string> multi_stringf(const std::string& format, const std::vector<Args>&... args) {
    if (min(args.size()...) == 0) return {};
    std::string s = format;
    std::regex e("([^%]|^)%[diuoxXfFeEgGaAcspn]");
    std::smatch sm;
    unsigned int match_count = 0;
    while (std::regex_search(s, sm, e)) {
        ++match_count;
        s = sm.suffix().str();
    }
    assert(match_count == sizeof...(args)); // assert that number of placeholder and argument matches

    std::vector<std::string> formatted_strings;
    for (int i = 0; i < max(args.size()...); ++i) {
        auto size = static_cast<size_t>(snprintf(nullptr, 0, format.c_str(), stringf_std_string_helper(args[std::min(size_t(i), args.size() - 1)])... ) + 1);
        assert(size >= 0); // assert that memory can be allocated
        std::unique_ptr<char[]> buf( new char[ size ] );
        snprintf( buf.get(), size, format.c_str(), stringf_std_string_helper(args[std::min(size_t(i), args.size() - 1)])... );
        formatted_strings.emplace_back(buf.get(), buf.get() + size - 1);
    }
    return formatted_strings;
}

LEVEL parent_level(LEVEL level);

std::vector<LEVEL> parent_levels(LEVEL level, bool inclusive = false, ORDER order = ASCENDING);

LEVEL sub_level(LEVEL level);

std::vector<LEVEL> sub_levels(LEVEL level, bool inclusive = false, ORDER order = DESCENDING);

unsigned int count(LEVEL start, LEVEL end);

std::vector<LEVEL> level_range(LEVEL start, LEVEL end);

unsigned int DIM_TYPE_ID(DIM_TYPE);

unsigned int LEVEL_ID(LEVEL);

template <int L_DIMS, int R_DIMS>
unsigned int CONTINUOUS_DIM_ID(const dimension_t &dim) {
    return (dim.type == DIM_TYPE::L ? 0 : L_DIMS) + dim.nr - 1;
}
template <int L_DIMS, int R_DIMS>
unsigned int CONTINUOUS_DIM_ID(LEVEL level, const dimension_t &dim) {
    return LEVEL_ID(level) * (L_DIMS + R_DIMS) + (dim.type == DIM_TYPE::L ? 0 : L_DIMS) + dim.nr - 1;
}

std::pair<std::vector<DIM_TYPE>, std::vector<unsigned int>> split_dim_range(const std::vector<dimension_t>& dim_range);

std::vector<dimension_t> dim_range(unsigned int l_dims, unsigned int r_dims);
std::vector<DIM_TYPE> dim_range_types(unsigned int l_dims, unsigned int r_dims);
std::vector<unsigned int> dim_range_nrs(unsigned int l_dims, unsigned int r_dims);
std::vector<unsigned int> uint_range(unsigned int start, unsigned int end);

std::string long_level(LEVEL);

char lower_case(char letter);
char lower_case(DIM_TYPE dim_type);
std::vector<char> lower_case(const std::vector<DIM_TYPE> &dim_type);
char lower_case(LEVEL level);
std::vector<char> lower_case(const std::vector<LEVEL> &level);
std::string lower_case(const std::string &str);

char upper_case(char letter);
char upper_case(DIM_TYPE dim_type);
char upper_case(LEVEL level);
std::string upper_case(const std::string &str);
std::vector<std::string> upper_case(const std::vector<std::string> &strs);


template<typename... Ts>
std::vector<std::string> make_standard_indices(unsigned int count, const std::string &pattern = "%%val%%",
                                               const std::vector<Ts>&... args) {
    std::vector<std::string> indices;
    for (unsigned int i = 0; i < count; ++i) {
        indices.push_back(stringf(search_and_replace(pattern, "%%val%%", std::string(1, char('i' + i))), (args[std::min(size_t(i), args.size() - 1)])...));
    }
    return indices;
}
template<typename... Ts>
std::vector<std::string> make_standard_indices(const std::vector<dimension_t> &dimensions,
                                               unsigned int l_dims, unsigned int r_dims,
                                               const std::string &pattern = "%%val%%",
                                               const std::vector<Ts>&... args) {
    std::vector<std::string> std_indices = make_standard_indices(l_dims + r_dims, pattern, args...);
    std::vector<std::string> indices;
    for (const auto &dim : dimensions) {
        indices.push_back(std_indices[(dim.type == DIM_TYPE::R ? l_dims : 0) + dim.nr - 1]);
    }
    return indices;
}
template<unsigned int L_DIMS, unsigned int R_DIMS, typename... Ts>
std::vector<std::string> make_standard_indices_retain_order(const std::vector<dimension_t> &dimensions,
                                                            const std::string &pattern = "%%val%%",
                                                            const std::vector<Ts>&... args) {
    std::vector<std::string> std_indices = make_standard_indices(L_DIMS + R_DIMS, pattern, args...);
    std::vector<std::string> indices;
    for (int i = 0; i < std_indices.size(); ++i) {
        for (const auto &dim : dimensions) {
            if (i == CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dim)) {
                indices.push_back(std_indices[i]);
            }
        }
    }
    return indices;
}

template<typename... Ts>
std::string make_standard_params(unsigned int count, const std::string &pattern = "%%val%%",
                                 const std::vector<Ts>&... args) {
    return concat(make_standard_indices(count, pattern, args...), ", ");
}
template<typename... Ts>
std::string make_standard_params(const std::vector<dimension_t> &dimensions,
                                 unsigned int l_dims, unsigned int r_dims,
                                 const std::string &pattern = "%%val%%",
                                 const std::vector<Ts>&... args) {
    return concat(make_standard_indices(dimensions, l_dims, r_dims, pattern, args...), ", ");
}
template<unsigned int L_DIMS, unsigned int R_DIMS, typename... Ts>
std::string make_standard_params_retain_order(const std::vector<dimension_t> &dimensions,
                                              const std::string &pattern = "%%val%%",
                                              const std::vector<Ts>&... args) {
    return concat(make_standard_indices_retain_order<L_DIMS, R_DIMS>(dimensions, pattern, args...), ", ");
}

std::string make_flat_index(const std::vector<std::string> &indices, const std::vector<std::string> &dim_sizes);

std::string resolve_flat_index(const std::string &flat_index_name,
                               const std::vector<std::string> &names,
                               const std::vector<std::string> &dim_sizes);

std::string cb_processing_loop_variable(LEVEL level, const dimension_t &dimension);
std::vector<std::string> cb_processing_loop_variable(LEVEL level, const std::vector<dimension_t> &dimensions);
std::vector<std::string> cb_processing_loop_variable(LEVEL level, const std::vector<dimension_t> &used_dimensions,
                                                     unsigned int l_dims, unsigned int r_dims);
template<unsigned int L_DIMS, unsigned int R_DIMS>
std::string cb_size(unsigned int kernel, LEVEL level, const dimension_t &dimension, const bool *dim_needs_guards) {
    if (dim_needs_guards == nullptr || !dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(level, dimension)]) {
        return stringf("K%d_%c_CB_SIZE_%c_%d", kernel, level, dimension.type, dimension.nr);
    } else {
        for (LEVEL first_complete_level : parent_levels(level, false, ORDER::ASCENDING)) {
            if (!dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(first_complete_level, dimension)]) {
                return stringf("K%d_%c_CB_REDUCED_SIZE_IN_COMPLETE_%c_CB_%c_%d",
                               kernel, level, first_complete_level,
                               dimension.type, dimension.nr);
            }
        }
    }
    exit(1);
}
template<unsigned int L_DIMS, unsigned int R_DIMS>
std::string wrap_in_cb_processing_loop(unsigned int kernel,
                                       LEVEL level,
                                       const std::vector<dimension_t> &dimensions,
                                       const std::string &body,
                                       bool parentheses,
                                       const bool *dim_needs_guards) {
    if (dimensions.empty()) {
#if ADD_CB_INFO_COMMENTS
        std::string comments;
        for (const auto& dim : dim_range(L_DIMS, R_DIMS)) {
            for (const auto& l : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                comments.append(stringf("// %c_cb_%c_%d: %s\n",
                                        l, dim.type, dim.nr,
                                        dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(l, dim)] ? "incomplete" : "complete"));
            }
        }
        return comments + body;
#else
        return body;
#endif
    }

    return stringf("for (size_t %s = 0; %s < %s; ++%s)%s\n%s%s",
                   cb_processing_loop_variable(level, dimensions.front()),
                   cb_processing_loop_variable(level, dimensions.front()),
                   cb_size<L_DIMS, R_DIMS>(kernel, level, dimensions.front(), dim_needs_guards),
                   cb_processing_loop_variable(level, dimensions.front()),
                   parentheses ? " {" : "",
                   indent(wrap_in_cb_processing_loop<L_DIMS, R_DIMS>(
                           kernel,
                           level,
                           std::vector<dimension_t>(dimensions.begin() + 1, dimensions.end()),
                           body,
                           parentheses,
                           dim_needs_guards),
                          1),
                   parentheses ? "\n}" : ""
    );
}

template<typename T, typename... Ts>
auto V(const T& head, const Ts&... tail) {
    return std::vector<T>({head, tail...});
}

LEVEL get_first_parent_level_not_in_first_iteration(LEVEL level, bool *first_iteration);

template<unsigned int L_DIMS, unsigned int R_DIMS>
std::string descending_ocl_dimension_order(const std::vector<dimension_t> &dimensions,
                                           std::function<std::string(dimension_t,unsigned int)> branch_gen,
                                           unsigned int ocl_dim,
                                           unsigned int order_nr) {
    std::stringstream code;
    if (order_nr == 0) {
//        code << indent(branch_gen(dimensions.front(), order_nr), static_cast<unsigned int>(ocl_dim < L_DIMS + R_DIMS - 1)).c_str();
        code << branch_gen(dimensions.front(), order_nr).c_str();
        return code.str();
    }

    bool first = true;
    for (const auto &dim : dimensions) {
        code << stringf("%sOCL_DIM_%c_%d == %d",
                        first ? "#if   " : "\n#elif ",
                        dim.type, dim.nr,
                        ocl_dim).c_str();
        code << std::endl << indent(branch_gen(dim, order_nr), 1);
        std::vector<dimension_t> sub_dimensions;
        for (const auto &sub_dim : dimensions) {
            if (sub_dim.type != dim.type || sub_dim.nr != dim.nr) sub_dimensions.push_back(sub_dim);
        }
        if (!sub_dimensions.empty()) {
            code << std::endl;
            code << std::endl << indent(descending_ocl_dimension_order<L_DIMS, R_DIMS>(sub_dimensions, branch_gen, ocl_dim - 1, order_nr - 1), 1);
        }
        first = false;
    }
    if (ocl_dim > order_nr) {
        code << std::endl << "#else";
        code << std::endl << indent(descending_ocl_dimension_order<L_DIMS, R_DIMS>(dimensions, branch_gen, ocl_dim - 1, order_nr), 1);
    }
    code << std::endl << "#endif";
    return code.str();
}

template<unsigned int L_DIMS, unsigned int R_DIMS>
std::string descending_ocl_dimension_order(const std::vector<dimension_t> &dimensions,
                                           std::function<std::string(dimension_t,unsigned int)> branch_gen) {
    return descending_ocl_dimension_order<L_DIMS, R_DIMS>(dimensions, branch_gen, L_DIMS + R_DIMS - 1, dimensions.size() - 1);
}


}
}

#endif //MD_BLAS_HELPER_HPP
