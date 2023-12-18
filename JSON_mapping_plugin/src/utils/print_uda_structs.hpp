#pragma once

#include <clientserver/udaStructs.h>
#include <clientserver/udaTypes.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace uda_structs
{
    template<typename T>
    inline std::string print_typed_buffer(T* data, int max_size)
    {
        std::stringstream ss;
        ss << "[";
        int max_elements = std::min(max_size, 10);
        bool all_data =  max_elements == max_size;
        if (!all_data)
        {
            max_elements /= 2;
        }
        for (unsigned int i = 0; i < max_elements; ++i)
        {
           ss << data[i] << ","; 
        }
        if (!all_data)
        {
            ss << " ... ";
            for (unsigned int i = max_size-max_elements-1; i < max_size; ++i)
            {
                ss << data[i] << ","; 
            } 
        }
        ss << "]" << std::endl;
        return ss.str();
    }

    inline std::string print_uda_data_buffer(char* data, int data_type, int max_size)
    {
        switch (data_type)
        {
            case UDA_TYPE_INT:
                return print_typed_buffer((int*) data, max_size);
            case UDA_TYPE_SHORT:
                return print_typed_buffer((short*) data, max_size);
            case UDA_TYPE_LONG:
                return print_typed_buffer((long*) data, max_size);
            case UDA_TYPE_UNSIGNED_INT:
                return print_typed_buffer((unsigned int*) data, max_size);
            case UDA_TYPE_UNSIGNED_SHORT:
                return print_typed_buffer((unsigned short*) data, max_size);
            case UDA_TYPE_UNSIGNED_LONG:
                return print_typed_buffer((unsigned long*) data, max_size);
            case UDA_TYPE_FLOAT:
                return print_typed_buffer((float*) data, max_size);
            case UDA_TYPE_DOUBLE:
                return print_typed_buffer((double*) data, max_size);
            default:
                throw std::runtime_error("unknown type " + std::to_string(data_type));
        }
    }

    inline std::string print_request_data(REQUEST_DATA* request_data)
    {
        std::stringstream ss;

        ss << "request     : " << request_data->request << std::endl;
        ss << "exp_number  : " << request_data->exp_number << std::endl;
        ss << "pass        : " << request_data->pass << std::endl;
        ss << "tpass       : " << request_data->tpass << std::endl;
        ss << "path        : " << request_data->path << std::endl;
        ss << "file        : " << request_data->file << std::endl;
        ss << "format      : " << request_data->format << std::endl;
        ss << "archive     : " << request_data->archive << std::endl;
        ss << "device_name : " << request_data->device_name << std::endl;
        ss << "server      : " << request_data->server << std::endl;
        ss << "function    : " << request_data->function << std::endl;
        ss << "signal      : " << request_data->signal << std::endl;
        ss << "source      : " << request_data->source << std::endl;
        ss << "api_delim   : " << request_data->api_delim << std::endl;
        ss << "subset      : " << request_data->subset << std::endl;
        ss << "subsetCount : " << request_data->datasubset.nbound << std::endl;
        for (int i = 0; i < request_data->datasubset.nbound; i++) 
        {
            ss << "subset dim: " << i << std::endl;
            ss << "\t" << "dimid: " << request_data->datasubset.dimid[i] << std::endl;
            ss << "\t" << "lower bound: " << request_data->datasubset.lbindex[i].value << std::endl;
            ss << "\t" << "upper index: " << request_data->datasubset.ubindex[i].value << std::endl;
            ss << "\t" << "stride: " << request_data->datasubset.stride[i].value << std::endl;
        }
        ss << "nameValueCount : " << request_data->nameValueList.pairCount << std::endl;
        for (int i = 0; i < request_data->nameValueList.pairCount; i++) 
        {
            ss << i << ", " << request_data->nameValueList.nameValue[i].pair << ", " <<
                    request_data->nameValueList.nameValue[i].name << ", " << request_data->nameValueList.nameValue[i].value << std::endl;
        }

        return ss.str();
    }

    inline std::string print_data_block(DATA_BLOCK* data_block)
    {
        std::stringstream ss;
        ss <<  "handle       : " << data_block->handle << std::endl;
        ss <<  "error code   : " << data_block->errcode << std::endl;
        ss <<  "error msg    : " << data_block->error_msg << std::endl;
        ss <<  "source status: " << data_block->source_status << std::endl;
        ss <<  "signal status: " << data_block->signal_status << std::endl;
        ss <<  "data_number  : " << data_block->data_n << std::endl;
        ss <<  "rank         : " << data_block->rank << std::endl;
        ss <<  "order        : " << data_block->order << std::endl;
        ss <<  "data_type    : " << data_block->data_type << std::endl;
        ss <<  "error_type   : " << data_block->error_type << std::endl;
        ss <<  "errhi != nullptr: " << (data_block->errhi != nullptr) << std::endl;
        ss <<  "errlo != nullptr: " << (data_block->errlo != nullptr) << std::endl;

        ss <<  "opaque_type : " << data_block->opaque_type << std::endl;
        ss <<  "opaque_count: " << data_block->opaque_count << std::endl; 

        ss <<  "error model : " << data_block->error_model << std::endl;
        ss <<  "asymmetry   : " << data_block->errasymmetry << std::endl;
        ss <<  "error model no. params : " << data_block->error_param_n << std::endl;

        ss <<  "data_units  : " << data_block->data_units << std::endl;
        ss <<  "data_label  : " << data_block->data_label << std::endl;
        ss <<  "data_desc   : " << data_block->data_desc << std::endl;

        if (data_block->data_type != UDA_TYPE_CAPNP)
        {
            ss <<  "data        : " << print_uda_data_buffer(data_block->data,  data_block->data_type, data_block->data_n);
        }
        else 
        {
            ss << "data is capnp buffer" << std::endl;
        }
        
        for(unsigned int i=0; i < data_block->rank; ++i)
        {
            DIMS* dim = &data_block->dims[i];
            
            ss << "DIM BLOCK " << i << std::endl;
            ss << "\tdata_number : " << dim->dim_n << std::endl;
            ss << "\tdata_type   : " << dim->data_type << std::endl;
            ss << "\tcompressed? : " << dim->compressed << std::endl;
            ss << "\tmethod      : " << dim->method << std::endl;
            ss << "\tstarting val: " << dim->dim0 << std::endl;
            ss << "\tstepping val: " << dim->diff << std::endl;
            ss << "\tdata_units  : " << dim->dim_units << std::endl;
            ss << "\tdata_label  : " << dim->dim_label << std::endl;

            if (dim->dim != nullptr)
            {
                ss <<  "\tdim " << i << " data : " << print_uda_data_buffer(dim->dim,  dim->data_type, dim->dim_n);
            }
        }

        return ss.str();
    }

}
