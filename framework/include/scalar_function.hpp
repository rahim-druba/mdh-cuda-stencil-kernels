//
// Created by   on 01.11.2017.
//

#ifndef MD_BLAS_SCALAR_FUNCTION_HPP
#define MD_BLAS_SCALAR_FUNCTION_HPP

#include <string>

namespace md_hom {

class scalar_function {
public:
    explicit scalar_function(const std::string &function_body);

    const std::string& function_body() const;
private:
    const std::string _function_body;
};

}

#endif // MD_BLAS_SCALAR_FUNCTION_HPP
