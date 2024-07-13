#pragma once

#include "map_types/base_mapping.hpp"

class DimMapping : public Mapping
{
  public:
    DimMapping() = delete;
    explicit DimMapping(std::string dim_probe) : m_dim_probe{std::move(dim_probe)} {};

    [[nodiscard]] int map(const MapArguments& arguments) const override;

  private:
    std::string m_dim_probe;
};
