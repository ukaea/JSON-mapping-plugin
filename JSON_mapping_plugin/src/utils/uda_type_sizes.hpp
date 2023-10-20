#pragma once

#include <clientserver/udaTypes.h>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace uda_type_utils
{
inline size_t size_of_uda_type(int type_enum)
{
    switch(type_enum)
    {
        case UDA_TYPE_SHORT:
            return sizeof(short);
        case UDA_TYPE_INT:
            return sizeof(int);
        case UDA_TYPE_LONG:
            return sizeof(long);
        case UDA_TYPE_LONG64:
            return sizeof(int64_t);
        case UDA_TYPE_UNSIGNED_SHORT:
            return sizeof(unsigned short);
        case UDA_TYPE_UNSIGNED_INT:
            return sizeof(unsigned int);
        case UDA_TYPE_UNSIGNED_LONG:
            return sizeof(unsigned long);
        case UDA_TYPE_UNSIGNED_LONG64:
            return sizeof(uint64_t);
        case UDA_TYPE_FLOAT:
            return sizeof(float);
        case UDA_TYPE_DOUBLE:
            return sizeof(double);
        default:
            throw std::runtime_error(std::string("uda type ") + std::to_string(type_enum) + " not implemented for json_imas_mapping cache");
    }
}
}
