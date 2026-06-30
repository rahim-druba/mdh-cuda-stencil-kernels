//
// Created by   on 30.10.2017.
//

#ifndef MD_BLAS_TYPES_HPP
#define MD_BLAS_TYPES_HPP

#include <algorithm>
#include <vector>

namespace md_hom {

enum class DIM_TYPE : char {
    L = 'L', R = 'R'
};

enum class LEVEL : char {
    PRIVATE = 'P', LOCAL = 'L', GLOBAL = 'G'
};

typedef struct {
    DIM_TYPE type;
    unsigned int nr;
} dimension_t;


namespace generator {

enum ORDER {
    ASCENDING, DESCENDING
};

}
}

#endif //MD_BLAS_TYPES_HPP
