#pragma once

#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>

#include "map_types/base_entry.hpp"
#include <nlohmann/json.hpp>

using IDSMapRegister_t = std::unordered_map<std::string, std::unique_ptr<Mapping>>;
using IDSMapRegisterStore_t = std::unordered_map<std::string, IDSMapRegister_t>;
using IDSAttrRegisterStore_t = std::unordered_map<std::string, nlohmann::json>;
using MappingPair = std::pair<nlohmann::json&, IDSMapRegister_t&>;

class MappingHandler {

  public:
    MappingHandler()
        : _init(false)
        , _imas_version("3.37")
    {}

    explicit MappingHandler(std::string imas_version)
        : _init(false)
        , _imas_version(std::move(imas_version))
    {}

    ~MappingHandler() {
        _ids_attributes.clear();
        _ids_map_register.clear();
        _mapping_config.clear();
        _init = false;
    }

    int init() {
        if (_init || !_ids_map_register.empty()) {
            return 0;
        }
        load_all();

        _init = true;
        return 0;
    }

    int set_map_dir(const std::string& mapping_dir);
    MappingPair read_mappings(const std::string& request_ids);

  private:
    int init_mappings(const std::string& ids_name, const nlohmann::json& data);
    int load_all();
    int load_globals(const std::string& ids_str);
    int load_mappings(const std::string& ids_str);

    IDSMapRegisterStore_t _ids_map_register;
    IDSAttrRegisterStore_t _ids_attributes;
    bool _init;

    std::string _imas_version;
    std::string _mapping_dir;
    nlohmann::json _mapping_config;
};
