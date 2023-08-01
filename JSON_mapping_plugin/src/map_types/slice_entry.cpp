#include "map_types/slice_entry.hpp"
#include "utils/uda_plugin_helpers.hpp"
#include <plugins/udaPlugin.h>
#include <inja/inja.hpp>
#include <algorithm>

int SliceEntry::map(
    IDAM_PLUGIN_INTERFACE* interface,
    const std::unordered_map<std::string, std::unique_ptr<Mapping>>& entries,
    const nlohmann::json& json_globals) const {

    int err{1};
    if (!entries.at(m_slice_key)->set_current_request_data(&interface->request_data->nameValueList)
        && !entries.at(m_slice_key)->map(interface, entries, json_globals)) {
        err = map_slice(interface->data_block, json_globals);
    }
    return err;
};

int SliceEntry::map_slice(DataBlock* data_block, const nlohmann::json& json_globals) const {

    int len_array{data_block->data_n/data_block->dims->dim_n};
    int err{1};

    // convert str_indices to int and complete template
    std::vector<int> int_indices;
    std::transform(m_slice_indices.begin(), m_slice_indices.end(), std::back_inserter(int_indices),
    [&](std::string str) {
        str = inja::render(str, json_globals);
        return stoi(str);
    });
    if (data_block->rank == 2) {
        // test case with float
        const auto temp = get_slice2D<float>(
                {reinterpret_cast<float*>(data_block->data), static_cast<size_t>(data_block->data_n)},
                int_indices.at(0), 
                {len_array, data_block->dims->dim_n});

        free((void*)data_block->data); 
        data_block->data = nullptr;
        err = imas_json_plugin::uda_helpers::setReturnDataValArray(data_block, temp);
    }

    return err;
}
