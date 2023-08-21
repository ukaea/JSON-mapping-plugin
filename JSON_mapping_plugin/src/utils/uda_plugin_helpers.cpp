#include "utils/uda_plugin_helpers.hpp"

namespace imas_json_plugin::uda_helpers {

int setReturnTimeArray(DATA_BLOCK* data_block) {

    // Retrieve index of the time block
    const auto time_dim = data_block->order;

    auto dims = gsl::span(data_block->dims, data_block->rank);

    if (dims[0].compressed != 0) {
        uncompressDim(dims.data());
    }
    data_block->rank = 1;
    data_block->order = -1;
    free((void*)data_block->data);

    data_block->data = dims[time_dim].dim;
    data_block->data_n = dims[time_dim].dim_n;
    data_block->data_type = dims[time_dim].data_type; // set to dims type
    strncpy(data_block->data_units, dims[time_dim].dim_units, STRING_LENGTH);
    strncpy(data_block->data_label, dims[time_dim].dim_label, STRING_LENGTH);

    // Cleanup to make things behave
    dims[time_dim].dim = nullptr;
    dims[time_dim].data_type = UDA_TYPE_UNSIGNED_INT;
    dims[time_dim].compressed = 1;
    dims[time_dim].method = 0;
    dims[time_dim].dim0 = 0.0;
    dims[time_dim].diff = 1.0;

    return 0;
}

} // namespace imas_json_plugin::uda_helpers
