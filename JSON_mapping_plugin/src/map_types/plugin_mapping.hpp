#pragma once

#include "base_mapping.hpp"
#include <optional>
#include <unordered_map>

enum class PluginType { UDA, GEOMETRY, JSONReader };

NLOHMANN_JSON_SERIALIZE_ENUM(PluginType, {{PluginType::UDA, "UDA"},
                                          {PluginType::GEOMETRY, "GEOM"},
                                          {PluginType::JSONReader, "DRaFT_JSON"}});

using MapArgs_t = std::unordered_map<std::string, nlohmann::json>;
class PluginMapping : public Mapping {
  public:
    PluginMapping() = delete;
    PluginMapping(std::pair<PluginType, std::string> plugin, MapArgs_t request_args, std::optional<float> offset,
             std::optional<float> scale)
        : m_plugin{std::move(plugin)}, m_map_args{std::move(request_args)}, m_offset{offset}, m_scale{scale} {};

    int map(const MapArguments& arguments) const override;

  private:
    std::pair<PluginType, std::string> m_plugin;
    MapArgs_t m_map_args;
    std::optional<float> m_offset;
    std::optional<float> m_scale;

    [[nodiscard]] std::string get_request_str(const MapArguments& arguments) const;
    int call_plugins(const MapArguments& arguments) const;
};
