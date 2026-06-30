//
// Created by   on 06.11.17.
//

#ifndef MD_BLAS_INPUT_WRAPPER_HPP
#define MD_BLAS_INPUT_WRAPPER_HPP


namespace md_hom {
namespace generator {

class input_wrapper {
public:
    virtual ~input_wrapper() = default;

    virtual std::string input_name() const = 0;
    virtual bool result_input() const = 0;
    virtual std::string macro_name(unsigned int kernel, LEVEL level) const = 0;
    virtual std::string call_macro(unsigned int kernel, LEVEL level,
                                   const std::vector<std::string> &parameters) const = 0;
    virtual bool has_definitions(unsigned int kernel) const = 0;
    virtual std::string definitions(unsigned int kernel) const = 0;
    virtual bool supports_caching() const = 0;
    virtual std::string caching_variable_declaration(unsigned int kernel, LEVEL level) const {
        return "";
    }
    virtual std::string caching_copy_code(unsigned int kernel, LEVEL level, bool *dim_needs_guard) const {
        return "";
    }
    virtual std::string scalar_function_parameter_definition() const = 0;
    virtual std::vector<std::string> scalar_function_parameter_values() const = 0;

    virtual std::vector<dimension_t> used_dimensions() const = 0;
};

}
}

#endif //MD_BLAS_INPUT_WRAPPER_HPP
