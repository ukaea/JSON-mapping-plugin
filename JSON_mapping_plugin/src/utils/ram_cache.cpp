#include "ram_cache.hpp"
#include "clientserver/initStructs.h"
#include "temp_geom_plugin/utils/uda_plugin_helpers.hpp"
#include <clientserver/udaTypes.h>
#include <iterator>
#include <stdexcept>
#include <utils/uda_type_sizes.hpp>

namespace ram_cache
{
    using uda_type_utils::size_of_uda_type;

    std::shared_ptr<DataEntry> RamCache::make_data_entry(DATA_BLOCK* data_block)
    {
        log_datablock_status(data_block, "data_block before caching");

        auto data_entry = std::make_shared<DataEntry>();
        size_t byte_length = data_block->data_n * size_of_uda_type(data_block->data_type);
        std::copy(data_block->data, data_block->data + byte_length, std::back_inserter(data_entry->data));
        for (unsigned int i=0; i < data_block->rank; ++i)
        {   
            std::vector<char> dim_vals;
            auto dim = data_block->dims[i];

            // expand any compressed dims for caching.
            if (dim.compressed != 0)
            {
                uncompressDim(&dim);
                dim.compressed = 0;
                dim.method = 0;
                free(dim.sams);
                free(dim.offs);
                free(dim.ints);
                dim.udoms = 0;
                dim.sams = nullptr;
                dim.offs = nullptr;
                dim.ints = nullptr;
            }

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


    bool RamCache::copy_from_cache(std::string key, DATA_BLOCK* data_block)
    {
        auto it = std::find(_keys.begin(), _keys.end(), key); 
        if (it == _keys.end())
        {
            return false;
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
        return true;
    }

}
