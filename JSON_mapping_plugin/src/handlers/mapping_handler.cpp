#include "mapping_handler.hpp"

#include <boost/algorithm/string.hpp>
#include <inja/inja.hpp>
#include <logging/logging.h>
#include <unordered_map>

#include "map_types/custom_mapping.hpp"
#include "map_types/dim_mapping.hpp"
#include "map_types/expr_mapping.hpp"
#include "map_types/plugin_mapping.hpp"
#include "map_types/value_mapping.hpp"

std::optional<MappingPair> MappingHandler::read_mappings(const MachineName_t& machine, const std::string& request_ids) {
    load_machine(machine);
    if (m_machine_register.count(machine) == 0) {
        return {};
    }
    auto& [mappings, attributes] = m_machine_register[machine];
    if (mappings.count(request_ids) == 0 || attributes.count(request_ids) == 0) {
        return {};
    }
    // AJP :: Safety check if ids request not in mapping json (and typo obviously)
    return std::optional<MappingPair>{
        std::make_pair(std::ref(attributes[request_ids]), std::ref(mappings[request_ids]))};
}

int MappingHandler::set_map_dir(const std::string& mapping_dir) {
    m_mapping_dir = mapping_dir;
    return 0;
}

std::string MappingHandler::mapping_path(const MachineName_t& machine, const IDSName_t& ids_name,
                                         const std::string& file_name) {
    if (ids_name.empty()) {
        return m_mapping_dir + "/" + machine + "/" + file_name;
    }
    return m_mapping_dir + "/" + machine + "/" + ids_name + "/" + file_name;
}

int MappingHandler::load_machine(const MachineName_t& machine) {
    if (m_machine_register.count(machine) == 1) {
        // machine already loaded
        return 0;
    }

    auto file_path = mapping_path(machine, "", "mappings.cfg.json");

    std::ifstream map_cfg_file(file_path);
    if (map_cfg_file) {
        map_cfg_file >> m_mapping_config;
    } else {
        RAISE_PLUGIN_ERROR("MappingHandler::load_configs - Cannot open JSON mapping config file")
    }

    m_machine_register[machine] = {{}, {}};

    for (const auto& ids_name : m_mapping_config[m_dd_version].get<std::vector<std::string>>()) {
        load_globals(machine, ids_name);
        load_mappings(machine, ids_name);
    }

    return 0;
}

nlohmann::json MappingHandler::load_toplevel(const MachineName_t& machine) {

    auto file_path = mapping_path(machine, "", "globals.json");

    nlohmann::json toplevel_globals;

    std::ifstream globals_file;
    globals_file.open(file_path);
    if (globals_file) {
        try {
            globals_file >> toplevel_globals;
        } catch (nlohmann::json::exception& ex) {
            std::string json_error{"MappingHandler::load_globals - "};
            json_error.append(ex.what());
            RAISE_PLUGIN_ERROR(json_error.c_str())
        }

    } else {
        RAISE_PLUGIN_ERROR("MappingHandler::load_globals- Cannot open top-level globals file")
    }
    return toplevel_globals;
}

int MappingHandler::load_globals(const MachineName_t& machine, const IDSName_t& ids_name) {

    auto file_path = mapping_path(machine, ids_name, "globals.json");

    std::ifstream globals_file;
    globals_file.open(file_path);
    if (globals_file) {
        nlohmann::json temp_globals;
        try {
            globals_file >> temp_globals;
        } catch (nlohmann::json::exception& ex) {
            std::string json_error{"MappingHandler::load_globals - "};
            json_error.append(ex.what());
            RAISE_PLUGIN_ERROR(json_error.c_str())
        }

        temp_globals.update(load_toplevel(machine));
        m_machine_register[machine].attributes[ids_name] = temp_globals; // Record globals

    } else {
        RAISE_PLUGIN_ERROR("MappingHandler::load_globals - Cannot open JSON globals file")
    }
    return 0;
}

int MappingHandler::load_mappings(const MachineName_t& machine, const IDSName_t& ids_name) {

    auto file_path = mapping_path(machine, ids_name, "mappings.json");

    std::ifstream map_file;
    map_file.open(file_path);
    if (map_file) {
        nlohmann::json temp_mappings;
        try {
            map_file >> temp_mappings;
        } catch (nlohmann::json::exception& ex) {
            std::string json_error{"MappingHandler::load_mappings - "};
            json_error.append(ex.what());
            RAISE_PLUGIN_ERROR(json_error.c_str())
        }

        init_mappings(machine, ids_name, temp_mappings);
    } else {
        RAISE_PLUGIN_ERROR("MappingHandler::load_mappings - Cannot open JSON mapping file")
    }
    return 0;
}

int MappingHandler::init_value_mapping(IDSMapRegister_t& map_reg, const std::string& key, const nlohmann::json& value) {
    const auto& value_json = value.at("VALUE");
    map_reg.try_emplace(key, std::make_unique<ValueMapping>(value_json));
    return 0;
}

namespace {

void apply_config(std::unordered_map<std::string, nlohmann::json>& args, std::optional<std::string>& function,
                  nlohmann::json plugin_config_map, const std::string& plugin_name) {
    if (plugin_config_map.contains(plugin_name)) {
        const auto& plugin_config = plugin_config_map[plugin_name].get<nlohmann::json>();
        const auto& plugin_args = plugin_config["ARGS"].get<nlohmann::json>();
        for (const auto& [name, arg] : plugin_args.items()) {
            if (args.count(name) == 0) {
                // don't overwrite mapping arguments with global values
                args[name] = arg;
            }
        }
        if (plugin_config.contains("FUNCTION") && !function) {
            function = plugin_config["FUNCTION"].get<std::string>();
        }
    }
}

std::optional<float> get_float_value(const std::string& name, const nlohmann::json& value,
                                     const nlohmann::json& ids_attributes) {
    std::optional<float> opt_float{std::nullopt};
    if (value.contains(name) and !value[name].is_null()) {
        if (value[name].is_number()) {
            opt_float = value[name].get<float>();
        } else if (value[name].is_string()) {
            try {
                const auto post_inja_str = inja::render(value[name].get<std::string>(), ids_attributes);
                opt_float = std::stof(post_inja_str);
            } catch (const std::invalid_argument& e) {
                std::string message = "\nCannot convert " + name + " string to float\n";
                UDA_LOG(UDA_LOG_DEBUG, "%s", message.c_str());
            }
        }
    }
    return opt_float;
}

} // anon namespace

int MappingHandler::init_plugin_mapping(IDSMapRegister_t& map_reg, const std::string& key, const nlohmann::json& value,
                                        const nlohmann::json& ids_attributes, std::shared_ptr<ram_cache::RamCache>& ram_cache) {
    auto plugin_name = value["PLUGIN"].get<std::string>();
    boost::to_upper(plugin_name);

    auto args = value["ARGS"].get<MapArgs_t>();
    auto offset = get_float_value("OFFSET", value, ids_attributes);
    auto scale = get_float_value("SCALE", value, ids_attributes);
    auto slice = value.contains("SLICE") ? std::optional<std::string>{value["SLICE"].get<std::string>()}
                                         : std::optional<std::string>{};
    auto function = value.contains("FUNCTION") ? std::optional<std::string>{value["FUNCTION"].get<std::string>()}
                                               : std::optional<std::string>{};
  
    if (ids_attributes.contains("PLUGIN_CONFIG")) {
        const auto& plugin_config_map = ids_attributes["PLUGIN_CONFIG"].get<nlohmann::json>();
        apply_config(args, function, plugin_config_map, plugin_name);
    }

    map_reg.try_emplace(key, std::make_unique<PluginMapping>(plugin_name, args, offset, scale, slice, function, ram_cache));
    return 0;
}

int MappingHandler::init_dim_mapping(IDSMapRegister_t& map_reg, const std::string& key, const nlohmann::json& value) {
    map_reg.try_emplace(key, std::make_unique<DimMapping>(value["DIM_PROBE"].get<std::string>()));
    return 0;
}

int MappingHandler::init_expr_mapping(IDSMapRegister_t& map_reg, const std::string& key, const nlohmann::json& value) {
    map_reg.try_emplace(
        key, std::make_unique<ExprMapping>(value["EXPR"].get<std::string>(),
                                           value["PARAMETERS"].get<std::unordered_map<std::string, std::string>>()));
    return 0;
}

int MappingHandler::init_custom_mapping(IDSMapRegister_t& map_reg, const std::string& key, const nlohmann::json& value) {
    map_reg.try_emplace(key, std::make_unique<CustomMapping>(value["CUSTOM_TYPE"].get<CustomMapType_t>()));
    return 0;
}

int MappingHandler::init_mappings(const MachineName_t& machine, const IDSName_t& ids_name, const nlohmann::json& data) {
    const auto& attributes = m_machine_register[machine].attributes;
    IDSMapRegister_t temp_map_reg;
    for (const auto& [key, value] : data.items()) {

        switch (value["MAP_TYPE"].get<MappingType>()) {
        case MappingType::VALUE:
            init_value_mapping(temp_map_reg, key, value);
            break;
        case MappingType::PLUGIN:
            init_plugin_mapping(temp_map_reg, key, value, attributes.at(ids_name), m_ram_cache);
            break;
        case MappingType::DIM:
            init_dim_mapping(temp_map_reg, key, value);
            break;
        case MappingType::EXPR:
            init_expr_mapping(temp_map_reg, key, value);
            break;
        case MappingType::CUSTOM:
            init_custom_mapping(temp_map_reg, key, value);
            break;
        default:
            break;
        }
    }

    m_machine_register[machine].mappings.try_emplace(ids_name, std::move(temp_map_reg));
    UDA_LOG(UDA_LOG_DEBUG, "calling read function \n");

    return 0;
}
