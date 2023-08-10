#include "mapping_handler.hpp"

#include <inja/inja.hpp>
#include <logging/logging.h>
#include <unordered_map>

#include "map_types/custom_mapping.hpp"
#include "map_types/dim_mapping.hpp"
#include "map_types/expr_mapping.hpp"
#include "map_types/plugin_mapping.hpp"
#include "map_types/slice_mapping.hpp"
#include "map_types/value_mapping.hpp"

MappingPair MappingHandler::read_mappings(const std::string& machine, const std::string& request_ids) {
    // AJP :: Safety check if ids request not in mapping json (and typo
    // obviously)
    return std::make_pair(std::ref(m_ids_attributes[request_ids]), std::ref(m_ids_map_register[request_ids]));
}

int MappingHandler::set_map_dir(const std::string& mapping_dir) {
    m_mapping_dir = mapping_dir;
    return 0;
}

int MappingHandler::load_all() {

    std::ifstream map_cfg_file(m_mapping_dir + "/mappings.cfg.json");
    if (map_cfg_file) {
        map_cfg_file >> m_mapping_config;
        map_cfg_file.close();
    } else {
        RAISE_PLUGIN_ERROR("MappingHandler::load_configs - Cannot open JSON mapping config file");
    }

    for (const auto& ids_str : m_mapping_config[m_imas_version].get<std::vector<std::string>>()) {
        load_globals(ids_str);
        load_mappings(ids_str);
    }

    return 0;
}

int MappingHandler::load_globals(const std::string& ids_str) {

    std::string file_path{m_mapping_dir + "/mappings/" + ids_str + "/" + "globals.json"};

    std::ifstream globals_file;
    globals_file.open(file_path);
    if (globals_file) {
        nlohmann::json temp_globals;
        try {
            globals_file >> temp_globals;
        } catch (nlohmann::json::exception& ex) {
            globals_file.close();
            std::string json_error{"MappingHandler::load_globals - "};
            json_error.append(ex.what());
            RAISE_PLUGIN_ERROR(json_error.c_str());
        }
        globals_file.close();

        m_ids_attributes[ids_str] = temp_globals; // Record globals

    } else {
        RAISE_PLUGIN_ERROR("MappingHandler::load_globals- Cannot open JSON globals file");
    }
    return 0;
}

int MappingHandler::load_mappings(const std::string& ids_str) {

    std::string file_path{m_mapping_dir + "/mappings/" + ids_str + "/" + "mappings.json"};

    std::ifstream map_file;
    map_file.open(file_path);
    if (map_file) {
        nlohmann::json temp_mappings;
        try {
            map_file >> temp_mappings;
        } catch (nlohmann::json::exception& ex) {
            map_file.close();
            std::string json_error{"MappingHandler::load_mappings - "};
            json_error.append(ex.what());
            RAISE_PLUGIN_ERROR(json_error.c_str());
        }
        map_file.close();

        init_mappings(ids_str, temp_mappings);
    } else {
        RAISE_PLUGIN_ERROR("MappingHandler::load_mappings - Cannot open JSON mapping file");
    }
    return 0;
}

int MappingHandler::init_mappings(const std::string& ids_name, const nlohmann::json& data) {

    IDSMapRegister_t temp_map_reg;
    for (const auto& [key, value] : data.items()) {

        switch (value["MAP_TYPE"].get<MappingType>()) {
        case MappingType::VALUE: {
            temp_map_reg.try_emplace(key, std::make_unique<ValueMapping>(ValueMapping(value["VALUE"])));
            break;
        }
        case MappingType::PLUGIN: {
            // Structured bindings lambda capture bug, json passed as argument
            auto get_offset_scale = [&](const std::string& var_str, nlohmann::json value_local) {
                std::optional<float> opt_float{std::nullopt};
                if (value_local.contains(var_str) and !value_local[var_str].is_null()) {
                    if (value_local[var_str].is_number_float()) {
                        opt_float = value_local[var_str].get<float>();
                    } else if (value_local[var_str].is_string()) {
                        try {
                            const auto post_inja_str =
                                inja::render(value_local[var_str].get<std::string>(), m_ids_attributes[ids_name]);
                            opt_float = std::stof(post_inja_str);
                        } catch (const std::invalid_argument& e) {
                            UDA_LOG(UDA_LOG_DEBUG, "\nCannot convert OFFSET/SCALE string to float\n");
                        }
                    }
                }
                return opt_float;
            };
            temp_map_reg.try_emplace(
                key, std::make_unique<PluginMapping>(PluginMapping(
                         std::make_pair(value["PLUGIN"].get<PluginType>(), value["PLUGIN"].get<std::string>()),
                         value["ARGS"].get<MapArgs_t>(), get_offset_scale("OFFSET", value),
                         get_offset_scale("SCALE", value))));
            break;
        }
        case MappingType::DIM: {
            temp_map_reg.try_emplace(key,
                                     std::make_unique<DimMapping>(DimMapping(value["DIM_PROBE"].get<std::string>())));
            break;
        }
        case MappingType::SLICE: {
            temp_map_reg.try_emplace(
                key, std::make_unique<SliceMapping>(SliceMapping(value["SLICE_INDEX"].get<std::vector<std::string>>(),
                                                                 value["SIGNAL"].get<std::string>())));
            break;
        }
        case MappingType::EXPR: {
            temp_map_reg.try_emplace(
                key, std::make_unique<ExprMapping>(
                         ExprMapping(value["EXPR"].get<std::string>(),
                                     value["PARAMETERS"].get<std::unordered_map<std::string, std::string>>())));
            break;
        }
        case MappingType::CUSTOM: {
            temp_map_reg.try_emplace(
                key, std::make_unique<CustomMapping>(CustomMapping(value["CUSTOM_TYPE"].get<CustomMapType_t>())));
            break;
        }
        default:
            break;
            // RAISE_PLUGIN_ERROR("ImasMastuPlugin::init_mappings(...) "
            // "Unrecognised mapping type");
        }
    }

    m_ids_map_register.try_emplace(ids_name, std::move(temp_map_reg));
    UDA_LOG(UDA_LOG_DEBUG, "calling read function \n");

    return 0;
}
