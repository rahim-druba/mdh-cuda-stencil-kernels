//
// Created by   on 30.10.2017.
//

#ifndef MD_BLAS_INPUT_STENCIL_BUFFER_HPP
#define MD_BLAS_INPUT_STENCIL_BUFFER_HPP

#include <string>
#include <vector>

#include "types.hpp"

namespace md_hom {

enum oob {
    NONE,
    ZERO
};

template<typename NT>
class input_stencil_buffer_class {
public:

    input_stencil_buffer_class(const std::string &name, const std::vector<dimension_t> &used_dimensions,
                               const NT &neighborhood, const oob oob_strategy)
            : _name(name), _used_dimensions(used_dimensions), _neighborhood(neighborhood),
              _oob_strategy(oob_strategy), _neighborhood_dimensionality(0), _neighborhood_dimension_sizes() {
        parse_neighborhood(_neighborhood);
        _neighborhood_dimensionality = _neighborhood_dimension_sizes.size();
        std::reverse(_neighborhood_dimension_sizes.begin(), _neighborhood_dimension_sizes.end());
        for (const auto &s : _neighborhood_dimension_sizes) {
            _neighborhood_dimension_front_sizes.push_back(s.first);
            _neighborhood_dimension_back_sizes.push_back(s.second);
        }
        _sorted_used_dimensions = _used_dimensions;
        std::sort(_sorted_used_dimensions.begin(), _sorted_used_dimensions.end(), [](const dimension_t &a, const dimension_t &b) {
            if (a.type == DIM_TYPE::L) {
                return b.type == DIM_TYPE::R || a.nr < b.nr;
            } else {
                return b.type == DIM_TYPE::R && a.nr < b.nr;
            }
        });
        for (const auto &dim : _sorted_used_dimensions) {
            _neighborhood_dimension_sizes_sorted.push_back(neighborhood_dimension_size(dim));
            _neighborhood_dimension_front_sizes_sorted.push_back(_neighborhood_dimension_sizes_sorted.back().first);
            _neighborhood_dimension_back_sizes_sorted.push_back(_neighborhood_dimension_sizes_sorted.back().second);
        }
    }

    const std::string &name() const {
        return _name;
    }
    const std::vector<dimension_t> &used_dimensions(bool sort = false) const {
        return sort ? _sorted_used_dimensions : _used_dimensions;
    }
    const NT &neighborhood() const {
        return _neighborhood;
    }
    const oob oob_strategy() const {
        return _oob_strategy;
    }
    const size_t neighborhood_dimensionality() const {
        return _neighborhood_dimensionality;
    }
    const std::vector<std::pair<size_t, size_t>> &neighborhood_dimension_sizes(bool sort = false) const {
        return sort ? _neighborhood_dimension_sizes_sorted : _neighborhood_dimension_sizes;
    }
    const std::vector<size_t> &neighborhood_dimension_front_sizes(bool sort = false) const {
        return sort ? _neighborhood_dimension_front_sizes_sorted : _neighborhood_dimension_front_sizes;
    }
    const std::vector<size_t> &neighborhood_dimension_back_sizes(bool sort = false) const {
        return sort ? _neighborhood_dimension_back_sizes_sorted : _neighborhood_dimension_back_sizes;
    }
    const std::pair<size_t, size_t> &neighborhood_dimension_size(const dimension_t &dimension) const {
        int index = 0;
        for (; index < _used_dimensions.size() && (dimension.type != _used_dimensions[index].type || dimension.nr != _used_dimensions[index].nr); ++index);
        return _neighborhood_dimension_sizes[index];
    }
private:
    const std::string              _name;
    const std::vector<dimension_t> _used_dimensions;
    std::vector<dimension_t>       _sorted_used_dimensions;
    const NT                       _neighborhood;
    const oob                      _oob_strategy;

    size_t                                 _neighborhood_dimensionality;
    std::vector<std::pair<size_t, size_t>> _neighborhood_dimension_sizes; // front, back neighborhood size for each dimension
    std::vector<std::pair<size_t, size_t>> _neighborhood_dimension_sizes_sorted;
    std::vector<size_t>                    _neighborhood_dimension_front_sizes;
    std::vector<size_t>                    _neighborhood_dimension_front_sizes_sorted;
    std::vector<size_t>                    _neighborhood_dimension_back_sizes;
    std::vector<size_t>                    _neighborhood_dimension_back_sizes_sorted;

    template<typename T>
    bool parse_neighborhood(const std::vector<T> &dimension) {
        std::pair<size_t, size_t> sizes = {0, 0};

        bool front = true;
        for (int dim = 0; dim < dimension.size(); ++dim) {
            bool has_origin = parse_neighborhood(dimension[dim]);

            if (has_origin) {
                front = false;
            } else {
                if (front)
                    ++sizes.first;
                else
                    ++sizes.second;
            }
        }
        if (!front) {
            _neighborhood_dimension_sizes.push_back(sizes);
        }
        return !front;
    }
    template<typename T>
    bool parse_neighborhood(const T &scalar) {
        return scalar == 2;
    }
};

template<typename NT>
auto input_stencil_buffer(const std::string &name,
                          const std::vector<dimension_t> &used_dimensions,
                          const NT &neighborhood,
                          const oob oob_strategy) {
    return input_stencil_buffer_class<NT>(name, used_dimensions, neighborhood, oob_strategy);
}

// helper for passing neighborhoods to the constructor
template<typename T>
void N_impl(std::vector<T> &result) {}
template<typename T, typename... Ts>
void N_impl(std::vector<T> &result, const T &arg, const Ts&... args) {
    result.push_back(arg);
    N_impl(result, args...);
}
template<typename T, typename... Ts>
std::vector<T> N(const T &arg, const Ts&... args) {
    std::vector<T> result;
    N_impl(result, arg, args...);
    return result;
}


}
#endif //MD_BLAS_INPUT_STENCIL_BUFFER_HPP
