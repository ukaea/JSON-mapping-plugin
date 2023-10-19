#include "ram_cache.hpp"
#include "clientserver/initStructs.h"
#include <clientserver/udaTypes.h>
#include <iterator>
#include <stdexcept>
namespace ram_cache
{

    size_t size_of_uda_type(int type_enum)
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

    template<typename T>
    std::string print_typed_buffer(T* data, int max_size)
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

    std::string print_uda_data_buffer(char* data, int data_type, int max_size)
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

    void log_datablock_status(DATA_BLOCK* data_block, std::string message)
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

        ss <<  "data        : " << print_uda_data_buffer(data_block->data,  data_block->data_type, data_block->data_n);
        
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

        log(LogLevel::DEBUG, message + "\n" + ss.str());
    }

    std::shared_ptr<DataEntry> RamCache::make_data_entry(DATA_BLOCK* data_block)
    {
        log_datablock_status(data_block, "data_block before caching");

        auto data_entry = std::make_shared<DataEntry>();
        size_t byte_length = data_block->data_n * size_of_uda_type(data_block->data_type);
        std::copy(data_block->data, data_block->data + byte_length, std::back_inserter(data_entry->data));
        for (unsigned int i=0; i < data_block->rank; ++i)
        {   
            // if not compressed
            std::vector<char> dim_vals;
            auto dim = data_block->dims[i];
            size_t byte_length = dim.dim_n * size_of_uda_type(dim.data_type);
            std::copy(dim.dim, dim.dim + byte_length, std::back_inserter(dim_vals));
            data_entry->dims.emplace_back(dim_vals);
            data_entry->dim_types.emplace_back(dim.data_type);
        }
        data_entry->order = data_block->order;
        data_entry->data_type = data_block->data_type;
        

//        std::copy(data_block->errhi, data_block->errhi + data_block->data_n, std::back_inserter(data_entry->error_high));
  //      std::copy(data_block->errlo, data_block->errlo + data_block->data_n, std::back_inserter(data_entry->error_high));
        data_entry->error_type = data_block->error_type;

        return data_entry;
    }

    std::optional<DATA_BLOCK*> RamCache::copy_from_cache(std::string key, DATA_BLOCK* data_block)
    {
        auto it = std::find(_keys.begin(), _keys.end(), key); 
        if (it == _keys.end())
        {
            return {};
        }
        log(LogLevel::INFO, "key found in ramcache: \"" + key + "\". copying data out");

        auto index = it - _keys.begin();
        std::shared_ptr<DataEntry> data_entry = _values[index];

        // DATA_BLOCK* data_block = (DATA_BLOCK*) malloc(sizeof(DATA_BLOCK));
        initDataBlock(data_block);
        data_block->data_type = data_entry->data_type;
        data_block->data_n = data_entry->data.size() / size_of_uda_type(data_entry->data_type);

        log(LogLevel::INFO, "data size is: " + std::to_string(data_block->data_n));

        data_block->data = (char*) malloc(data_entry->data.size());
        std::copy(data_entry->data.data(), data_entry->data.data() + data_entry->data.size(), data_block->data);
  //      if (data_entry->error_high.size())
  //      {
  //          data_block->errhi = (char*) malloc(data_block->data_n);
  //          std::copy(data_entry->error_high.data(), data_entry->error_high.data() + data_entry->error_high.size(), data_block->errhi);
  //      }
  //      if (data_entry->error_low.size())
  //      {
  //          data_block->errlo = (char*) malloc(data_block->data_n);
  //          std::copy(data_entry->error_low.data(), data_entry->error_low.data() + data_entry->error_low.size(), data_block->errlo);
  //      }
        data_block->rank = data_entry->dims.size();
        log(LogLevel::INFO, "data rank is: " + std::to_string(data_block->rank));

        DIMS* dims = (DIMS*) malloc (data_block->rank * sizeof(DIMS));
        for (unsigned int i=0; i < data_block->rank; ++i)
        {
            initDimBlock(&dims[i]);

            dims[i].data_type = data_entry->dim_types[i];
            dims[i].dim_n = data_entry->dims[i].size() / size_of_uda_type(dims[i].data_type);

            log(LogLevel::INFO, "dim " + std::to_string(i) + " length: " + std::to_string(dims[i].dim_n));

            dims[i].dim = (char*) malloc(data_entry->dims[i].size());
            std::copy(data_entry->dims[i].data(), data_entry->dims[i].data() + data_entry->dims[i].size(), dims[i].dim);

        }
        data_block->dims = dims;
        data_block->order = data_entry->order; 

        log_datablock_status(data_block, "data_block from cache");
        return data_block;
    }

}
