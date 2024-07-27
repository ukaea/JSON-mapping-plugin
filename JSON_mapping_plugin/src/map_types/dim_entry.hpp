#pragma once

#include "map_types/base_entry.hpp"

class DimEntry : public Mapping {
  public:
    DimEntry() = delete;
    explicit DimEntry(std::string dim_probe)
        : m_dim_probe{std::move(dim_probe)} {};

    int map(IDAM_PLUGIN_INTERFACE* interface,
            const std::unordered_map<std::string, std::unique_ptr<Mapping>>&
                entries,
            const nlohmann::json& json_globals) const override;

  private:
    std::string m_dim_probe;
};
