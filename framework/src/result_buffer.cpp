//
// Created by   on 06.11.17.
//
#include <types.hpp>
#include "result_buffer.hpp"

namespace md_hom {

result_buffer::result_buffer(const std::string &name,
                             const std::vector<dimension_t>& dimension_order)
        : _name(name), _dimension_order(dimension_order) {

}

}