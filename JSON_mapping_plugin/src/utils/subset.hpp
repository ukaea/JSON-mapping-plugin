#include <cstdint>
#include <plugins/pluginStructs.h>
#include <vector>
#include <cmath>
#include <clientserver/parseXML.h>
#include <clientserver/udaStructs.h>

namespace subset
{

    class SubsetInfo
    {
        private:
        uint64_t _start;
        uint64_t _stop;
        int64_t _stride = 1;
        uint64_t _dim_size;

        public:
        inline explicit SubsetInfo(uint64_t size): 
            _start(0), _stop(size), _dim_size(size)
        {}

        inline SubsetInfo(uint64_t start, uint64_t stop, int stride, uint64_t size)
            : _start(start), _stop(stop), _stride(stride), _dim_size(size)
        {}

        [[nodiscard]] inline uint64_t size() const
        {
            return std::floor((_stop - _start) / _stride);
        }

        [[nodiscard]] inline bool validate() const
        {
            return _stop <= _dim_size and _stride < _dim_size;
        }

        [[nodiscard]] inline uint64_t start() const
        {
            return _start;
        }

        [[nodiscard]] inline uint64_t stop() const
        {
            return _stop;
        }

        [[nodiscard]] inline int64_t stride() const
        {
            return _stride;
        }

        [[nodiscard]] inline uint64_t dim_size() const
        {
            return _dim_size;
        }
    };

    void apply_subsetting(IDAM_PLUGIN_INTERFACE* plugin_interface, double scale_factor, double offset);

    std::vector<SubsetInfo> subset_info_converter(const SUBSET& datasubset, const DATA_BLOCK* data_block);

    template<typename T>
    std::vector<T> subset(std::vector<T>& input, std::vector<SubsetInfo>& subset_dims, double scale_factor=1.0, double offset=0.0);

}
