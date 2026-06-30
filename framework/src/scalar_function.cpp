//
// Created by   on 01.11.2017.
//

#include "scalar_function.hpp"

namespace md_hom {

scalar_function::scalar_function(const std::string &function_body) : _function_body(function_body) {
}

const std::string &scalar_function::function_body() const {
    return _function_body;
}

}
