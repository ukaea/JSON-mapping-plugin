#include "map_types/base_entry.hpp"

class SliceEntry : public Mapping {
  public:
    SliceEntry() = delete;
    SliceEntry(std::vector<std::string> slice_indices, std::string slice_key)
        : _slice_indices(std::move(slice_indices)),
          _slice_key(std::move(slice_key)) {}

    int map(IDAM_PLUGIN_INTERFACE* interface,
            const std::unordered_map<std::string, std::unique_ptr<Mapping>>&
                entries,
            const nlohmann::json& json_globals) const override;

  private:
    std::vector<std::string> _slice_indices;
    std::string _slice_key;

    int map_slice(DataBlock* data_block, const nlohmann::json& json_globals)
        const; // const for some reason
    template <typename T>
    std::valarray<T> get_slice2D(std::valarray<T>&& orig, int slice_index,
                                 std::pair<int, int> shape) const;
};

template <typename T>
std::valarray<T> SliceEntry::get_slice2D(std::valarray<T>&& orig,
                                         int slice_index,
                                         std::pair<int, int> shape) const {

    // Replace with templated strings like {{indices.0}}
    return orig[std::slice(slice_index, shape.first, shape.second)];
}
