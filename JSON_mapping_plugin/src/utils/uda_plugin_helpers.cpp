#include <helpers/uda_plugin_helpers.hpp>

namespace imas_json_plugin::uda_helpers {

int setReturnTimeArray(DATA_BLOCK* data_block) {

    // Retrieve index of the time block
    const auto time_dim = data_block->order;

    if (data_block->dims[0].compressed) {
        uncompressDim(&data_block->dims[0]);
    }
    data_block->rank = 1;
    data_block->order = -1;
    free((void*)data_block->data);

    data_block->data = data_block->dims[time_dim].dim;
    data_block->data_n = data_block->dims[time_dim].dim_n;
    data_block->data_type =
        data_block->dims[time_dim].data_type; // set to dims type
    strcpy(data_block->data_units, data_block->dims[time_dim].dim_units);
    strcpy(data_block->data_label, data_block->dims[time_dim].dim_label);

    // Cleanup to make things behave
    data_block->dims[time_dim].dim = nullptr;
    data_block->dims[time_dim].data_type = UDA_TYPE_UNSIGNED_INT;
    data_block->dims[time_dim].compressed = 1;
    data_block->dims[time_dim].method = 0;
    data_block->dims[time_dim].dim0 = 0.0;
    data_block->dims[time_dim].diff = 1.0;

    return 0;
};

}; // namespace imas_json_plugin::uda_helpers
