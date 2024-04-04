#pragma once

#include "base_entry.hpp"
#include <optional>
#include <unordered_map>

enum class PluginType { UDA, GEOMETRY, JSONReader };

NLOHMANN_JSON_SERIALIZE_ENUM(PluginType,
                             {{PluginType::UDA, "UDA"},
                              {PluginType::GEOMETRY, "GEOM"},
                              {PluginType::JSONReader, "DRaFT_JSON"}});

using MapArgs_t = std::unordered_map<std::string, nlohmann::json>;
class MapEntry : public Mapping {
  public:
    MapEntry() = delete;
    MapEntry(std::pair<PluginType, std::string> plugin, MapArgs_t request_args,
             std::optional<float> offset, std::optional<float> scale)
        : _plugin{std::move(plugin)}, _map_args{std::move(request_args)},
          _offset{offset}, _scale{scale} {};

    int map(IDAM_PLUGIN_INTERFACE* interface,
            const std::unordered_map<std::string, std::unique_ptr<Mapping>>&
                entries,
            const nlohmann::json& json_globals) const override;

  private:
    std::pair<PluginType, std::string> _plugin;
    MapArgs_t _map_args;
    std::optional<float> _offset;
    std::optional<float> _scale;

    [[nodiscard]] std::string
    get_request_str(const nlohmann::json& json_globals) const;
    int call_plugins(IDAM_PLUGIN_INTERFACE* interface,
                     const nlohmann::json& json_globals) const;
};
