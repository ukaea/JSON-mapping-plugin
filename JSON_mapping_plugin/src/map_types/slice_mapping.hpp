#pragma once

#include "map_types/base_mapping.hpp"

class SliceMapping : public Mapping {
  public:
    SliceMapping() = delete;
    SliceMapping(std::vector<std::string> slice_indices, std::string slice_key)
        : m_slice_indices(std::move(slice_indices)), m_slice_key(std::move(slice_key)) {}

    [[nodiscard]] int map(const MapArguments& arguments) const override;

  private:
    std::vector<std::string> m_slice_indices;
    std::string m_slice_key;

    int map_slice(DataBlock* data_block, const nlohmann::json& json_globals) const; // const for some reason
    template <typename T>
    std::valarray<T> get_slice2D(std::valarray<T>&& orig, int slice_index, std::pair<int, int> shape) const;
};

template <typename T>
std::valarray<T> SliceMapping::get_slice2D(std::valarray<T>&& orig, int slice_index, std::pair<int, int> shape) const {
    // Replace with templated strings like {{indices.0}}
    return orig[std::slice(slice_index, shape.first, shape.second)];
}
