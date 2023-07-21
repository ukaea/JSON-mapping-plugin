#pragma once

#include "base_entry.hpp"
#include <optional>

class MapEntry : public Mapping { 
public:
    MapEntry() = delete;
    MapEntry(PluginType plugin, std::string key, std::optional<std::string> var) 
        : m_plugin{plugin}, m_key{key}, m_var{var} {};
    int map(IDAM_PLUGIN_INTERFACE* interface, const std::unordered_map<std::string, 
            std::unique_ptr<Mapping>>& entries, const nlohmann::json& json_globals) const override;

protected:
    std::string m_key;
    std::optional<std::string> m_var;
    PluginType m_plugin;

    std::string get_request(const nlohmann::json& json_globals) const;
    int call_plugins(IDAM_PLUGIN_INTERFACE* interface, const nlohmann::json& json_globals) const;
};
