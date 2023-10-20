#include <plugins/pluginStructs.h>
#include <utils/subset.hpp>
#include <stdexcept>
#include <utils/uda_type_sizes.hpp>
// #include <utils/print_uda_structs.hpp>

namespace subset
{

    /*
     * comvert SUBSET block from the request_block on the plugin_interface structure
     * into a vector of SubsetInfo classes for use with the current implementation of 
     * the multidimensional subset function.
     */
    std::vector<SubsetInfo> subset_info_converter(const SUBSET& datasubset, const DATA_BLOCK* data_block)
    {
        if (datasubset.nbound != data_block->rank)
        {
            throw std::runtime_error("Number of subset dimensions specified must equal dimensions of data");
        }
        std::vector<SubsetInfo> result;
        for (unsigned int i=0; i< datasubset.nbound; ++i)
        {
            if (datasubset.dimid[i] > data_block->rank)
            {
                throw std::runtime_error("Specified subset dimension exceeds dimensions of data");
            }
            uint64_t dim_n = data_block->dims[datasubset.dimid[i]].dim_n;
            uint64_t start = datasubset.lbindex[i].init ? datasubset.lbindex[i].value : 0;
            uint64_t stop = datasubset.ubindex[i].init ? datasubset.ubindex[i].value : 0;
            int64_t stride = datasubset.stride[i].init ? datasubset.stride[i].value : 1;
            result.emplace_back(start, stop, stride, dim_n); 
        }
        return result;
    }

    /*
     * The input_id "factors" of each dim into a flattened buffer of multi-dimensional 
     * data is given by the product of the dimension lengths of all higher-frequency 
     * dimensions preceding it. These are constants that don't need to be recalculated
     * in each loop.
     *
     * i.e. for 3d data, index is: i + (Ni * j) + (Ni * Nj * k)
     * "index factors" here would be {1, Ni, (Ni * Nj)}
     */
    std::vector<unsigned int> get_index_factors(std::vector<unsigned int>& dim_sizes)
    {
        std::vector<unsigned int> factors = {1};
        for (unsigned int i=1; i<dim_sizes.size(); ++i)
        {
            factors.emplace_back(factors[i-1] * dim_sizes[i-1]);
        }
        return factors;
    }

    /*
     * Get the linear input_id into a flattened buffer of multi-dimensional data
     * given the current index of each dimension and the previosly calculated
     * index factors.
     *
     * e.g. for 3d data: input_id = i + (Ni * j) + (Ni * Nj * k)
     * this is the dot-product of the vectors of position-index and input_id-factors 
     * for each data dimension.
     *
     */ 
    unsigned int get_input_offset(std::vector<unsigned int>& current_indices, std::vector<unsigned int>& index_factors)
    {
        unsigned int result = 0;
        for (unsigned int i=0; i<current_indices.size(); ++i)
        {
            result += current_indices[i] * index_factors[i];
        }
        return result;
    }

    /*
     * Subset a flattened buffer of multidimensional data and apply optional scale and offset factors
     *
     * avoids recursion
     */
    template<typename T>
    std::vector<T> subset(std::vector<T>& input, std::vector<SubsetInfo>& subset_dims, double scale_factor, double offset)
    {
        unsigned int result_length = 1;
        std::vector<unsigned int> total_dim_lengths;
        std::vector<unsigned int> current_indices;
        for (const auto& subset_info: subset_dims)
        {
            result_length *= subset_info.size();
            total_dim_lengths.emplace_back(subset_info.dim_size());
            current_indices.emplace_back(subset_info.start());
        }

        std::vector<unsigned int> factors = get_index_factors(total_dim_lengths);
        std::vector<T> result(result_length);
        for (unsigned int output_id=0; output_id<result_length; ++output_id)
        {
            current_indices[0] += subset_dims[0].stride();

            // increment vector of current_indices (cascading when they roll-over)
            for (unsigned int k=0; k<subset_dims.size() and current_indices[k] > subset_dims[k].stop(); ++k)
            {
                current_indices[k] = subset_dims[k].start();
                if (k < subset_dims.size() - 1)
                {
                    current_indices[k+1] += subset_dims[k].stride();
                }
                else 
                {
                    // something wrong !
                }
            }
            unsigned int input_id = get_input_offset(current_indices, factors);
            result[output_id] = (input[input_id] * scale_factor) + offset;
        }
        return result;
    }

    template<typename T>
    void do_a_subset(IDAM_PLUGIN_INTERFACE* plugin_interface, double scale_factor, double offset)
    {
        DATA_BLOCK* data_block = plugin_interface->data_block;
        size_t bytes_size = data_block->data_n * uda_type_utils::size_of_uda_type(data_block->data_type);
        std::vector<T> data_in(data_block->data, data_block->data + bytes_size); 

        auto subset_dims = subset_info_converter(plugin_interface->request_data->datasubset, data_block);
        auto transformed_data = subset(data_in, subset_dims, scale_factor, offset);

        free((void*) data_block->data);
        data_block->data_n = transformed_data.size();
        data_block->data = (char*) malloc(data_block->data_n * sizeof(T));
        std::copy(transformed_data.data(), transformed_data.data() + data_block->data_n * sizeof(T), data_block->data);
    }

    void apply_subsetting(IDAM_PLUGIN_INTERFACE* plugin_interface, double scale_factor, double offset)
    {
       switch(plugin_interface->data_block->data_type)
    {
        case UDA_TYPE_SHORT:
            do_a_subset<short>(plugin_interface, scale_factor, offset);
        case UDA_TYPE_INT:
            do_a_subset<int>(plugin_interface, scale_factor, offset);
        case UDA_TYPE_LONG:
            do_a_subset<long>(plugin_interface, scale_factor, offset);
        case UDA_TYPE_LONG64:
            do_a_subset<int64_t>(plugin_interface, scale_factor, offset);
        case UDA_TYPE_UNSIGNED_SHORT:
            do_a_subset<unsigned short>(plugin_interface, scale_factor, offset);
        case UDA_TYPE_UNSIGNED_INT:
            do_a_subset<unsigned int>(plugin_interface, scale_factor, offset);
        case UDA_TYPE_UNSIGNED_LONG:
            do_a_subset<unsigned long>(plugin_interface, scale_factor, offset);
        case UDA_TYPE_UNSIGNED_LONG64:
            do_a_subset<uint64_t>(plugin_interface, scale_factor, offset);
        case UDA_TYPE_FLOAT:
            do_a_subset<float>(plugin_interface, scale_factor, offset);
        case UDA_TYPE_DOUBLE:
            do_a_subset<double>(plugin_interface, scale_factor, offset);
        default:
            throw std::runtime_error(std::string("uda type ") + std::to_string(type_enum) + " not implemented for json_imas_mapping cache");
    }
 
    }

}
