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

inline std::unordered_map<std::string, UDA_TYPE> UDA_TYPE_MAP{
    {typeid(unsigned int).name(), UDA_TYPE_UNSIGNED_INT},
    {typeid(int).name(), UDA_TYPE_INT},
    {typeid(float).name(), UDA_TYPE_FLOAT},
    {typeid(double).name(), UDA_TYPE_DOUBLE}};

int setReturnTimeArray(DATA_BLOCK* data_block);

template <typename T>
int setReturnDataScalarType(DATA_BLOCK* data_block, T value,
                            const char* description = nullptr) {

    initDataBlock(data_block);

    T* data = static_cast<T*>(malloc(sizeof(T)));
    data[0] = value;

    if (description != nullptr) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    data_block->rank = 0;
    data_block->data_type = UDA_TYPE_MAP.at(typeid(T).name());
    data_block->data = reinterpret_cast<char*>(data);
    data_block->data_n = 1;

    return 0;
};

template <typename T>
int setReturnDataArrayType(DATA_BLOCK* data_block, gsl::span<const T> values,
                           gsl::span<const size_t> shape,
                           const char* description = nullptr) {

    initDataBlock(data_block);

    if (description != nullptr) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    const auto rank{shape.size()};
    data_block->rank = rank;
    data_block->dims = (DIMS*)malloc(rank * sizeof(DIMS));

    size_t len = 1;

    for (size_t i = 0; i < rank; ++i) {
        initDimBlock(&data_block->dims[i]);

        int shape_i = shape[i];
        data_block->dims[i].data_type = UDA_TYPE_UNSIGNED_INT;

        data_block->dims[i].dim_n = shape_i;

        // Always setting the dim to compressed initial value and spacing
        data_block->dims[i].compressed = 1;
        data_block->dims[i].dim0 = 0.0;
        data_block->dims[i].diff = 1.0;
        data_block->dims[i].method = 0;

        len *= shape_i;
    }

    T* data = static_cast<T*>(malloc(len * sizeof(T)));
    std::copy(values.begin(), values.end(), data);

    data_block->data_type = UDA_TYPE_MAP.at(typeid(T).name());
    data_block->data = reinterpret_cast<char*>(data);
    data_block->data_n = (int)len; // Not ideal....

    return 0;
};

template <typename T>
int setReturnDataArrayType_Vec(DATA_BLOCK* data_block,
                               const std::vector<T>& vec_values,
                               const char* description = nullptr) {

    initDataBlock(data_block);

    if (description != nullptr) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    const auto vec_size{vec_values.size()};
    data_block->rank = 1;
    data_block->dims = (DIMS*)malloc(1 * sizeof(DIMS));

    initDimBlock(&data_block->dims[0]);

    data_block->dims[0].data_type = UDA_TYPE_UNSIGNED_INT;

    data_block->dims[0].dim_n = vec_size;

    // Always setting the dim to compressed initial value and spacing
    data_block->dims[0].compressed = 1;
    data_block->dims[0].dim0 = 0.0;
    data_block->dims[0].diff = 1.0;
    data_block->dims[0].method = 0;

    T* data = static_cast<T*>(malloc(vec_size * sizeof(T)));
    std::copy(vec_values.begin(), vec_values.end(), data);

    data_block->data_type = UDA_TYPE_MAP.at(typeid(T).name());
    data_block->data = reinterpret_cast<char*>(data);
    data_block->data_n = (int)vec_size;

    return 0;
};

template <typename T>
int setReturnDataValArray(DATA_BLOCK* data_block,
                          const std::valarray<T>& va_values,
                          const char* description = nullptr) {

    initDataBlock(data_block);

    if (description != nullptr) {
        strncpy(data_block->data_desc, description, STRING_LENGTH);
        data_block->data_desc[STRING_LENGTH - 1] = '\0';
    }

    const auto va_size{va_values.size()};
    data_block->rank = 1;
    data_block->dims = (DIMS*)malloc(1 * sizeof(DIMS));

    initDimBlock(&data_block->dims[0]);

    data_block->dims[0].data_type = UDA_TYPE_UNSIGNED_INT;

    data_block->dims[0].dim_n = va_size;

    // Always setting the dim to compressed initial value and spacing
    data_block->dims[0].compressed = 1;
    data_block->dims[0].dim0 = 0.0;
    data_block->dims[0].diff = 1.0;
    data_block->dims[0].method = 0;

    T* data = static_cast<T*>(malloc(va_size * sizeof(T)));
    std::copy(std::begin(va_values), std::end(va_values), data);

    data_block->data_type = UDA_TYPE_MAP.at(typeid(T).name());
    data_block->data = reinterpret_cast<char*>(data);
    data_block->data_n = (int)va_size;

    return 0;
}

}; // namespace imas_json_plugin::uda_helpers
