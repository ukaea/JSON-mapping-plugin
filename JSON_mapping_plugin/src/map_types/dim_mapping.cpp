#include "map_types/dim_mapping.hpp"
#include "map_types/base_mapping.hpp"
#include <clientserver/udaStructs.h>

int DimMapping::map(const MapArguments& arguments) const {

    if (arguments.m_entries.count(m_dim_probe) == 0) {
        return 1;
    }

    // SignalType needs to be changed to DIM to avoid slicing
    // Temporary hack to change const arguments
    //
    // Alternative would be to have an _extra entry in the mapping
    // file sans slicing
    MapArguments temp_map_args = arguments;
    temp_map_args.m_sig_type = SignalType::DIM;

    const int err = arguments.m_entries.at(m_dim_probe)->map(temp_map_args);
    if (err == 0) {
        free((void*)arguments.m_interface->data_block->data); // fix
        arguments.m_interface->data_block->data = nullptr;
        if (arguments.m_interface->data_block->data_n == 0) {
            UDA_LOG(UDA_LOG_DEBUG, "\nDimMapping::map: Dim probe could not be used for Shape_of \n");
            return 1;
        }
        setReturnDataIntScalar(arguments.m_interface->data_block, arguments.m_interface->data_block->data_n, nullptr);
    }
    return err;
}
