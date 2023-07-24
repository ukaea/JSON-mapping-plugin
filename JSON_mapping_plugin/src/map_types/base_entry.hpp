#pragma once

#include <clientserver/udaStructs.h>
#include <helpers/uda_plugin_helpers.hpp>
#include <nlohmann/json.hpp>
#include <plugins/pluginStructs.h>
#include <plugins/udaPlugin.h>

enum class MapTransfos { VALUE, MAP, OFFSET, SCALE, EXPR, DIM };

NLOHMANN_JSON_SERIALIZE_ENUM(MapTransfos, {{MapTransfos::VALUE, "VALUE"},
                                           {MapTransfos::MAP, "MAP"},
                                           {MapTransfos::OFFSET, "OFFSET"},
                                           {MapTransfos::SCALE, "SCALE"},
                                           {MapTransfos::EXPR, "EXPR"},
                                           {MapTransfos::DIM, "DIMENSION"}});

enum class PluginType { UDA, GEOMETRY, JSONReader };

NLOHMANN_JSON_SERIALIZE_ENUM(PluginType,
                             {{PluginType::UDA, "UDA"},
                              {PluginType::GEOMETRY, "GEOMETRY"},
                              {PluginType::JSONReader, "JSONReader"}});

enum class SignalType { DEFAULT, DATA, TIME, ERROR, DIM, INVALID };

class Mapping {
  public:
    Mapping() = default;
    virtual ~Mapping() = default;
    virtual int map(IDAM_PLUGIN_INTERFACE* interface,
                    const std::unordered_map<std::string,
                                             std::unique_ptr<Mapping>>& entries,
                    const nlohmann::json& global_data) const = 0;
    int set_current_request_data(NAMEVALUELIST* nvlist);
    int set_sig_type(SignalType sig_type) {
        m_request_data.sig_type = sig_type;
        return 0;
    };

  protected:
    struct RequestStruct {
        std::string host;
        int port;
        int shot;
        std::vector<int> indices;
        SignalType sig_type;
    };
    RequestStruct m_request_data;
};

class ValueEntry : public Mapping {
  public:
    ValueEntry() = delete;
    ~ValueEntry() override = default;
    explicit ValueEntry(nlohmann::json value) : m_value{std::move(value)} {};
    int map(IDAM_PLUGIN_INTERFACE* interface,
            const std::unordered_map<std::string, std::unique_ptr<Mapping>>&
                entries,
            const nlohmann::json& global_data) const override;

  private:
    nlohmann::json m_value;
    int type_deduc_array(DATA_BLOCK* data_block,
                         const nlohmann::json& arrValue) const;
    int type_deduc_prim(DATA_BLOCK* data_block, const nlohmann::json& numValue,
                        const nlohmann::json& global_data) const;
};
