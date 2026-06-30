//
// Created by   on 30.10.2017.
//

#ifndef MD_HOM_GENERATOR_CONFIGURATION_CLASS_HPP
#define MD_HOM_GENERATOR_CONFIGURATION_CLASS_HPP

#include <string>
#include <tuple>

#include "scalar_function.hpp"
#include "types.hpp"


namespace md_hom {

template <unsigned int L_DIMS, unsigned int R_DIMS, typename T, typename... Ts>
class md_hom_class {
public:
    md_hom_class(const std::string       &routine_name,
                 const std::tuple<Ts...> &inputs,
                 const scalar_function   &_scalar_function,
                 const scalar_function   &_result_function,
                 const T                 &result,
                 const bool               apply_result_function,
                 const bool               keep_both_results) :
            _routine_name(routine_name), _inputs(inputs),
            _scalar_function(_scalar_function), _result_function(_result_function), _result(result),
            _apply_result_function(apply_result_function), _keep_both_results(keep_both_results) {
    }

    const std::string &routine_name() const {
        return _routine_name;
    }
    const std::tuple<Ts...> &inputs() const {
        return _inputs;
    }
    const scalar_function &get_scalar_function() const {
        return _scalar_function;
    }
    const scalar_function &get_result_function() const {
        return _result_function;
    }
    const T &result() const {
        return _result;
    }
    const bool apply_result_function() const {
        return _apply_result_function;
    }
    const bool keep_both_results() const {
        return _keep_both_results;
    }
private:
    const std::string       _routine_name;
    const std::tuple<Ts...> _inputs;
    const scalar_function   _scalar_function;
    const scalar_function   _result_function;
    const T                 _result;
    const bool              _apply_result_function;
    const bool              _keep_both_results;
};

template <unsigned int L_DIMS, unsigned int R_DIMS, typename T, typename... Ts>
auto md_hom(const std::string       routine_name,
            const std::tuple<Ts...> &inputs,
            const scalar_function   _scalar_function,
            const scalar_function   _result_function,
            const T                 result,
            const bool              apply_result_function = false,
            const bool              keep_both_results = false) {
    return md_hom_class<L_DIMS, R_DIMS, T, Ts...>(routine_name, inputs, _scalar_function, _result_function, result, apply_result_function, keep_both_results);
}

}


#endif //MD_HOM_GENERATOR_CONFIGURATION_CLASS_HPP
