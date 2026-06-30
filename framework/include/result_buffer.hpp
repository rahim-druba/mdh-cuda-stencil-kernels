//
// Created by   on 06.11.17.
//

#ifndef MD_BLAS_RESULT_BUFFER_HPP
#define MD_BLAS_RESULT_BUFFER_HPP

#include <string>

namespace md_hom {

class result_buffer {
public:
    result_buffer(const std::string &name, const std::vector<dimension_t> &dimension_order);

    const std::string name() const {
        return _name;
    }

    const std::vector<dimension_t> dimension_order() const {
        return _dimension_order;
    }
private:
    const std::string              _name;
    const std::vector<dimension_t> _dimension_order;
};


}

#endif //MD_BLAS_RESULT_BUFFER_HPP
