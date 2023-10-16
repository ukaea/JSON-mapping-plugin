#include "map_types/dim_mapping.hpp"
#include "map_types/base_mapping.hpp"
#include <clientserver/udaStructs.h>

int DimMapping::map(const MapArguments& arguments) const {

    if (arguments.m_entries.count(m_dim_probe) == 0) {
        return 1;
    }

    int err = arguments.m_entries.at(m_dim_probe)->map(arguments);
    if (err == 0) {
        free((void*)arguments.m_interface->data_block->data); // fix
        arguments.m_interface->data_block->data = nullptr;
        if (arguments.m_interface->data_block->data_n == 0) {
            UDA_LOG(UDA_LOG_DEBUG, "\nDimMapping::map: Dim probe could not be used for Shape_of \n");
            return 1;
        }
        err = setReturnDataIntScalar(arguments.m_interface->data_block, arguments.m_interface->data_block->data_n, nullptr);
    }
    return err;
}
