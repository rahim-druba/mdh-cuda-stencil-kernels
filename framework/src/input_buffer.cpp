//
// Created by   on 30.10.2017.
//
#include "input_buffer.hpp"

#include <iostream>
#include "helper.hpp"

namespace md_hom {

input_buffer_class::input_buffer_class(const std::string &name,
                                       const std::vector<dimension_t> &used_dimensions,
                                       const bool result_input)
        : _name(name), _used_dimensions(used_dimensions), _result_input(result_input) {

}

const std::string &input_buffer_class::name() const {
    return _name;
}

const std::vector<dimension_t> &input_buffer_class::used_dimensions() const {
    return _used_dimensions;
}

const bool input_buffer_class::result_input() const {
    return _result_input;
}

input_buffer_class input_buffer(const std::string &name,
                                const std::vector<dimension_t> &used_dimensions,
                                const bool result_input) {
    return input_buffer_class(name, used_dimensions, result_input);
}

}