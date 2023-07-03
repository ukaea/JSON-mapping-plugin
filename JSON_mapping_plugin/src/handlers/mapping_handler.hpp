#pragma once

#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>

#include <map_types/base_entry.hpp>
#include <nlohmann/json.hpp>

using IDSMapRegister_t =
    std::unordered_map<std::string, std::unique_ptr<Mapping>>;
using IDSMapRegisterStore_t = std::unordered_map<std::string, IDSMapRegister_t>;
using IDSAttrRegisterStore_t = std::unordered_map<std::string, nlohmann::json>;
using MappingPair = std::pair<nlohmann::json&, IDSMapRegister_t&>;

class MappingHandler {

  public:
    MappingHandler() : m_init(false), m_imas_version("3.37"){};
    explicit MappingHandler(std::string imas_version)
        : m_init(false), m_imas_version(std::move(imas_version)){};
    ~MappingHandler() {
        m_ids_attributes.clear();
        m_ids_map_register.clear();
        m_mapping_config.clear();
        m_init = false;
    }
    int init() {
        if (m_init || !m_ids_map_register.empty()) {
            return 0;
        }
        load_all();

        m_init = true;
        return 1;
    };
    int set_map_dir(const std::string mapping_dir);
    const MappingPair read_mappings(const std::string& request_ids);

  private:
    int init_mappings(const std::string& ids_name, const nlohmann::json& data);
    int load_all();
    int load_globals(const std::string& ids_str);
    int load_mappings(const std::string& ids_str);

    IDSMapRegisterStore_t m_ids_map_register;
    IDSAttrRegisterStore_t m_ids_attributes;
    bool m_init;

    std::string m_imas_version;
    std::string m_mapping_dir;
    nlohmann::json m_mapping_config;
};
