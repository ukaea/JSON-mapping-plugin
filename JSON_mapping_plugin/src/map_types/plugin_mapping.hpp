#pragma once

#include "base_mapping.hpp"
#include <optional>
#include <unordered_map>

using MapArgs_t = std::unordered_map<std::string, nlohmann::json>;

class PluginMapping : public Mapping {
  public:
    PluginMapping() = delete;
    PluginMapping(std::string plugin, MapArgs_t request_args, std::optional<float> offset, std::optional<float> scale)
        : m_plugin{std::move(plugin)}, m_map_args{std::move(request_args)}, m_offset{offset}, m_scale{scale} {};

    [[nodiscard]] int map(const MapArguments& arguments) const override;

  private:
    std::string m_plugin;
    MapArgs_t m_map_args;
    std::optional<float> m_offset;
    std::optional<float> m_scale;

    [[nodiscard]] std::string get_request_str(const MapArguments& arguments) const;
    [[nodiscard]] int call_plugins(const MapArguments& arguments) const;
};
