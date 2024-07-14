#pragma once

/*#include <cstdlib>*/
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "map_types/base_mapping.hpp"
#include "utils/ram_cache.hpp"
#include <nlohmann/json.hpp>

using IDSName = std::string;
using MachineName = std::string;
using MappingName = std::string;

using IDSMapRegister = std::unordered_map<MappingName, std::unique_ptr<Mapping>>;
using IDSMapRegisterStore = std::unordered_map<IDSName, IDSMapRegister>;
using IDSGlobalsStore = std::unordered_map<IDSName, nlohmann::json>;

struct MachineMapping {
    IDSMapRegisterStore mappings;
    IDSGlobalsStore attributes;
};

using MachineRegisterStore = std::unordered_map<MachineName, MachineMapping>;
using MappingPair = std::pair<nlohmann::json&, IDSMapRegister&>;

class MappingHandler
{

  public:
    MappingHandler() : m_init(false), m_dd_version("3.39.0") {};
    explicit MappingHandler(std::string dd_version) : m_init(false), m_dd_version(std::move(dd_version)) {};

    int reset();
    int init();
    int set_map_dir(const std::string& mapping_dir);
    std::optional<MappingPair> read_mappings(const MachineName& machine, const std::string& request_ids);

  private:
    std::string mapping_path(const MachineName& machine, const IDSName& ids_name, const std::string& file_name);
    int load_machine(const MachineName& machine);
    nlohmann::json load_toplevel(const MachineName& machine);
    int load_globals(const MachineName& machine, const IDSName& ids_name);
    int load_mappings(const MachineName& machine, const IDSName& ids_name);

    int init_mappings(const MachineName& machine, const IDSName& ids_name, const nlohmann::json& data);
    static int init_value_mapping(IDSMapRegister& map_reg, const std::string& key, const nlohmann::json& value);
    static int init_plugin_mapping(IDSMapRegister& map_reg, const std::string& key, const nlohmann::json& value,
                                   const nlohmann::json& ids_attributes,
                                   std::shared_ptr<ram_cache::RamCache>& ram_cache);
    static int init_dim_mapping(IDSMapRegister& map_reg, const std::string& key, const nlohmann::json& value);
    static int init_slice_mapping(IDSMapRegister& map_reg, const std::string& key, const nlohmann::json& value);
    static int init_expr_mapping(IDSMapRegister& map_reg, const std::string& key, const nlohmann::json& value);
    static int init_custom_mapping(IDSMapRegister& map_reg, const std::string& key, const nlohmann::json& value);

    MachineRegisterStore m_machine_register;
    bool m_init;

    std::string m_dd_version;
    std::string m_mapping_dir;
    nlohmann::json m_mapping_config;
    std::shared_ptr<ram_cache::RamCache> m_ram_cache;
    bool m_cache_enabled;
};
