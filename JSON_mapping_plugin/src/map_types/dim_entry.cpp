#include "map_types/dim_entry.hpp"
#include "map_types/base_entry.hpp"
#include <clientserver/udaStructs.h>

int DimEntry::map(IDAM_PLUGIN_INTERFACE* interface, 
        const std::unordered_map<std::string, std::unique_ptr<Mapping>>& entries, 
        const nlohmann::json& json_globals) const { 

    if (!entries.count(m_dim_probe)) { return 1; }
    // 0 if both successful
    int err = entries.at(m_dim_probe)->set_sig_type(SignalType::DIM)
            or entries.at(m_dim_probe)->map(interface, entries, json_globals);
    if (!err) {
        free((void*)interface->data_block->data); // fix
        interface->data_block->data = nullptr;
        if (!interface->data_block->data_n) {
            UDA_LOG(UDA_LOG_DEBUG, "\nDimEntry::map: Dim probe could not be used for Shape_of \n");
            return 1;
        }
        setReturnDataIntScalar(interface->data_block, interface->data_block->data_n, nullptr);
    }
    return err;
};
