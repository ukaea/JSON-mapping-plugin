#pragma once

#include <cstring>
#include <typeinfo>
#include <unordered_map>
#include <valarray>
#include <vector>

#include <clientserver/compressDim.h>
#include <clientserver/initStructs.h>
#include <clientserver/udaTypes.h>
#include <gsl/gsl-lite.hpp>

namespace imas_json_plugin::uda_helpers {

inline std::unordered_map<std::string, UDA_TYPE> uda_type_map() {
    static std::unordered_map<std::string, UDA_TYPE> type_map;
    if (type_map.empty()) {
        type_map = {{typeid(unsigned int).name(), UDA_TYPE_UNSIGNED_INT},
                    {typeid(int).name(), UDA_TYPE_INT},
                    {typeid(float).name(), UDA_TYPE_FLOAT},
                    {typeid(double).name(), UDA_TYPE_DOUBLE}};
    }
    return type_map;
}

int setReturnTimeArray(DATA_BLOCK* data_block);

template <typename T> int setReturnDataScalarType(DATA_BLOCK* data_block, T value, const char* description = nullptr) {

    initDataBlock(data_block);

    auto data = static_cast<T*>(malloc(sizeof(T)));
    *data = value;

    if (description != nullptr) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    data_block->rank = 0;
    data_block->data_type = uda_type_map().at(typeid(T).name());
    data_block->data = reinterpret_cast<char*>(data);
    data_block->data_n = 1;

    return 0;
}

template <typename T>
int setReturnDataArrayType(DATA_BLOCK* data_block, gsl::span<const T> values, gsl::span<const size_t> shape,
                           const char* description = nullptr) {

    initDataBlock(data_block);

    if (description != nullptr) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    const auto rank{shape.size()};
    data_block->rank = rank;
    data_block->dims = static_cast<DIMS*>(malloc(rank * sizeof(DIMS)));

    auto dims = gsl::span(data_block->dims, data_block->rank);

    size_t len = 1;

    for (size_t i = 0; i < rank; ++i) {
        initDimBlock(&dims[i]);

        int const shape_i = static_cast<int>(shape[i]);
        dims[i].data_type = UDA_TYPE_UNSIGNED_INT;

        dims[i].dim_n = shape_i;

        // Always setting the dim to compressed initial value and spacing
        dims[i].compressed = 1;
        dims[i].dim0 = 0.0;
        dims[i].diff = 1.0;
        dims[i].method = 0;

        len *= shape_i;
    }

    auto data = static_cast<T*>(malloc(len * sizeof(T)));
    std::copy(values.begin(), values.end(), data);

    data_block->data_type = uda_type_map().at(typeid(T).name());
    data_block->data = reinterpret_cast<char*>(data);
    data_block->data_n = (int)len; // Not ideal....

    return 0;
}

template <typename T>
int setReturnDataArrayType_Vec(DATA_BLOCK* data_block, const std::vector<T>& vec_values,
                               const char* description = nullptr) {

    initDataBlock(data_block);

    if (description != nullptr) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    const auto vec_size{vec_values.size()};
    data_block->rank = 1;
    data_block->dims = static_cast<DIMS*>(malloc(1 * sizeof(DIMS)));

    auto dims = gsl::span(data_block->dims, data_block->rank);

    initDimBlock(dims.data());

    dims[0].data_type = UDA_TYPE_UNSIGNED_INT;

    dims[0].dim_n = vec_size;

    // Always setting the dim to compressed initial value and spacing
    dims[0].compressed = 1;
    dims[0].dim0 = 0.0;
    dims[0].diff = 1.0;
    dims[0].method = 0;

    T* data = static_cast<T*>(malloc(vec_size * sizeof(T)));
    std::copy(vec_values.begin(), vec_values.end(), data);

    data_block->data_type = uda_type_map().at(typeid(T).name());
    data_block->data = reinterpret_cast<char*>(data);
    data_block->data_n = (int)vec_size;

    return 0;
}

template <typename T>
int setReturnDataValArray(DATA_BLOCK* data_block, const std::valarray<T>& va_values,
                          const char* description = nullptr) {

    initDataBlock(data_block);

    if (description != nullptr) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    const auto va_size{va_values.size()};
    data_block->rank = 1;
    data_block->dims = static_cast<DIMS*>(malloc(1 * sizeof(DIMS)));

    auto dims = gsl::span(data_block->dims, data_block->rank);

    initDimBlock(dims.data());

    dims[0].data_type = UDA_TYPE_UNSIGNED_INT;

    dims[0].dim_n = va_size;

    // Always setting the dim to compressed initial value and spacing
    dims[0].compressed = 1;
    dims[0].dim0 = 0.0;
    dims[0].diff = 1.0;
    dims[0].method = 0;

    T* data = static_cast<T*>(malloc(va_size * sizeof(T)));
    std::copy(std::begin(va_values), std::end(va_values), data);

    data_block->data_type = uda_type_map().at(typeid(T).name());
    data_block->data = reinterpret_cast<char*>(data);
    data_block->data_n = (int)va_size;

    return 0;
}

} // namespace imas_json_plugin::uda_helpers
