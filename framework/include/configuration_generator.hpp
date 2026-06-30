//
// Created by   on 09.01.18.
//

#ifndef MD_BLAS_CONFIGURATION_GENERATOR_HPP
#define MD_BLAS_CONFIGURATION_GENERATOR_HPP

#include "md_hom.hpp"
#include "types.hpp"
#include <fstream>

namespace md_hom {
namespace generator {

template <unsigned int L_DIMS, unsigned int R_DIMS, typename... Ts>
class configuration_generator_class {
public:
    explicit configuration_generator_class(const md_hom_class<L_DIMS, R_DIMS, Ts...> &md_hom)
            : _md_hom(md_hom), _counter(0) { }

    void generate(const std::string &file, bool reduce_configurations = false) {
        _counter = 0;
        std::ofstream out_file(file, std::ofstream::out | std::ofstream::trunc);
        std::vector<int> configuration(7 * (L_DIMS + R_DIMS) + 5);
        if (L_DIMS == 0) {
            // in case first dimension is an R dimension and the configurations should
            // not be reduced, generate both configurations with and without reduction
            if (!reduce_configurations) {
                generate_impl(R(1), configuration, out_file, reduce_configurations, false);
            }
            generate_impl(R(1), configuration, out_file, reduce_configurations, true);
        } else {
            generate_impl(L(1), configuration, out_file, reduce_configurations);
        }
        out_file.close();
        std::cout << "Generated " << _counter << " configurations" << std::endl;
    }
private:
    const md_hom_class<L_DIMS, R_DIMS, Ts...>        &_md_hom;
    size_t _counter;

    enum TP {
        M_N,
        G_CB_SIZE,
        NUM_WG,
        L_CB_SIZE,
        NUM_WI,
        P_CB_SIZE,
        OCL_DIM
    };

    int index(const int dimension_id, const TP &tp) {
        return 5 + dimension_id * 7 + tp;
    }

    int index(const dimension_t dimension, const TP &tp) {
        return index(CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dimension), tp);
    }

    int factorial(int n) const {
        return (n == 1 || n == 0) ? 1 : factorial(n - 1) * n;
    }

    void write_config_to_file(std::vector<int> &configuration, std::ofstream &out_file) {
        for (int cache_l_cb = 1; cache_l_cb <= 1; ++cache_l_cb) {
            configuration[0] = cache_l_cb;
            for (int cache_p_cb = 1; cache_p_cb <= 1; ++cache_p_cb) {
                configuration[1] = cache_p_cb;
                // g_cb_res_dest_level is always 2
                configuration[2] = 2;
                for (int l_cb_res_dest_level = 0; l_cb_res_dest_level <= 2; ++l_cb_res_dest_level) {
                    configuration[3] = l_cb_res_dest_level;
                    for (int p_cb_res_dest_level = 0; p_cb_res_dest_level <= l_cb_res_dest_level; ++p_cb_res_dest_level) {
                        configuration[4] = p_cb_res_dest_level;

                        std::function<void(const dimension_t)> iterate_over_dimension_mappings;
                        iterate_over_dimension_mappings = [&](const dimension_t dimension) -> void {
                            bool last_dim = false;
                            dimension_t sub_dim = dimension;
                            if ((dimension.type == DIM_TYPE::L && dimension.nr < L_DIMS)
                                || (dimension.type == DIM_TYPE::R && dimension.nr < R_DIMS)) {
                                ++sub_dim.nr;
                            } else if (dimension.type == DIM_TYPE::L && R_DIMS > 0) {
                                sub_dim.type = DIM_TYPE::R;
                                sub_dim.nr = 1;
                            } else {
                                last_dim = true;
                            }

                            for (int ocl_dim = std::min(2, static_cast<int>(L_DIMS + R_DIMS) - 1) - CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dimension); ocl_dim <= std::min(2, static_cast<int>(L_DIMS + R_DIMS) - 1) - CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dimension); ++ocl_dim) {
//                            for (int ocl_dim = 0; ocl_dim <= std::min(2, static_cast<int>(L_DIMS + R_DIMS) - 1); ++ocl_dim) {
                                configuration[index(dimension, OCL_DIM)] = ocl_dim;
                                if (!last_dim) {
                                    iterate_over_dimension_mappings(sub_dim);
                                } else {
                                    bool mapping_correct = true;
                                    for (int dim_id = 1; dim_id < L_DIMS + R_DIMS; ++dim_id) {
                                        for (int prev_dim_id = 0; prev_dim_id < dim_id; ++prev_dim_id) {
                                            if (configuration[index(dim_id, OCL_DIM)] == configuration[index(prev_dim_id, OCL_DIM)]) {
                                                mapping_correct = false;
                                                break;
                                            }
                                        }
                                        if (!mapping_correct) break;
                                    }
                                    if (!mapping_correct) continue;

                                    out_file << configuration[0];
                                    for (auto i = 1; i < configuration.size(); ++i) {
                                        out_file << "\t" << configuration[i];
                                    }
                                    out_file << std::endl;
                                    ++_counter;
                                }
                            }
                        };
                        iterate_over_dimension_mappings(L_DIMS > 0 ? L(1) : R(1));
                    }
                }
            }
        }
    }

    void generate_impl(const dimension_t dimension,
                       std::vector<int> &configuration,
                       std::ofstream &out_file,
                       bool reduce_configurations,
                       bool reduction = false) {
        bool last_dim = false;
        dimension_t sub_dim = dimension;
        if ((dimension.type == DIM_TYPE::L && dimension.nr < L_DIMS)
            || (dimension.type == DIM_TYPE::R && dimension.nr < R_DIMS)) {
            ++sub_dim.nr;
        } else if (dimension.type == DIM_TYPE::L && R_DIMS > 0) {
            sub_dim.type = DIM_TYPE::R;
            sub_dim.nr = 1;
        } else {
            last_dim = true;
        }

        std::vector<bool> private_first_iteration_choices({true});
        if (dimension.type == DIM_TYPE::R || !reduce_configurations)
            private_first_iteration_choices.push_back(false);
        for (bool private_first_iteration : private_first_iteration_choices) {
            std::vector<bool> private_for_loop_choices;
            if (private_first_iteration) private_for_loop_choices.push_back(true);
            if (dimension.type == DIM_TYPE::R || !reduce_configurations)
                private_for_loop_choices.push_back(false);
            for (bool private_for_loop : private_for_loop_choices) {
                std::vector<bool> private_pp1_choices({true});
                if (dimension.type == DIM_TYPE::R || !reduce_configurations)
                    private_pp1_choices.push_back(false);
                for (bool private_pp1 : private_pp1_choices) {
                    std::vector<bool> private_pp2_choices;
                    if (private_first_iteration || private_pp1) private_pp2_choices.push_back(true);
                    if (dimension.type == DIM_TYPE::R || !reduce_configurations)
                        private_pp2_choices.push_back(false);
                    for (bool private_pp2 : private_pp2_choices) {
                        if (!private_first_iteration && !private_for_loop && !private_pp1 && !private_pp2) {
                            // at least one part on each level has to be executed
                            continue;
                        }
                        if (dimension.type == DIM_TYPE::R && !reduction && private_pp1) {
                            // in R dimensions no reduction is not possible in
                            // combination with first type of post processing
                            continue;
                        }

                        configuration[index(dimension, NUM_WI)] = dimension.type == DIM_TYPE::L || reduction ? 4 : 1;
                        configuration[index(dimension, P_CB_SIZE)] = 2;

                        // set range for valid local cache block sizes
                        int min_l_cb_size = configuration[index(dimension, P_CB_SIZE)];
                        int max_l_cb_size = 128;
                        if (private_first_iteration) {
                            min_l_cb_size = std::max(min_l_cb_size, configuration[index(dimension, NUM_WI)] * configuration[index(dimension, P_CB_SIZE)]);
                        } else {
                            max_l_cb_size = std::min(max_l_cb_size, configuration[index(dimension, NUM_WI)] * configuration[index(dimension, P_CB_SIZE)] - 1);
                        }
                        if (private_for_loop) {
                            min_l_cb_size = std::max(min_l_cb_size, configuration[index(dimension, NUM_WI)] * configuration[index(dimension, P_CB_SIZE)] * 2);
                        } else {
                            max_l_cb_size = std::min(max_l_cb_size, configuration[index(dimension, NUM_WI)] * configuration[index(dimension, P_CB_SIZE)] * 2 - 1);
                        }

                        std::vector<bool> local_first_iteration_choices({true});
                        if (dimension.type == DIM_TYPE::R || !reduce_configurations)
                            local_first_iteration_choices.push_back(false);
                        for (bool local_first_iteration : local_first_iteration_choices) {
                            std::vector<bool> local_for_loop_choices;
                            if (local_first_iteration) local_for_loop_choices.push_back(true);
                            if (dimension.type == DIM_TYPE::R || !reduce_configurations)
                                local_for_loop_choices.push_back(false);
                            for (bool local_for_loop : local_for_loop_choices) {
                                std::vector<bool> local_pp1_choices({true});
                                if (dimension.type == DIM_TYPE::R || !reduce_configurations)
                                    local_pp1_choices.push_back(false);
                                for (bool local_pp1 : local_pp1_choices) {
                                    std::vector<bool> local_pp2_choices;
                                    if (local_first_iteration || local_pp1) local_pp2_choices.push_back(true);
                                    if (dimension.type == DIM_TYPE::R || !reduce_configurations)
                                        local_pp2_choices.push_back(false);
                                    for (bool local_pp2 : local_pp2_choices) {
                                        if (!local_first_iteration && !local_for_loop && !local_pp1 && !local_pp2) {
                                            // at least one part on each level has to be executed
                                            continue;
                                        }

                                        // calculate l_cb_size
                                        int l_cb_size = 0;
                                        for (int lcbs = min_l_cb_size; lcbs <= max_l_cb_size; ++lcbs) {
                                            if (((private_pp1 && ((lcbs / configuration[index(dimension, P_CB_SIZE)]) % configuration[index(dimension, NUM_WI)] != 0))
                                                 || (!private_pp1 && ((lcbs / configuration[index(dimension, P_CB_SIZE)]) % configuration[index(dimension, NUM_WI)] == 0)))

                                                && ((private_pp2 && (lcbs % configuration[index(dimension, P_CB_SIZE)] != 0))
                                                    || (!private_pp2 && (lcbs % configuration[index(dimension, P_CB_SIZE)] == 0)))) {
                                                l_cb_size = lcbs;
                                                break;
                                            }
                                        }
                                        if (l_cb_size == 0) {
                                            std::cout << "could not find valid l_cb_size in [" << min_l_cb_size << ", " << max_l_cb_size << "] for:" << std::endl;
                                            std::cout << "private_first_iteration: " << private_first_iteration << std::endl;
                                            std::cout << "private_for_loop: " << private_for_loop << std::endl;
                                            std::cout << "private_pp1: " << private_pp1 << std::endl;
                                            std::cout << "private_pp2: " << private_pp2 << std::endl;
                                            std::cout << "local_first_iteration: " << local_first_iteration << std::endl;
                                            std::cout << "local_for_loop: " << local_for_loop << std::endl;
                                            std::cout << "local_pp1: " << local_pp1 << std::endl;
                                            std::cout << "local_pp2: " << local_pp2 << std::endl;
                                            if (dimension.type == DIM_TYPE::R) {
                                                std::cout << "reduction: " << reduction << std::endl;
                                            }
                                            std::cout << std::endl;
                                            exit(1);
                                        }

                                        configuration[index(dimension, NUM_WG)] = 4;
                                        configuration[index(dimension, L_CB_SIZE)] = l_cb_size;

                                        // set range for valid global cache block sizes
                                        int min_g_cb_size = configuration[index(dimension, L_CB_SIZE)];
                                        int max_g_cb_size = 512;
                                        if (local_first_iteration) {
                                            min_g_cb_size = std::max(min_g_cb_size, configuration[index(dimension, NUM_WG)] * l_cb_size);
                                        } else {
                                            max_g_cb_size = std::min(max_g_cb_size, configuration[index(dimension, NUM_WG)] * l_cb_size - 1);
                                        }
                                        if (local_for_loop) {
                                            min_g_cb_size = std::max(min_g_cb_size, configuration[index(dimension, NUM_WG)] * l_cb_size * 2);
                                        } else {
                                            max_g_cb_size = std::min(max_g_cb_size, configuration[index(dimension, NUM_WG)] * l_cb_size * 2 - 1);
                                        }

                                        // calculate g_cb_size
                                        int g_cb_size = 0;
                                        for (int gcbs = min_g_cb_size; gcbs <= max_g_cb_size; ++gcbs) {
                                            if (((local_pp1 && ((gcbs / configuration[index(dimension, L_CB_SIZE)]) % configuration[index(dimension, NUM_WG)] != 0))
                                                 || (!local_pp1 && ((gcbs / configuration[index(dimension, L_CB_SIZE)]) % configuration[index(dimension, NUM_WG)] == 0)))

                                                && ((local_pp2 && (gcbs % configuration[index(dimension, L_CB_SIZE)] != 0))
                                                    || (!local_pp2 && (gcbs % configuration[index(dimension, L_CB_SIZE)] == 0)))) {
                                                g_cb_size = gcbs;
                                                break;
                                            }
                                        }
                                        if (g_cb_size == 0) {
                                            std::cout << "could not find valid g_cb_size in [" << min_g_cb_size << ", " << max_g_cb_size << "] for:" << std::endl;
                                            std::cout << "private_first_iteration: " << private_first_iteration << std::endl;
                                            std::cout << "private_for_loop: " << private_for_loop << std::endl;
                                            std::cout << "private_pp1: " << private_pp1 << std::endl;
                                            std::cout << "private_pp2: " << private_pp2 << std::endl;
                                            std::cout << "local_first_iteration: " << local_first_iteration << std::endl;
                                            std::cout << "local_for_loop: " << local_for_loop << std::endl;
                                            std::cout << "local_pp1: " << local_pp1 << std::endl;
                                            std::cout << "local_pp2: " << local_pp2 << std::endl;
                                            if (dimension.type == DIM_TYPE::R) {
                                                std::cout << "reduction: " << reduction << std::endl;
                                            }
                                            std::cout << std::endl;
                                            exit(1);
                                        }

                                        configuration[index(dimension, M_N)] = g_cb_size;
                                        configuration[index(dimension, G_CB_SIZE)] = g_cb_size;

                                        if (!last_dim) {
                                            if (sub_dim.type == DIM_TYPE::R) {
                                                if (!reduce_configurations) {
                                                    generate_impl(sub_dim, configuration, out_file, reduce_configurations, false);
                                                }
                                                generate_impl(sub_dim, configuration, out_file, reduce_configurations, true);
                                            } else {
                                                generate_impl(sub_dim, configuration, out_file, reduce_configurations);
                                            }
                                        } else {
                                            write_config_to_file(configuration, out_file);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
};

template <unsigned int L_DIMS, unsigned int R_DIMS, typename... Ts>
auto configuration_generator(const md_hom_class<L_DIMS, R_DIMS, Ts...> &md_hom) {
    return configuration_generator_class<L_DIMS, R_DIMS, Ts...>(md_hom);
}

}
}

#endif //MD_BLAS_CONFIGURATION_GENERATOR_HPP
