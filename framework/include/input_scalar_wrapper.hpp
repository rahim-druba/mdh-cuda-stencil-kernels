//
// Created by   on 06.11.17.
//

#ifndef MD_BLAS_INPUT_SCALAR_WRAPPER_HPP
#define MD_BLAS_INPUT_SCALAR_WRAPPER_HPP


#include "input_wrapper.hpp"
#include "types.hpp"
#include "macros.hpp"
#include "input_scalar.hpp"

namespace md_hom {
namespace generator {

template<unsigned int L_DIMS, unsigned int R_DIMS>
class input_scalar_wrapper : public input_wrapper {
public:
    input_scalar_wrapper(const input_scalar_class &input_scalar, const macros <L_DIMS, R_DIMS> &macros)
            : _input_scalar(input_scalar), _macros(macros) {

    }

    std::string input_name() const {
        return _input_scalar.name();
    }
    bool result_input() const {
        return _input_scalar.result_input();
    }
    std::string macro_name(unsigned int kernel, LEVEL level) const {
        return _input_scalar.name();
    }
    std::string call_macro(unsigned int kernel, LEVEL level,
                                   const std::vector<std::string> &parameters) const {
        return _input_scalar.name();
    }
    bool has_definitions(unsigned int kernel) const {
        return false;
    }
    std::string definitions(unsigned int kernel) const {
        return "";
    }
    bool supports_caching() const {
        return false;
    }
    std::string scalar_function_parameter_definition() const {
        return stringf("const TYPE_T %s_val", _input_scalar.name());
    }
    std::vector<std::string> scalar_function_parameter_values() const {
        return {call_macro(1, LEVEL::PRIVATE, cb_processing_loop_variable(LEVEL::PRIVATE, dim_range(L_DIMS, R_DIMS)))};
    }

    std::vector<dimension_t> used_dimensions() const {
        return {};
    }

private:
    const input_scalar_class &_input_scalar;
    const macros<L_DIMS, R_DIMS> &_macros;
};

}
}

#endif //MD_BLAS_INPUT_SCALAR_WRAPPER_HPP
