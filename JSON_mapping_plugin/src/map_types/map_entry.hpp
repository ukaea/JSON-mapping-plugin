#pragma once

#include "base_entry.hpp"
#include <optional>
#include <unordered_map>

enum class PluginType { UDA, GEOMETRY, CUSTOM, JSONReader };

NLOHMANN_JSON_SERIALIZE_ENUM(PluginType,
                             {{PluginType::UDA, "UDA"},
                              {PluginType::GEOMETRY, "GEOMETRY"},
                              {PluginType::JSONReader, "JSONReader"}});

using MapArgs_t = std::unordered_map<std::string, std::string>;
class MapEntry : public Mapping {
  public:
    MapEntry() = delete;
    MapEntry(std::pair<PluginType, std::string> plugin, MapArgs_t request_args,
             std::optional<float> offset, std::optional<float> scale)
        : m_plugin{std::move(plugin)}, m_map_args{std::move(request_args)},
          m_offset{offset}, m_scale{scale} {};

    int map(IDAM_PLUGIN_INTERFACE* interface,
            const std::unordered_map<std::string, std::unique_ptr<Mapping>>&
                entries,
            const nlohmann::json& json_globals) const override;

  private:
    std::pair<PluginType, std::string> m_plugin;
    MapArgs_t m_map_args;
    std::optional<float> m_offset;
    std::optional<float> m_scale;

    [[nodiscard]] std::string
    get_request_string(const nlohmann::json& json_globals) const;
    int call_plugins(IDAM_PLUGIN_INTERFACE* interface,
                     const nlohmann::json& json_globals) const;
};
