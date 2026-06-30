//
// Created by   on 30.01.18.
//

#include "input_scalar.hpp"

namespace md_hom {

input_scalar_class::input_scalar_class(const std::string &name, const bool result_input) :
        _name(name), _result_input(result_input) {
}

const std::string &input_scalar_class::name() const {
    return _name;
}

const bool input_scalar_class::result_input() const {
    return _result_input;
}

input_scalar_class input_scalar(const std::string &name, const bool result_input) {
    return input_scalar_class(name, result_input);
}

}