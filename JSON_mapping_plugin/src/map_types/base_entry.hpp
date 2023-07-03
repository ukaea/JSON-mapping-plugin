#pragma once

#include <nlohmann/json.hpp>
#include <plugins/pluginStructs.h>
#include <plugins/udaPlugin.h>
#include <uda_plugin_helpers.hpp>

enum class MapTransfos { VALUE, MAP, OFFSET, SCALE, EXPR, DIM };

NLOHMANN_JSON_SERIALIZE_ENUM(MapTransfos, {{MapTransfos::VALUE, "VALUE"},
                                           {MapTransfos::MAP, "MAP"},
                                           {MapTransfos::OFFSET, "OFFSET"},
                                           {MapTransfos::SCALE, "SCALE"},
                                           {MapTransfos::EXPR, "EXPR"},
                                           {MapTransfos::DIM, "DIMENSION"}});

enum class PluginType { UDA, GEOMETRY };

NLOHMANN_JSON_SERIALIZE_ENUM(PluginType, {{PluginType::UDA, "UDA"},
                                          {PluginType::GEOMETRY, "GEOMETRY"}});

enum class SignalType { STANDARD, DATA, TIME, ERROR, DIM };

class Mapping {
  public:
    Mapping() = default;
    virtual ~Mapping() = default;
    virtual int
    map(IDAM_PLUGIN_INTERFACE* interface,
        const std::unordered_map<std::string, std::unique_ptr<Mapping>>&
            entries,
        const nlohmann::json& global_data, SignalType sig_type) const = 0;
};

class ValueEntry : public Mapping {
  public:
    ValueEntry() = delete;
    ~ValueEntry() override = default;
    explicit ValueEntry(nlohmann::json value) : m_value{std::move(value)} {};
    int map(IDAM_PLUGIN_INTERFACE* interface,
            const std::unordered_map<std::string, std::unique_ptr<Mapping>>&
                entries,
            const nlohmann::json& global_data,
            SignalType sig_type) const override;

  private:
    nlohmann::json m_value;
};
