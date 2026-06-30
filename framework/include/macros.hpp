//
// Created by   on 03.11.17.
//

#ifndef MD_BLAS_MACROS_HPP
#define MD_BLAS_MACROS_HPP

namespace md_hom {
namespace generator {

#include <string>

#include "types.hpp"
#include "helper.hpp"

template <unsigned int L_DIMS, unsigned int R_DIMS>
class macros {
public:
    macros() {
        // cache block size
        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                for (DIM_TYPE dim_type : {DIM_TYPE::L, DIM_TYPE::R}) {
                    for (unsigned int dim_nr = 1; dim_nr <= (dim_type == DIM_TYPE::L ? L_DIMS : R_DIMS); ++dim_nr) {
                        std::string name = stringf("K%d_%c_CB_SIZE_%c_%d", kernel, level, dim_type, dim_nr);
                        std::string def;
#ifndef RUNTIME_INPUT_SIZE
                        if (kernel == 1 || dim_type == DIM_TYPE::L) {
#endif
                            def = stringf("#define %s %s", name, stringf("%c_CB_SIZE_%c_%d", level, dim_type, dim_nr));
#ifndef RUNTIME_INPUT_SIZE
                        } else {
                            if (level == LEVEL::GLOBAL) {
                                def = stringf("#if CEIL(G_CB_SIZE_R_%d, L_CB_SIZE_R_%d) >= NUM_WG_R_%d\n", dim_nr, dim_nr, dim_nr);
                                def.append(stringf("#define %s %s\n", name, stringf("NUM_WG_%c_%d", dim_type, dim_nr)));
                                def.append("#else\n");
                                def.append(stringf("#define %s CEIL(G_CB_SIZE_R_%d, L_CB_SIZE_R_%d)\n", name, dim_nr, dim_nr));
                                def.append("#endif");
                            } else {
                                def = stringf("#if %s <= %s\n",
                                              stringf("%c_CB_SIZE_%c_%d", level, dim_type, dim_nr),
                                              cb_size(kernel, parent_level(level), {dim_type, dim_nr}));
                                def.append(stringf("#define %s %s\n", name, stringf("%c_CB_SIZE_%c_%d", level, dim_type, dim_nr)));
                                def.append("#else\n");
                                def.append(stringf("#define %s %s\n", name, cb_size(kernel, parent_level(level), {dim_type, dim_nr})));
                                def.append("#endif");
                            }
                        }
#endif

                        resolve_helper(_cb_size, kernel, level, {dim_type, dim_nr}) = name;
                        resolve_helper(_def_cb_size, kernel, level, {dim_type, dim_nr}) = def;
                    }
                }
            }
        }

        // functional unit id
        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                for (DIM_TYPE dim_type : {DIM_TYPE::L, DIM_TYPE::R}) {
                    for (unsigned int dim_nr = 1; dim_nr <= (dim_type == DIM_TYPE::L ? L_DIMS : R_DIMS); ++dim_nr) {
                        std::string name = stringf("K%d_%c_FU_ID_%c_%d", kernel, level, dim_type, dim_nr);
                        std::string def = stringf("#define %s %s", name,
                                                  level == LEVEL::PRIVATE
                                                  ? "0"
                                                  : stringf("i_%s_%c_%d", level == LEVEL::LOCAL ? "wi" : "wg", lower_case(dim_type), dim_nr));
                        resolve_helper(_fu_id, kernel, level, {dim_type, dim_nr}) = name;
                        resolve_helper(_def_fu_id, kernel, level, {dim_type, dim_nr}) = def;
                    }
                }
            }
        }

        // number of functional units
        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                for (DIM_TYPE dim_type : {DIM_TYPE::L, DIM_TYPE::R}) {
                    for (unsigned int dim_nr = 1; dim_nr <= (dim_type == DIM_TYPE::L ? L_DIMS : R_DIMS); ++dim_nr) {
                        std::string name = stringf("K%d_%c_NUM_FU_%c_%d", kernel, level, dim_type, dim_nr);
                        std::string def;
                        if (kernel == 1 || dim_type == DIM_TYPE::L) {
                            def = stringf("#define %s %s", name,
                                                      level == LEVEL::PRIVATE
                                                      ? "1"
                                                      : stringf("NUM_%s_%c_%d", level == LEVEL::GLOBAL ? "WG" : "WI", dim_type, dim_nr));
                        } else {
                            if (level == LEVEL::LOCAL) {
                                def = stringf("#if %s <= CEIL(%s, %s)\n",
                                              stringf("NUM_WI_%c_%d", dim_type, dim_nr),
                                              cb_size(kernel, level, {dim_type, dim_nr}),
                                              cb_size(kernel, sub_level(level), {dim_type, dim_nr}));
                                def.append(stringf("#define %s %s\n", name, stringf("NUM_WI_%c_%d", dim_type, dim_nr)));
                                def.append("#else\n");
                                def.append(stringf("#define %s CEIL(%s, %s)\n", name,
                                                   cb_size(kernel, level, {dim_type, dim_nr}),
                                                   cb_size(kernel, sub_level(level), {dim_type, dim_nr})));
                                def.append("#endif");
                            } else {
                                def = stringf("#define %s 1", name);
                            }
                        }
                        resolve_helper(_num_fu, kernel, level, {dim_type, dim_nr}) = name;
                        resolve_helper(_def_num_fu, kernel, level, {dim_type, dim_nr}) = def;
                    }
                }
            }
        }

        // number of cache blocks (per FU)
        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                for (DIM_TYPE dim_type : {DIM_TYPE::L, DIM_TYPE::R}) {
                    for (unsigned int dim_nr = 1; dim_nr <= (dim_type == DIM_TYPE::L ? L_DIMS : R_DIMS); ++dim_nr) {
                        std::string name = stringf("K%d_%c_NUM_CB_%c_%d", kernel, level, dim_type, dim_nr);
                        std::string def = stringf("#define %s %s", name,
                                                  level == LEVEL::GLOBAL
                                                  ? stringf("1 // == (%c_%d / %s / 1)", dim_type == DIM_TYPE::L ? 'M' : 'N', dim_nr, cb_size(kernel, LEVEL::GLOBAL, {dim_type, dim_nr}))
                                                  : stringf("(%s / %s / %s)", cb_size(kernel, parent_level(level), {dim_type, dim_nr}), cb_size(kernel, level, {dim_type, dim_nr}), num_fu(kernel, parent_level(level), {dim_type, dim_nr})));
                        resolve_helper(_num_cb, kernel, level, {dim_type, dim_nr}) = name;
                        resolve_helper(_def_num_cb, kernel, level, {dim_type, dim_nr}) = def;
                    }
                }
            }
        }

        // number of extra cache blocks
        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                for (DIM_TYPE dim_type : {DIM_TYPE::L, DIM_TYPE::R}) {
                    for (unsigned int dim_nr = 1; dim_nr <= (dim_type == DIM_TYPE::L ? L_DIMS : R_DIMS); ++dim_nr) {
                        std::string name = stringf("K%d_%c_NUM_EXTRA_CB_%c_%d", kernel, level, dim_type, dim_nr);
                        std::string def = stringf("#define %s %s", name,
                                                  level == LEVEL::GLOBAL
                                                  ? stringf("0 // == (%c_%d / %s %% 1)", dim_type == DIM_TYPE::L ? 'M' : 'N', dim_nr, cb_size(kernel, LEVEL::GLOBAL, {dim_type, dim_nr}))
                                                  : stringf("(%s / %s %% %s)", cb_size(kernel, parent_level(level), {dim_type, dim_nr}), cb_size(kernel, level, {dim_type, dim_nr}), num_fu(kernel, parent_level(level), {dim_type, dim_nr})));
                        resolve_helper(_num_extra_cb, kernel, level, {dim_type, dim_nr}) = name;
                        resolve_helper(_def_num_extra_cb, kernel, level, {dim_type, dim_nr}) = def;
                    }
                }
            }
        }

        // number of extra elements
        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                for (DIM_TYPE dim_type : {DIM_TYPE::L, DIM_TYPE::R}) {
                    for (unsigned int dim_nr = 1; dim_nr <= (dim_type == DIM_TYPE::L ? L_DIMS : R_DIMS); ++dim_nr) {
                        std::string name = stringf("K%d_%c_NUM_EXTRA_ELEMS_%c_%d", kernel, level, dim_type, dim_nr);
                        std::string def = stringf("#define %s %s", name,
                                                  level == LEVEL::GLOBAL
                                                  ? stringf("0 // == (%c_%d %% %s)", dim_type == DIM_TYPE::L ? 'M' : 'N', dim_nr, cb_size(kernel, level, {dim_type, dim_nr}))
                                                  : stringf("(%s %% %s)", cb_size(kernel, parent_level(level), {dim_type, dim_nr}), cb_size(kernel, level, {dim_type, dim_nr})));
                        resolve_helper(_num_extra_elems, kernel, level, {dim_type, dim_nr}) = name;
                        resolve_helper(_def_num_extra_elems, kernel, level, {dim_type, dim_nr}) = def;
                    }
                }
            }
        }

        // size of incomplete private cache block
        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            for (LEVEL cb_size_level : {LEVEL::PRIVATE, LEVEL::LOCAL, LEVEL::GLOBAL}) {
                for (LEVEL first_complete_level : parent_levels(cb_size_level, false, ORDER::ASCENDING)) {
                    for (DIM_TYPE dim_type : {DIM_TYPE::L, DIM_TYPE::R}) {
                        for (unsigned int dim_nr = 1; dim_nr <= (dim_type == DIM_TYPE::L ? L_DIMS : R_DIMS); ++dim_nr) {
                            std::string name = stringf(
                                    "K%d_%c_CB_REDUCED_SIZE_IN_COMPLETE_%c_CB_%c_%d",
                                    kernel, cb_size_level, first_complete_level, dim_type, dim_nr);
                            std::string def = stringf(
                                    "#define %s (%s)",
                                    name,
                                    concat(cb_size(V(kernel), level_range(first_complete_level, cb_size_level), V(dimension_t{dim_type, dim_nr})), " % "));

                            resolve_helper(_cb_incomplete_size, kernel, cb_size_level, {dim_type, dim_nr}, first_complete_level) = name;
                            resolve_helper(_def_cb_incomplete_size, kernel, cb_size_level, {dim_type, dim_nr}, first_complete_level) = def;
                        }
                    }
                }
            }
        }

        // number of cache blocks in incomplete parent cache block (per FU)
        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            for (LEVEL level : {LEVEL::PRIVATE, LEVEL::LOCAL}) {
                for (LEVEL first_complete_level : parent_levels(parent_level(level), false, ORDER::ASCENDING)) {
                    for (DIM_TYPE dim_type : {DIM_TYPE::L, DIM_TYPE::R}) {
                        for (unsigned int dim_nr = 1; dim_nr <= (dim_type == DIM_TYPE::L ? L_DIMS : R_DIMS); ++dim_nr) {
                            std::string name = stringf("K%d_%c_NUM_CB_IN_INCOMPLETE_%c_CB_IN_COMPLETE_%c_CB_%c_%d", kernel,
                                                       level, parent_level(level), first_complete_level, dim_type, dim_nr);
                            std::string def = stringf(
                                    "#define %s %s",
                                    name,
                                    stringf("(%s / %s / %s)",
                                            cb_incomplete_size(kernel, parent_level(level), {dim_type, dim_nr}, first_complete_level),
                                            cb_size(kernel, level, {dim_type, dim_nr}),
                                            num_fu(kernel, parent_level(level), {dim_type, dim_nr})));
                            resolve_helper(_num_cb_in_incomplete_cb, kernel, level, {dim_type, dim_nr}, first_complete_level) = name;
                            resolve_helper(_def_num_cb_in_incomplete_cb, kernel, level, {dim_type, dim_nr}, first_complete_level) = def;
                        }
                    }
                }
            }
        }

        // number of extra cache blocks in incomplete parent cache block
        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            for (LEVEL level : {LEVEL::PRIVATE, LEVEL::LOCAL}) {
                for (LEVEL first_complete_level : parent_levels(parent_level(level), false, ORDER::ASCENDING)) {
                    for (DIM_TYPE dim_type : {DIM_TYPE::L, DIM_TYPE::R}) {
                        for (unsigned int dim_nr = 1; dim_nr <= (dim_type == DIM_TYPE::L ? L_DIMS : R_DIMS); ++dim_nr) {
                            std::string name = stringf(
                                    "K%d_%c_NUM_EXTRA_CB_IN_INCOMPLETE_%c_CB_IN_COMPLETE_%c_CB_%c_%d",
                                    kernel, level, parent_level(level), first_complete_level, dim_type, dim_nr);
                            std::string def = stringf(
                                    "#define %s %s",
                                    name,
                                    stringf("((%s / %s) %% %s)",
                                            cb_incomplete_size(kernel, parent_level(level), {dim_type, dim_nr}, first_complete_level),
                                            cb_size(kernel, level, {dim_type, dim_nr}),
                                            num_fu(kernel, parent_level(level), {dim_type, dim_nr})));
                            resolve_helper(_num_extra_cb_in_incomplete_cb, kernel, level, {dim_type, dim_nr}, first_complete_level) = name;
                            resolve_helper(_def_num_extra_cb_in_incomplete_cb, kernel, level, {dim_type, dim_nr}, first_complete_level) = def;
                        }
                    }
                }
            }
        }

        // number of extra elements in incomplete parent cache block
        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            for (LEVEL level : {LEVEL::PRIVATE, LEVEL::LOCAL}) {
                for (LEVEL first_complete_level : parent_levels(parent_level(level), false, ORDER::ASCENDING)) {
                    for (DIM_TYPE dim_type : {DIM_TYPE::L, DIM_TYPE::R}) {
                        for (unsigned int dim_nr = 1; dim_nr <= (dim_type == DIM_TYPE::L ? L_DIMS : R_DIMS); ++dim_nr) {
                            std::string name = stringf(
                                    "K%d_%c_NUM_EXTRA_ELEMS_IN_INCOMPLETE_%c_CB_IN_COMPLETE_%c_CB_%c_%d",
                                    kernel, level, parent_level(level), first_complete_level, dim_type, dim_nr);
                            std::string def = stringf(
                                    "#define %s %s",
                                    name,
                                    stringf("(%s %% %s)",
                                            cb_incomplete_size(kernel, parent_level(level), {dim_type, dim_nr}, first_complete_level),
                                            cb_size(kernel, level, {dim_type, dim_nr})));
                            resolve_helper(_num_extra_elems_in_incomplete_cb, kernel, level, {dim_type, dim_nr}, first_complete_level) = name;
                            resolve_helper(_def_num_extra_elems_in_incomplete_cb, kernel, level, {dim_type, dim_nr}, first_complete_level) = def;
                        }
                    }
                }
            }
        }

        // cache block offsets
        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                for (DIM_TYPE dim_type : {DIM_TYPE::L, DIM_TYPE::R}) {
                    for (unsigned int dim_nr = 1; dim_nr <= (dim_type == DIM_TYPE::L ? L_DIMS : R_DIMS); ++dim_nr) {
                        std::string name = stringf("K%d_%c_CB_OFFSET_%c_%d", kernel, level, dim_type, dim_nr);
                        std::string def = stringf("#define %s %s", name,
                                                  level == LEVEL::GLOBAL
                                                  ? stringf("0 // == (%s * (0 + i_g_cb_%c_%d * 1))", cb_size(kernel, LEVEL::GLOBAL, {dim_type, dim_nr}), dim_type, dim_nr)
                                                  : stringf("(%s * (%s + i_%c_cb_%c_%d * %s))", cb_size(kernel, level, {dim_type, dim_nr}), fu_id(kernel, parent_level(level), {dim_type, dim_nr}), lower_case(level), lower_case(dim_type), dim_nr, num_fu(kernel, parent_level(level), {dim_type, dim_nr})));
                        resolve_helper(_cb_offset, kernel, level, {dim_type, dim_nr}) = name;
                        resolve_helper(_def_cb_offset, kernel, level, {dim_type, dim_nr}) = def;
                    }
                }
            }
        }

        // flat WI ids
        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                std::string name = stringf("K%d_%c_FLAT_WI_ID", kernel, level);
                std::string def = stringf("#define %s (%s)", name,
                                          level == LEVEL::PRIVATE
                                          ? "0"
                                          : stringf("FLAT_INDEX_IN_DESCENDING_OCL_ORDER(%s, %s)",
                                                    concat(multi_stringf("GET_%s_ID_%c_%d", V(level == LEVEL::GLOBAL ? "GLOBAL" : "LOCAL"), dim_range_types(L_DIMS, R_DIMS), dim_range_nrs(L_DIMS, R_DIMS)), ", "),
                                                    concat(multi_stringf("GET_%s_SIZE_%c_%d", V(level == LEVEL::GLOBAL ? "GLOBAL" : "LOCAL"), dim_range_types(L_DIMS, R_DIMS), dim_range_nrs(L_DIMS, R_DIMS)), ", ")
                                          ));
                resolve_helper(_flat_wi_id, kernel, level) = name;
                resolve_helper(_def_flat_wi_id, kernel, level) = def;
            }
        }

        // flat number of WIs
        for (unsigned int kernel = 1; kernel <= 2; ++kernel) {
            for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                std::string name = stringf("K%d_%c_FLAT_NUM_WI", kernel, level);
                std::string def = stringf("#define %s (%s)", name,
                                          level == LEVEL::PRIVATE
                                          ? "1"
                                          : level == LEVEL::LOCAL
                                            ? concat(num_fu(V(kernel), V(level), dim_range(L_DIMS, R_DIMS)), " * ")
                                            : concat(multi_stringf("%s * %s", num_fu(V(kernel), V(level), dim_range(L_DIMS, R_DIMS)), num_fu(V(kernel), V(sub_level(level)), dim_range(L_DIMS, R_DIMS))), " * "));
                resolve_helper(_flat_num_wi, kernel, level) = name;
                resolve_helper(_def_flat_num_wi, kernel, level) = def;
            }
        }
    }

    const std::string fu_id(unsigned int kernel, LEVEL level, dimension_t dimension) const {
        return resolve_helper(_fu_id, kernel, level, dimension);
    }
    const std::string num_fu(unsigned int kernel, LEVEL level, dimension_t dimension) const {
        return resolve_helper(_num_fu, kernel, level, dimension);
    }
    const std::string num_cb(unsigned int kernel, LEVEL level, dimension_t dimension) const {
        return resolve_helper(_num_cb, kernel, level, dimension);
    }
    const std::string num_extra_cb(unsigned int kernel, LEVEL level, dimension_t dimension) const {
        return resolve_helper(_num_extra_cb, kernel, level, dimension);
    }
    const std::string num_extra_elems(unsigned int kernel, LEVEL level, dimension_t dimension) const {
        return resolve_helper(_num_extra_elems, kernel, level, dimension);
    }
    const std::string num_cb_in_incomplete_cb(unsigned int kernel, LEVEL level, dimension_t dimension, LEVEL first_complete_level) const {
        return resolve_helper(_num_cb_in_incomplete_cb, kernel, level, dimension, first_complete_level);
    }
    const std::string num_extra_cb_in_incomplete_cb(unsigned int kernel, LEVEL level, dimension_t dimension, LEVEL first_complete_level) const {
        return resolve_helper(_num_extra_cb_in_incomplete_cb, kernel, level, dimension, first_complete_level);
    }
    const std::string num_extra_elems_in_incomplete_cb(unsigned int kernel, LEVEL level, dimension_t dimension, LEVEL first_complete_level) const {
        return resolve_helper(_num_extra_elems_in_incomplete_cb, kernel, level, dimension, first_complete_level);
    }
    const std::string cb_size(unsigned int kernel, LEVEL level, dimension_t dimension) const {
        return resolve_helper(_cb_size, kernel, level, dimension);
    }
    const std::string cb_incomplete_size(unsigned int kernel, LEVEL level, dimension_t dimension, LEVEL first_complete_level) const {
        return resolve_helper(_cb_incomplete_size, kernel, level, dimension, first_complete_level);
    }
    const std::string cb_offset(unsigned int kernel, LEVEL level, dimension_t dimension) const {
        return resolve_helper(_cb_offset, kernel, level, dimension);
    }
    const std::string flat_wi_id(unsigned int kernel, LEVEL level) const {
        return resolve_helper(_flat_wi_id, kernel, level);
    }
    const std::string flat_num_wi(unsigned int kernel, LEVEL level) const {
        return resolve_helper(_flat_num_wi, kernel, level);
    }

    const std::vector<std::string> fu_id(const std::vector<unsigned int> &kernel,
                                         const std::vector<LEVEL> &level,
                                         const std::vector<dimension_t> &dimension) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), dimension.size())); ++i) {
            results.push_back(resolve_helper(_fu_id, kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)]));
        }
        return results;
    }
    const std::vector<std::string> num_fu(const std::vector<unsigned int> &kernel,
                                          const std::vector<LEVEL> &level,
                                          const std::vector<dimension_t> &dimension) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), dimension.size())); ++i) {
            results.push_back(resolve_helper(_num_fu, kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)]));
        }
        return results;
    }
    const std::vector<std::string> num_cb(const std::vector<unsigned int> &kernel,
                                          const std::vector<LEVEL> &level,
                                          const std::vector<dimension_t> &dimension) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), dimension.size())); ++i) {
            results.push_back(resolve_helper(_num_cb, kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)]));
        }
        return results;
    }
    const std::vector<std::string> num_extra_cb(const std::vector<unsigned int> &kernel,
                                                const std::vector<LEVEL> &level,
                                                const std::vector<dimension_t> &dimension) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), dimension.size())); ++i) {
            results.push_back(resolve_helper(_num_extra_cb, kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)]));
        }
        return results;
    }
    const std::vector<std::string> num_extra_elems(const std::vector<unsigned int> &kernel,
                                                   const std::vector<LEVEL> &level,
                                                   const std::vector<dimension_t> &dimension) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), dimension.size())); ++i) {
            results.push_back(resolve_helper(_num_extra_elems, kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)]));
        }
        return results;
    }
    const std::vector<std::string> num_cb_in_incomplete_cb(const std::vector<unsigned int> &kernel,
                                                           const std::vector<LEVEL> &level,
                                                           const std::vector<dimension_t> &dimension,
                                                           const std::vector<LEVEL> &first_complete_level) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty() || first_complete_level.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), std::max(dimension.size(), first_complete_level.size()))); ++i) {
            results.push_back(resolve_helper(_num_cb_in_incomplete_cb, kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)], first_complete_level[std::min(i, first_complete_level.size() - 1)]));
        }
        return results;
    }
    const std::vector<std::string> num_extra_cb_in_incomplete_cb(const std::vector<unsigned int> &kernel,
                                                                 const std::vector<LEVEL> &level,
                                                                 const std::vector<dimension_t> &dimension,
                                                                 const std::vector<LEVEL> &first_complete_level) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty() || first_complete_level.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), std::max(dimension.size(), first_complete_level.size()))); ++i) {
            results.push_back(resolve_helper(_num_extra_cb_in_incomplete_cb, kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)], first_complete_level[std::min(i, first_complete_level.size() - 1)]));
        }
        return results;
    }
    const std::vector<std::string> num_extra_elems_in_incomplete_cb(const std::vector<unsigned int> &kernel,
                                                                    const std::vector<LEVEL> &level,
                                                                    const std::vector<dimension_t> &dimension,
                                                                    const std::vector<LEVEL> &first_complete_level) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty() || first_complete_level.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), std::max(dimension.size(), first_complete_level.size()))); ++i) {
            results.push_back(resolve_helper(_num_extra_elems_in_incomplete_cb, kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)], first_complete_level[std::min(i, first_complete_level.size() - 1)]));
        }
        return results;
    }
    const std::vector<std::string> cb_size(const std::vector<unsigned int> &kernel,
                                           const std::vector<LEVEL> &level,
                                           const std::vector<dimension_t> &dimension) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), dimension.size())); ++i) {
            results.push_back(resolve_helper(_cb_size, kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)]));
        }
        return results;
    }
    const std::vector<std::string> cb_incomplete_size(const std::vector<unsigned int> &kernel,
                                                      const std::vector<LEVEL> &level,
                                                      const std::vector<dimension_t> &dimension,
                                                      const std::vector<LEVEL> &first_complete_level) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty() || first_complete_level.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), std::max(dimension.size(), first_complete_level.size()))); ++i) {
            results.push_back(resolve_helper(_cb_incomplete_size, kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)], first_complete_level[std::min(i, first_complete_level.size() - 1)]));
        }
        return results;
    }
    const std::vector<std::string> cb_offset(const std::vector<unsigned int> &kernel,
                                             const std::vector<LEVEL> &level,
                                             const std::vector<dimension_t> &dimension) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), dimension.size())); ++i) {
            results.push_back(resolve_helper(_cb_offset, kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)]));
        }
        return results;
    }
    const std::vector<std::string> flat_wi_id(const std::vector<unsigned int> &kernel,
                                              const std::vector<LEVEL> &level) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), level.size()); ++i) {
            results.push_back(resolve_helper(_flat_wi_id, kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)]));
        }
        return results;
    }
    const std::vector<std::string> flat_num_wi(const std::vector<unsigned int> &kernel,
                                               const std::vector<LEVEL> &level) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), level.size()); ++i) {
            results.push_back(resolve_helper(_flat_num_wi, kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)]));
        }
        return results;
    }

    std::string cb_size(unsigned int kernel, LEVEL level, const dimension_t &dimension, const bool *dim_needs_guards) const {
        if (dim_needs_guards == nullptr || !dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(level, dimension)]) {
            return cb_size(kernel, level, dimension);
        } else {
            for (LEVEL first_complete_level : parent_levels(level, false, ORDER::ASCENDING)) {
                if (!dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(first_complete_level, dimension)]) {
                    return cb_incomplete_size(kernel, level, dimension, first_complete_level);
                }
            }
        }
        exit(1);
    }

    std::string num_cb(unsigned int kernel, LEVEL level, const dimension_t &dimension, const bool *dim_needs_guards) const {
        if (dim_needs_guards == nullptr || !dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(parent_level(level), dimension)]) {
            return num_cb(kernel, level, dimension);
        } else {
            for (LEVEL first_complete_level : parent_levels(parent_level(level), false, ORDER::ASCENDING)) {
                if (!dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(first_complete_level, dimension)]) {
                    return num_cb_in_incomplete_cb(kernel, level, dimension, first_complete_level);
                }
            }
        }
        exit(1);
    }
    std::string num_extra_cb(unsigned int kernel, LEVEL level, const dimension_t &dimension, const bool *dim_needs_guards) const {
        if (dim_needs_guards == nullptr || !dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(parent_level(level), dimension)]) {
            return num_extra_cb(kernel, level, dimension);
        } else {
            for (LEVEL first_complete_level : parent_levels(parent_level(level), false, ORDER::ASCENDING)) {
                if (!dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(first_complete_level, dimension)]) {
                    return num_extra_cb_in_incomplete_cb(kernel, level, dimension, first_complete_level);
                }
            }
        }
        exit(1);
    }
    std::string num_extra_elems(unsigned int kernel, LEVEL level, const dimension_t &dimension, const bool *dim_needs_guards) const {
        if (dim_needs_guards == nullptr || !dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(parent_level(level), dimension)]) {
            return num_extra_elems(kernel, level, dimension);
        } else {
            for (LEVEL first_complete_level : parent_levels(parent_level(level), false, ORDER::ASCENDING)) {
                if (!dim_needs_guards[CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(first_complete_level, dimension)]) {
                    return num_extra_elems_in_incomplete_cb(kernel, level, dimension, first_complete_level);
                }
            }
        }
        exit(1);
    }

    std::vector<std::string> cb_size(const std::vector<unsigned int> &kernel, const std::vector<LEVEL> &level, const std::vector<dimension_t> &dimension, const std::vector<bool *> &dim_needs_guards) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty() || dim_needs_guards.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), std::max(dimension.size(), dim_needs_guards.size()))); ++i) {
            results.push_back(cb_size(kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)], dim_needs_guards[std::min(i, dim_needs_guards.size() - 1)]));
        }
        return results;
    }

    std::vector<std::string> num_cb(const std::vector<unsigned int> &kernel, const std::vector<LEVEL> &level, const std::vector<dimension_t> &dimension, const std::vector<bool *> &dim_needs_guards) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty() || dim_needs_guards.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), std::max(dimension.size(), dim_needs_guards.size()))); ++i) {
            results.push_back(num_cb(kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)], dim_needs_guards[std::min(i, dim_needs_guards.size() - 1)]));
        }
        return results;
    }
    std::vector<std::string> num_extra_cb(const std::vector<unsigned int> &kernel, const std::vector<LEVEL> &level, const std::vector<dimension_t> &dimension, const std::vector<bool *> &dim_needs_guards) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty() || dim_needs_guards.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), std::max(dimension.size(), dim_needs_guards.size()))); ++i) {
            results.push_back(num_extra_cb(kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)], dim_needs_guards[std::min(i, dim_needs_guards.size() - 1)]));
        }
        return results;
    }
    std::vector<std::string> num_extra_elems(const std::vector<unsigned int> &kernel, const std::vector<LEVEL> &level, const std::vector<dimension_t> &dimension, const std::vector<bool *> &dim_needs_guards) const {
        std::vector<std::string> results;
        if (kernel.empty() || level.empty() || dimension.empty() || dim_needs_guards.empty()) return results;
        for (size_t i = 0; i < std::max(kernel.size(), std::max(level.size(), std::max(dimension.size(), dim_needs_guards.size()))); ++i) {
            results.push_back(num_extra_elems(kernel[std::min(i, kernel.size() - 1)], level[std::min(i, level.size() - 1)], dimension[std::min(i, dimension.size() - 1)], dim_needs_guards[std::min(i, dim_needs_guards.size() - 1)]));
        }
        return results;
    }

    std::string definitions(unsigned int kernel) const {
        std::stringstream ss;

        bool first_dim = true;
        for (DIM_TYPE dim_type : {DIM_TYPE::L, DIM_TYPE::R}) {
            for (unsigned int dim_nr = 1; dim_nr <= (dim_type == DIM_TYPE::L ? L_DIMS : R_DIMS); ++dim_nr) {
                if (!first_dim) {
                    ss << std::endl;
                    ss << std::endl;
                }
                ss << stringf("// -------------------- %c_%d --------------------", dim_type, dim_nr).c_str() << std::endl;
                ss << std::endl;
                ss << "// cache block sizes" << std::endl;
                for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
#ifdef RUNTIME_INPUT_SIZE
                    if (level == LEVEL::GLOBAL)
                        continue;
#endif
                    ss << resolve_helper(_def_cb_size, kernel, level, {dim_type, dim_nr}) << std::endl;
                }
                ss << std::endl;
                ss << "// functional unit ids" << std::endl;
                for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                    ss << resolve_helper(_def_fu_id, kernel, level, {dim_type, dim_nr}) << std::endl;
                }
                ss << std::endl;
                ss << "// number of functional units" << std::endl;
                for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                    ss << resolve_helper(_def_num_fu, kernel, level, {dim_type, dim_nr}) << std::endl;
                }
                ss << std::endl;
                ss << "// number of cache blocks per functional unit" << std::endl;
                for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                    ss << resolve_helper(_def_num_cb, kernel, level, {dim_type, dim_nr}) << std::endl;
                }
                ss << std::endl;
                ss << "// number of extra cache blocks" << std::endl;
                for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                    ss << resolve_helper(_def_num_extra_cb, kernel, level, {dim_type, dim_nr}) << std::endl;
                }
                ss << std::endl;
                ss << "// number of extra elements" << std::endl;
                for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                    ss << resolve_helper(_def_num_extra_elems, kernel, level, {dim_type, dim_nr}) << std::endl;
                }
                ss << std::endl;
                ss << "// size of incomplete cache blocks" << std::endl;
                for (LEVEL cb_size_level : {LEVEL::PRIVATE, LEVEL::LOCAL, LEVEL::GLOBAL}) {
                    for (LEVEL first_complete_level : parent_levels(cb_size_level, false, ORDER::ASCENDING)) {
                        ss << resolve_helper(_def_cb_incomplete_size, kernel, cb_size_level, {dim_type, dim_nr}, first_complete_level) << std::endl;
                    }
                }
                ss << std::endl;
                ss << "// number of cache blocks in incomplete parent cache block per functional unit" << std::endl;
                for (LEVEL level : {LEVEL::PRIVATE, LEVEL::LOCAL}) {
                    for (LEVEL first_complete_level : parent_levels(parent_level(level), false, ORDER::ASCENDING)) {
                        ss << resolve_helper(_def_num_cb_in_incomplete_cb, kernel, level, {dim_type, dim_nr}, first_complete_level) << std::endl;
                    }
                }
                ss << std::endl;
                ss << "// number of extra cache blocks in incomplete parent cache block" << std::endl;
                for (LEVEL level : {LEVEL::PRIVATE, LEVEL::LOCAL}) {
                    for (LEVEL first_complete_level : parent_levels(parent_level(level), false, ORDER::ASCENDING)) {
                        ss << resolve_helper(_def_num_extra_cb_in_incomplete_cb, kernel, level, {dim_type, dim_nr}, first_complete_level) << std::endl;
                    }
                }
                ss << std::endl;
                ss << "// number of extra elements in incomplete parent cache block" << std::endl;
                for (LEVEL level : {LEVEL::PRIVATE, LEVEL::LOCAL}) {
                    for (LEVEL first_complete_level : parent_levels(parent_level(level), false, ORDER::ASCENDING)) {
                        ss << resolve_helper(_def_num_extra_elems_in_incomplete_cb, kernel, level, {dim_type, dim_nr}, first_complete_level) << std::endl;
                    }
                }
                ss << std::endl;
                ss << "// cache block offsets" << std::endl;
                for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
                    ss << resolve_helper(_def_cb_offset, kernel, level, {dim_type, dim_nr}) << std::endl;
                }

                first_dim = false;
            }
        }

        ss << std::endl;
        ss << std::endl;
        ss << "// -------------------- combined over dimensions --------------------" << std::endl;
        ss << std::endl;
        ss << "// flat WI ids" << std::endl;
        for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
            ss << resolve_helper(_def_flat_wi_id, kernel, level) << std::endl;
        }
        ss << std::endl;
        ss << "// flat number of WIs" << std::endl;
        for (LEVEL level : {LEVEL::GLOBAL, LEVEL::LOCAL, LEVEL::PRIVATE}) {
            ss << resolve_helper(_def_flat_num_wi, kernel, level) << std::endl;
        }

        // erase last new line character
        return ss.str().erase(ss.str().length() - 1);
    }
private:
    // macro names for each kernel, level and dimension
    std::string _cb_size                            [2][3][L_DIMS + R_DIMS];
    std::string _fu_id                              [2][3][L_DIMS + R_DIMS];
    std::string _num_fu                             [2][3][L_DIMS + R_DIMS];
    std::string _num_cb                             [2][3][L_DIMS + R_DIMS];
    std::string _num_extra_cb                       [2][3][L_DIMS + R_DIMS];
    std::string _num_extra_elems                    [2][3][L_DIMS + R_DIMS];
    std::string _num_cb_in_incomplete_cb            [2][3][L_DIMS + R_DIMS][3];
    std::string _num_extra_cb_in_incomplete_cb      [2][3][L_DIMS + R_DIMS][3];
    std::string _num_extra_elems_in_incomplete_cb   [2][3][L_DIMS + R_DIMS][3];
    std::string _cb_incomplete_size                 [2][3][L_DIMS + R_DIMS][3];
    std::string _cb_offset                          [2][3][L_DIMS + R_DIMS];
    // macro names for each kernel and level
    std::string _flat_wi_id [2][3];
    std::string _flat_num_wi[2][3];

    // macro definitions for each kernel, level and dimension
    std::string _def_cb_size                            [2][3][L_DIMS + R_DIMS];
    std::string _def_fu_id                              [2][3][L_DIMS + R_DIMS];
    std::string _def_num_fu                             [2][3][L_DIMS + R_DIMS];
    std::string _def_num_cb                             [2][3][L_DIMS + R_DIMS];
    std::string _def_num_extra_cb                       [2][3][L_DIMS + R_DIMS];
    std::string _def_num_extra_elems                    [2][3][L_DIMS + R_DIMS];
    std::string _def_num_cb_in_incomplete_cb            [2][3][L_DIMS + R_DIMS][3];
    std::string _def_num_extra_cb_in_incomplete_cb      [2][3][L_DIMS + R_DIMS][3];
    std::string _def_num_extra_elems_in_incomplete_cb   [2][3][L_DIMS + R_DIMS][3];
    std::string _def_cb_incomplete_size                 [2][3][L_DIMS + R_DIMS][3];
    std::string _def_cb_offset                          [2][3][L_DIMS + R_DIMS];
    // macro definitions for each kernel and level
    std::string _def_flat_wi_id [2][3];
    std::string _def_flat_num_wi[2][3];

    std::string& resolve_helper(std::string (&data)[2][3][L_DIMS + R_DIMS],
                                unsigned int kernel, LEVEL level, dimension_t dimension) {
        return data[kernel - 1][LEVEL_ID(level)][CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dimension)];
    }
    const std::string& resolve_helper(const std::string (&data)[2][3][L_DIMS + R_DIMS],
                                      unsigned int kernel, LEVEL level, dimension_t dimension) const {
        return data[kernel - 1][LEVEL_ID(level)][CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dimension)];
    }

    std::string& resolve_helper(std::string (&data)[2][3], unsigned int kernel, LEVEL level) {
        return data[kernel - 1][LEVEL_ID(level)];
    }
    const std::string& resolve_helper(const std::string (&data)[2][3], unsigned int kernel, LEVEL level) const {
        return data[kernel - 1][LEVEL_ID(level)];
    }

    std::string& resolve_helper(std::string (&data)[2][3][L_DIMS + R_DIMS][3],
                                unsigned int kernel, LEVEL level, dimension_t dimension, LEVEL first_complete_level) {
        return data[kernel - 1][LEVEL_ID(level)][CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dimension)][LEVEL_ID(first_complete_level)];
    }
    const std::string& resolve_helper(const std::string (&data)[2][3][L_DIMS + R_DIMS][3],
                                      unsigned int kernel, LEVEL level, dimension_t dimension, LEVEL first_complete_level) const {
        return data[kernel - 1][LEVEL_ID(level)][CONTINUOUS_DIM_ID<L_DIMS, R_DIMS>(dimension)][LEVEL_ID(first_complete_level)];
    }
};

}
}

#endif //MD_BLAS_MACROS_HPP
