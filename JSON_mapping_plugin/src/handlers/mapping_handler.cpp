#include "mapping_handler.hpp"

#include <inja/inja.hpp>

// #include <map_entry.hpp>
// #include <offset_entry.hpp>
// #include <scale_entry.hpp>
// #include <expr_entry.hpp>
// #include <dim_entry.hpp>

const MappingPair
MappingHandler::read_mappings(const std::string& request_ids) {
    // AJP :: Safety check if ids request not in mapping json (and typo
    // obviously)
    return std::make_pair(std::ref(m_ids_attributes[request_ids]),
                          std::ref(m_ids_map_register[request_ids]));
}

int MappingHandler::set_map_dir(const std::string mapping_dir) {
    m_mapping_dir = mapping_dir;
    return 0;
}

int MappingHandler::load_all() {

    std::ifstream map_cfg_file(m_mapping_dir + "/mappings.cfg.json");
    if (map_cfg_file) {
        map_cfg_file >> m_mapping_config;
        map_cfg_file.close();
    } else {
        RAISE_PLUGIN_ERROR("MappingHandler::load_configs - Cannot open JSON "
                           "mapping config file");
    }

    for (const auto& ids_str :
         m_mapping_config[m_imas_version].get<std::vector<std::string>>()) {
        load_globals(ids_str);
        load_mappings(ids_str);
    }

    return 0;
}

int MappingHandler::load_globals(const std::string& ids_str) {

    std::string file_path{"/" + m_mapping_dir + "/" + ids_str + "/" +
                          "globals.json"};

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
        RAISE_PLUGIN_ERROR(
            "MappingHandler::load_globals- Cannot open JSON globals file");
    }
    return 0;
}

int MappingHandler::load_mappings(const std::string& ids_str) {

    std::string file_path{m_mapping_dir + "/" + ids_str + "/" + ids_str +
                          ".json"};

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

        init_mappings(ids_str, temp_mappings[ids_str]);

    } else {
        RAISE_PLUGIN_ERROR(
            "MappingHandler::load_mappings - Cannot open JSON mapping file");
    }
    return 0;
}

int MappingHandler::init_mappings(const std::string& ids_name,
                                  const nlohmann::json& data) {

    IDSMapRegister_t temp_map_reg;
    std::optional<std::string> var{std::nullopt};
    for (const auto& [key, value] : data.items()) {

        switch (value["MAP_TYPE"].get<MapTransfos>()) {
        case MapTransfos::VALUE:
            temp_map_reg.try_emplace(
                key, std::make_unique<ValueEntry>(ValueEntry(value["VALUE"])));
            break;
            // case MapTransfos::MAP :
            //     if(value.contains("VAR")) value["VAR"].get_to(var);
            //     temp_map_reg.try_emplace(key,
            //             std::make_unique<MapEntry>( MapEntry(
            //                     value["PLUGIN"].get<PluginType>(),
            //                     value["KEY"].get<std::string>(),
            //                     var
            //                 )));
            //     break;
            // case MapTransfos::OFFSET :
            //     if(value.contains("VAR")) value["VAR"].get_to(var);
            //     float temp_float_offset;
            //     if (value["OFFSET"].is_number_float()){
            //         temp_float_offset = value["OFFSET"].get<float>();
            //     } else if (value["OFFSET"].is_string()) {
            //         try {
            //             const auto post_inja_str = inja::render(
            //                                             value["OFFSET"].get<std::string>(),
            //                                             m_ids_attributes[ids_name]
            //                                         );
            //             temp_float_offset = std::stof(post_inja_str);
            //         } catch (const std::invalid_argument& e) {
            //             UDA_LOG(UDA_LOG_DEBUG, "\nCannot convert OFFSET
            //             string to float\n"); break;
            //         }
            //     }
            //     temp_map_reg.try_emplace(key,
            //             std::make_unique<OffsetEntry>(OffsetEntry(
            //                     value["PLUGIN"].get<PluginType>(),
            //                     value["KEY"].get<std::string>(),
            //                     var,
            //                     temp_float_offset
            //                 )));
            //     break;
            // case MapTransfos::SCALE :
            //     if(value.contains("VAR")) value["VAR"].get_to(var);
            //     float temp_float_scale;
            //     if (value["SCALAR"].is_number_float()){
            //         temp_float_scale = value["SCALAR"].get<float>();
            //     } else if (value["SCALAR"].is_string()) {
            //         try {
            //             const auto post_inja_str = inja::render(
            //                                             value["SCALAR"].get<std::string>(),
            //                                             m_ids_attributes[ids_name]
            //                                         );
            //             temp_float_scale = std::stof(post_inja_str);
            //         } catch (const std::invalid_argument& e) {
            //             UDA_LOG(UDA_LOG_DEBUG, "\nCannot convert SCALAR
            //             string to float\n"); break;
            //         }
            //     }
            //     temp_map_reg.try_emplace(key,
            //             std::make_unique<ScaleEntry>(ScaleEntry(
            //                     value["PLUGIN"].get<PluginType>(),
            //                     value["KEY"].get<std::string>(),
            //                     var,
            //                     temp_float_scale
            //                 )));
            //     break;
            // case MapTransfos::DIM :
            //     temp_map_reg.try_emplace(key,
            //             std::make_unique<DimEntry>(DimEntry(
            //                     value["DIM_PROBE"].get<std::string>()
            //                 )));
            //     break;
            // case MapTransfos::EXPR :
            //     temp_map_reg.try_emplace(key,
            //             std::make_unique<ExprEntry>(ExprEntry(
            //                     value["EXPR"].get<std::string>(),
            //                     value["PARAMETERS"].get<std::unordered_map<std::string,
            //                     std::string>>()
            //                 )));
            //     break;
        default:
            RAISE_PLUGIN_ERROR("ImasMastuPlugin::init_mappings(...) "
                               "Unrecognised mapping type");
        }
    }
    m_ids_map_register.try_emplace(ids_name, std::move(temp_map_reg));
    UDA_LOG(UDA_LOG_DEBUG, "calling read function \n");

    return 0;
}
