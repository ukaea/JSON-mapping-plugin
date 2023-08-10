#include "map_types/slice_mapping.hpp"
#include "utils/uda_plugin_helpers.hpp"
#include <algorithm>
#include <inja/inja.hpp>
#include <plugins/udaPlugin.h>

int SliceMapping::map(const MapArguments& arguments) const {

    int err = arguments.m_entries.at(m_slice_key)->map(arguments);
    if (err == 0) {
        err = map_slice(arguments.m_interface->data_block, arguments.m_global_data);
    }
    return err;
};

int SliceMapping::map_slice(DataBlock* data_block, const nlohmann::json& json_globals) const {

    int len_array{data_block->data_n / data_block->dims->dim_n};
    int err{1};

    // convert str_indices to int and complete template
    std::vector<int> int_indices;
    std::transform(m_slice_indices.begin(), m_slice_indices.end(),
                   std::back_inserter(int_indices), [&](std::string str) {
                       str = inja::render(str, json_globals);
                       return stoi(str);
                   });
    if (data_block->rank == 2) {
        // test case with float
        const auto temp = get_slice2D<float>(
            {reinterpret_cast<float*>(data_block->data),
             static_cast<size_t>(data_block->data_n)},
            int_indices.at(0), {len_array, data_block->dims->dim_n});

        free((void*)data_block->data);
        data_block->data = nullptr;
        err = imas_json_plugin::uda_helpers::setReturnDataValArray(data_block,
                                                                   temp);
    }

    return err;
}
