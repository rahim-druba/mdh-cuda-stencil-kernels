//
// Created by   on 30.10.2017.
//

#ifndef MD_BLAS_INPUT_BUFFER_HPP
#define MD_BLAS_INPUT_BUFFER_HPP

#include <string>
#include <vector>

#include "types.hpp"

namespace md_hom {

class input_buffer_class {
public:
    input_buffer_class(const std::string &name, const std::vector<dimension_t> &used_dimensions, const bool result_input);

    const std::string &name() const;
    const std::vector<dimension_t> &used_dimensions() const;
    const bool result_input() const;
private:
    const std::string              _name;
    const std::vector<dimension_t> _used_dimensions;
    const bool                     _result_input;
};

input_buffer_class input_buffer(const std::string &name,
                                const std::vector<dimension_t> &used_dimensions,
                                const bool result_input = false);


}
#endif //MD_BLAS_INPUT_BUFFER_HPP
