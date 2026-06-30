//
// Created by   on 30.01.18.
//

#ifndef MD_BLAS_INPUT_SCALAR_HPP
#define MD_BLAS_INPUT_SCALAR_HPP

#include <string>

namespace md_hom {

class input_scalar_class {
public:
    explicit input_scalar_class(const std::string &name, const bool result_input);

    const std::string &name() const;
    const bool result_input() const;
private:
    const std::string _name;
    const bool        _result_input;
};

input_scalar_class input_scalar(const std::string &name, const bool result_input = false);


}

#endif //MD_BLAS_INPUT_SCALAR_HPP
