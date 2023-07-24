#include "map_types/offset_entry.hpp"

int OffsetEntry::map(
    IDAM_PLUGIN_INTERFACE* interface,
    const std::unordered_map<std::string, std::unique_ptr<Mapping>>& entries,
    const nlohmann::json& json_globals) const {

    // Retrieve data
    int err = call_plugins(interface, json_globals);
    if (err) {
        return 1;
    }

    if (m_request_data.sig_type == SignalType::DEFAULT or
        m_request_data.sig_type == SignalType::DATA) {
        err = transform(interface);
    }
    return err;
};

int OffsetEntry::transform(IDAM_PLUGIN_INTERFACE* interface) const {

    int err{0};
    if (interface->data_block->rank > 0) {
        const size_t array_size(interface->data_block->data_n);
        switch (interface->data_block->data_type) {
        case UDA_TYPE_SHORT: {
            auto* data = reinterpret_cast<short*>(interface->data_block->data);
            err = offset_span(gsl::span{data, array_size});
            break;
        }
        case UDA_TYPE_INT: {
            auto* data = reinterpret_cast<int*>(interface->data_block->data);
            err = offset_span(gsl::span{data, array_size});
            break;
        }
        case UDA_TYPE_LONG: {
            auto* data = reinterpret_cast<long*>(interface->data_block->data);
            err = offset_span(gsl::span{data, array_size});
            break;
        }
        case UDA_TYPE_FLOAT: {
            auto* data = reinterpret_cast<float*>(interface->data_block->data);
            err = offset_span(gsl::span{data, array_size});
            break;
        }
        case UDA_TYPE_DOUBLE: {
            auto* data = reinterpret_cast<double*>(interface->data_block->data);
            err = offset_span(gsl::span{data, array_size});
            break;
        }
        default:
            UDA_LOG(UDA_LOG_DEBUG,
                    "\nOffsetEntry::transform(...) Unrecognised type\n");
            return 1;
        }
    } else {
        switch (interface->data_block->data_type) {
        case UDA_TYPE_SHORT: {
            auto* data = reinterpret_cast<short*>(interface->data_block->data);
            err = offset(data);
            break;
        }
        case UDA_TYPE_INT: {
            auto* data = reinterpret_cast<int*>(interface->data_block->data);
            err = offset(data);
            break;
        }
        case UDA_TYPE_LONG: {
            auto* data = reinterpret_cast<long*>(interface->data_block->data);
            err = offset(data);
            break;
        }
        case UDA_TYPE_FLOAT: {
            auto* data = reinterpret_cast<float*>(interface->data_block->data);
            err = offset(data);
            break;
        }
        case UDA_TYPE_DOUBLE: {
            auto* data = reinterpret_cast<double*>(interface->data_block->data);
            err = offset(data);
            break;
        }
        default:
            UDA_LOG(UDA_LOG_DEBUG,
                    "\nOffsetEntry::transform(...) Unrecognised type\n");
            return 1;
        }
    }

    return err;
}
