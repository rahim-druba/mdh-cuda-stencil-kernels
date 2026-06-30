//
// Created by   on 30.10.2017.
//
#include <types.hpp>
#include "helper.hpp"

std::string search_and_replace(const std::string& str,
                               const std::string& oldStr,
                               const std::string& newStr) {
    std::string result = str;
    std::string::size_type pos = 0u;
    while((pos = result.find(oldStr, pos)) != std::string::npos){
        result.replace(pos, oldStr.length(), newStr);
        pos += newStr.length();
    }
    return result;
}

std::string concat(const std::vector<std::string> &elements, const std::string &concatenator) {
    std::stringstream ss;
    bool first = true;
    for (const auto& elem : elements) {
        if (!first) ss << concatenator.c_str();
        ss << elem.c_str();

        first = false;
    }
    return ss.str();
}
std::vector<std::string> wrap(const std::vector<std::string> &elements, const std::string &wrapper_prefix, const std::string &wrapper_suffix) {
    std::vector<std::string> wrapped_elements;
    for (const auto& elem : elements) {
        wrapped_elements.push_back(wrapper_prefix + elem + wrapper_suffix);
    }
    return wrapped_elements;
}
std::string indent(const std::string &text, unsigned int indentation) {
    std::string prefix;
    for (auto i = 0; i < indentation; ++i) {
        prefix += "  ";
    }
    if (text.empty()) return prefix;
    std::string result = search_and_replace(text.substr(0, text.length() - 1), "\r\n", std::string("\r\n") + prefix);
    result = search_and_replace(result, "\n", std::string("\n") + prefix);
    return prefix + result + text.substr(text.length() - 1);
}

namespace md_hom {

dimension_t L(unsigned int nr) {
    return {DIM_TYPE::L, nr};
}
dimension_t R(unsigned int nr) {
    return {DIM_TYPE::R, nr};
}

bool operator==(const dimension_t& lhs, const dimension_t& rhs) {
    return lhs.type == rhs.type && lhs.nr == rhs.nr;
}


namespace generator {

const char* stringf_std_string_helper(const std::string& arg) {
    return arg.c_str();
}

LEVEL parent_level(LEVEL level) {
    switch (level) {
        case LEVEL::GLOBAL:
            // TODO
            return LEVEL::GLOBAL;
        case LEVEL::LOCAL:
            return LEVEL::GLOBAL;
        case LEVEL::PRIVATE:
            return LEVEL::LOCAL;
    }
}

std::vector<LEVEL> parent_levels(LEVEL level, bool inclusive, ORDER order) {
    std::vector<LEVEL> ret;
    if (inclusive) {
        ret.push_back(level);
    }
    switch (level) {
        case LEVEL::GLOBAL:
            break;
        case LEVEL::LOCAL:
            ret.push_back(LEVEL::GLOBAL);
            break;
        case LEVEL::PRIVATE:
            ret.push_back(LEVEL::LOCAL);
            ret.push_back(LEVEL::GLOBAL);
            break;
    }
    if (order == DESCENDING) {
        std::reverse(ret.begin(), ret.end());
    }
    return ret;
}

LEVEL sub_level(LEVEL level) {
    switch (level) {
        case LEVEL::GLOBAL:
            return LEVEL::LOCAL;
        case LEVEL::LOCAL:
            return LEVEL::PRIVATE;
        case LEVEL::PRIVATE:
            // TODO
            return LEVEL::PRIVATE;
    }
}

std::vector<LEVEL> sub_levels(LEVEL level, bool inclusive, ORDER order) {
    std::vector<LEVEL> ret;
    if (inclusive) {
        ret.push_back(level);
    }
    switch (level) {
        case LEVEL::GLOBAL:
            ret.push_back(LEVEL::LOCAL);
            ret.push_back(LEVEL::PRIVATE);
            break;
        case LEVEL::LOCAL:
            ret.push_back(LEVEL::PRIVATE);
            break;
        case LEVEL::PRIVATE:
            break;
    }
    if (order == ASCENDING) {
        std::reverse(ret.begin(), ret.end());
    }
    return ret;
}

unsigned int count(LEVEL start, LEVEL end) {
    std::vector<LEVEL> levels_to_check;
    unsigned int count = 1;
    if (start == end) return count;
    else if (start < end) {
        levels_to_check = sub_levels(start, false, ORDER::DESCENDING);
    } else {
        levels_to_check = parent_levels(start, false, ORDER::ASCENDING);
    }
    for (const auto& level : levels_to_check) {
        ++count;
        if (level == end) {
            return count;
        }
    }
}

std::vector<LEVEL> level_range(LEVEL start, LEVEL end) {
    std::vector<LEVEL> range;
    range.push_back(start);
    std::vector<LEVEL> levels_to_check;
    if (start == end) {
        return range;
    } else if (start < end) {
        levels_to_check = sub_levels(start, false, ORDER::DESCENDING);
    } else {
        levels_to_check = parent_levels(start, false, ORDER::ASCENDING);
    }
    for (const auto& level : levels_to_check) {
        range.push_back(level);
        if (level == end) {
            return range;
        }
    }
}

unsigned int DIM_TYPE_ID(DIM_TYPE dt) {
    switch (dt) {
        case DIM_TYPE::L: return 0;
        case DIM_TYPE::R: return 1;
    }
}

std::pair<std::vector<DIM_TYPE>, std::vector<unsigned int>> split_dim_range(const std::vector<dimension_t>& dim_range) {
    std::pair<std::vector<DIM_TYPE>, std::vector<unsigned int>> result;
    for (const auto& dim : dim_range) {
        result.first.push_back(dim.type);
        result.second.push_back(dim.nr);
    }
    return result;
}

std::vector<dimension_t> dim_range(unsigned int l_dims, unsigned int r_dims) {
    std::vector<dimension_t> result;
    for (unsigned int l_dim = 1; l_dim <= l_dims; ++l_dim) {
        result.push_back(L(l_dim));
    }
    for (unsigned int r_dim = 1; r_dim <= r_dims; ++r_dim) {
        result.push_back(R(r_dim));
    }
    return result;
}
std::vector<DIM_TYPE> dim_range_types(unsigned int l_dims, unsigned int r_dims) {
    return split_dim_range(dim_range(l_dims, r_dims)).first;
}
std::vector<unsigned int> dim_range_nrs(unsigned int l_dims, unsigned int r_dims) {
    return split_dim_range(dim_range(l_dims, r_dims)).second;
}
std::vector<unsigned int> uint_range(unsigned int start, unsigned int end) {
    std::vector<unsigned int> result;
    if (start < end) {
        for (long i = start; i <= end; ++i)
            result.push_back(static_cast<unsigned int>(i));
    } else {
        for (long i = start; i >= end; --i)
            result.push_back(static_cast<unsigned int>(i));
    }

    return result;
}

unsigned int LEVEL_ID(LEVEL l) {
    switch (l) {
        case LEVEL::PRIVATE: return 0;
        case LEVEL::LOCAL:   return 1;
        case LEVEL::GLOBAL:  return 2;
    }
}

std::string long_level(LEVEL l) {
    switch (l) {
        case LEVEL::PRIVATE: return "PRIVATE";
        case LEVEL::LOCAL:   return "LOCAL";
        case LEVEL::GLOBAL:  return "GLOBAL";
    }
}

char lower_case(char letter) {
    assert((letter >= 65 && letter <= 90) || (letter >= 97 && letter <= 122));
    if (letter >= 97 && letter <= 122) {
        return letter;
    } else {
        return static_cast<char>(letter + 32);
    }
}
char lower_case(DIM_TYPE dim_type) {
    return lower_case(static_cast<char>(dim_type));
}
std::vector<char> lower_case(const std::vector<DIM_TYPE> &dim_type) {
    std::vector<char> lc_dim_types;
    for (const auto& type : dim_type) {
        lc_dim_types.push_back(lower_case(type));
    }
    return lc_dim_types;
}
char lower_case(LEVEL level) {
    return lower_case(static_cast<char>(level));
}
std::vector<char> lower_case(const std::vector<LEVEL> &level) {
    std::vector<char> lc_level;
    for (const auto& l : level) {
        lc_level.push_back(lower_case(l));
    }
    return lc_level;
}
std::string lower_case(const std::string &str) {
    std::string ret = str;
    for (int i = 0; i < str.length(); ++i) {
        if (ret[i] >= 65 && ret[i] <= 90) {
            ret[i] = lower_case(ret[i]);
        }
    }
    return ret;
}

char upper_case(char letter) {
    assert((letter >= 65 && letter <= 90) || (letter >= 97 && letter <= 122));
    if (letter >= 65 && letter <= 90) {
        return letter;
    } else {
        return static_cast<char>(letter - 32);
    }
}
char upper_case(DIM_TYPE dim_type) {
    return upper_case(static_cast<char>(dim_type));
}
char upper_case(LEVEL level) {
    return upper_case(static_cast<char>(level));
}
std::string upper_case(const std::string &str) {
    std::string ret = str;
    for (int i = 0; i < str.length(); ++i) {
        if (ret[i] >= 97 && ret[i] <= 122) {
            ret[i] = upper_case(ret[i]);
        }
    }
    return ret;
}
std::vector<std::string> upper_case(const std::vector<std::string> &strs) {
    std::vector<std::string> uc_strs(strs.size());
    for (int i = 0; i < strs.size(); ++i) {
        uc_strs[i] = upper_case(strs[i]);
    }
    return uc_strs;
}

std::string make_flat_index(const std::vector<std::string> &indices, const std::vector<std::string> &dim_sizes) {
    if (indices.empty()) return "0";
    std::stringstream flat_addressing;
    for (int k = 0; k < indices.size(); ++k) {
        if (k > 0) {
            flat_addressing << " + ";
        }
        std::stringstream factor;
        for (int j = k + 1; j < dim_sizes.size(); ++j) {
            factor << " * " << dim_sizes[j].c_str();
        }
        flat_addressing << "(" << indices[k].c_str() << ")" << factor.str().c_str();
    }
    return flat_addressing.str();
}

std::string resolve_flat_index(const std::string &flat_index_name,
                               const std::vector<std::string> &names,
                               const std::vector<std::string> &dim_sizes) {
    std::string result;
    if (names.size() == 1) {
        result.append(stringf("const size_t %s = %s;", names[0].c_str(), flat_index_name.c_str()));
        return result;
    }
    for (int i = 0; i < names.size(); ++i) {
        std::string divisor = "(";
        for (int k = i + 1; k < dim_sizes.size(); ++k) {
            divisor.append(stringf("%s%s", k > i + 1 ? " * " : "", dim_sizes[k].c_str()));
        }
        divisor.append(")");
        std::string modulator = dim_sizes[i];
        if (i == 0) {
            result.append(stringf("const size_t %s = %s / %s;\n", names[i].c_str(), flat_index_name.c_str(), divisor.c_str()));
        } else if (i == names.size() - 1) {
            result.append(stringf("const size_t %s = %s %% %s;", names[i].c_str(), flat_index_name.c_str(), modulator.c_str()));
        } else {
            result.append(stringf("const size_t %s = (%s / %s) %% %s;\n", names[i].c_str(), flat_index_name.c_str(), divisor.c_str(), modulator.c_str()));
        }
    }
    return result;
}

std::string cb_processing_loop_variable(LEVEL level, const dimension_t &dimension) {
    return stringf("i_%c_elem_%c_%d", lower_case(level), lower_case(dimension.type), dimension.nr);
}
std::vector<std::string> cb_processing_loop_variable(LEVEL level, const std::vector<dimension_t> &dimensions) {
    std::vector<std::string> variables;
    for (const auto &dimension : dimensions) {
        variables.push_back(cb_processing_loop_variable(level, dimension));
    }
    return variables;
}
std::vector<std::string> cb_processing_loop_variable(LEVEL level, const std::vector<dimension_t> &used_dimensions,
                                                     unsigned int l_dims, unsigned int r_dims) {
    std::vector<std::string> indices;
    for (unsigned int i = 0; i < l_dims + r_dims; ++i) {
        auto dim = std::find(used_dimensions.begin(), used_dimensions.end(), (i < l_dims ? L(i + 1) : R(i - l_dims + 1)));
        if (dim != used_dimensions.end()) {
            indices.push_back(cb_processing_loop_variable(level, *dim));
        } else {
            indices.emplace_back(1, '0');
        }
    }
    return indices;
}

LEVEL get_first_parent_level_not_in_first_iteration(LEVEL level, bool *first_iteration) {
    if (level == LEVEL::GLOBAL) {
        return level;
    }
    for (LEVEL p_level : parent_levels(level, false, ORDER::ASCENDING)) {
        if (!first_iteration[LEVEL_ID(p_level)]) {
            return p_level;
        }
    }
    return level;
}

}
}
