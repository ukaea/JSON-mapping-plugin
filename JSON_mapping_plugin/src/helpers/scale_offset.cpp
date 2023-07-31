#include "helpers/scale_offset.hpp"
#include <clientserver/udaTypes.h>
#include <logging/logging.h>

namespace JMP::map_transform {

int transform_offset(DataBlock* data_block, float offset) {

    int err{1};
    if (data_block->rank > 0) {
        const size_t array_size(data_block->data_n);
        switch (data_block->data_type) {
        case UDA_TYPE_SHORT: {
            auto* data = reinterpret_cast<short*>(data_block->data);
            err = offset_span(gsl::span{data, array_size}, offset);
            break;
        }
        case UDA_TYPE_INT: {
            auto* data = reinterpret_cast<int*>(data_block->data);
            err = offset_span(gsl::span{data, array_size}, offset);
            break;
        }
        case UDA_TYPE_LONG: {
            auto* data = reinterpret_cast<long*>(data_block->data);
            err = offset_span(gsl::span{data, array_size}, offset);
            break;
        }
        case UDA_TYPE_FLOAT: {
            auto* data = reinterpret_cast<float*>(data_block->data);
            err = offset_span(gsl::span{data, array_size}, offset);
            break;
        }
        case UDA_TYPE_DOUBLE: {
            auto* data = reinterpret_cast<double*>(data_block->data);
            err = offset_span(gsl::span{data, array_size}, offset);
            break;
        }
        default:
            UDA_LOG(UDA_LOG_DEBUG,
                    "\nOffsetEntry::transform(...) Unrecognised type\n");
            return 1;
        }
    } else {
        switch (data_block->data_type) {
        case UDA_TYPE_SHORT: {
            auto* data = reinterpret_cast<short*>(data_block->data);
            err = offset_value(data, offset);
            break;
        }
        case UDA_TYPE_INT: {
            auto* data = reinterpret_cast<int*>(data_block->data);
            err = offset_value(data, offset);
            break;
        }
        case UDA_TYPE_LONG: {
            auto* data = reinterpret_cast<long*>(data_block->data);
            err = offset_value(data, offset);
            break;
        }
        case UDA_TYPE_FLOAT: {
            auto* data = reinterpret_cast<float*>(data_block->data);
            err = offset_value(data, offset);
            break;
        }
        case UDA_TYPE_DOUBLE: {
            auto* data = reinterpret_cast<double*>(data_block->data);
            err = offset_value(data, offset);
            break;
        }
        default:
            UDA_LOG(UDA_LOG_DEBUG,
                    "\nOffsetEntry::transform(...) Unrecognised type\n");
            break;
        }
    }

    return err;
}

int transform_scale(DataBlock* data_block, float scale) {

    int err{1};
    if (data_block->rank > 0) {
        const size_t array_size(data_block->data_n);
        switch (data_block->data_type) {
        case UDA_TYPE_SHORT: {
            auto* data = reinterpret_cast<short*>(data_block->data);
            err = scale_span(gsl::span{data, array_size}, scale);
            break;
        }
        case UDA_TYPE_INT: {
            auto* data = reinterpret_cast<int*>(data_block->data);
            err = scale_span(gsl::span{data, array_size}, scale);
            break;
        }
        case UDA_TYPE_LONG: {
            auto* data = reinterpret_cast<long*>(data_block->data);
            err = scale_span(gsl::span{data, array_size}, scale);
            break;
        }
        case UDA_TYPE_FLOAT: {
            auto* data = reinterpret_cast<float*>(data_block->data);
            err = scale_span(gsl::span{data, array_size}, scale);
            break;
        }
        case UDA_TYPE_DOUBLE: {
            auto* data = reinterpret_cast<double*>(data_block->data);
            err = scale_span(gsl::span{data, array_size}, scale);
            break;
        }
        default:
            UDA_LOG(UDA_LOG_DEBUG,
                    "\nOffsetEntry::transform(...) Unrecognised type\n");
            return 1;
        }
    } else {
        switch (data_block->data_type) {
        case UDA_TYPE_SHORT: {
            auto* data = reinterpret_cast<short*>(data_block->data);
            err = scale_value(data, scale);
            break;
        }
        case UDA_TYPE_INT: {
            auto* data = reinterpret_cast<int*>(data_block->data);
            err = scale_value(data, scale);
            break;
        }
        case UDA_TYPE_LONG: {
            auto* data = reinterpret_cast<long*>(data_block->data);
            err = scale_value(data, scale);
            break;
        }
        case UDA_TYPE_FLOAT: {
            auto* data = reinterpret_cast<float*>(data_block->data);
            err = scale_value(data, scale);
            break;
        }
        case UDA_TYPE_DOUBLE: {
            auto* data = reinterpret_cast<double*>(data_block->data);
            err = scale_value(data, scale);
            break;
        }
        default:
            UDA_LOG(UDA_LOG_DEBUG,
                    "\nOffsetEntry::transform(...) Unrecognised type\n");
            break;
        }
    }

    return err;
}
} // namespace JMP::map_transform
