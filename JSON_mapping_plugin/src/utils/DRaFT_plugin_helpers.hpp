/**
 * @file
 * @brief
 */

#include "utils/uda_plugin_helpers.hpp"
#include <clientserver/udaStructs.h>
#include <fstream>
#include <nlohmann/json.hpp>

namespace DRaFT_plugin_helpers {

/**
 * @brief
 *
 * @param key
 * @param shot
 * @return
 */
nlohmann::json read_json_data(std::string_view key, int shot) {

    // data directory in mapping repository
    std::string map_dir = getenv("JSON_MAPPING_DIR");
    std::string data_path = map_dir + "/data/" + std::to_string(shot) + ".json";
    std::ifstream json_file(data_path);
    auto temp_json = nlohmann::json::parse(json_file);
    json_file.close();

    try {
        nlohmann::json::json_pointer json_p{key.data()}; // TO CHANGE
        temp_json = temp_json[json_p];
    } catch (nlohmann::json::parse_error& e) {
        temp_json = nlohmann::json::parse(R"(null)");
    }

    return temp_json;
}

/**
 * @brief
 *
 * @return
 */
int set_return_data(DATA_BLOCK* data_block, nlohmann::json& data,
                    std::string_view var) {

    int err{1};
    // sort out tomorrow
    std::string type{data["type"]};
    int rank{data["rank"]};

    const std::unordered_map<std::string, UDA_TYPE> UDA_TYPE_MAP{
        {typeid(int).name(), UDA_TYPE_INT},
        {typeid(float).name(), UDA_TYPE_FLOAT},
        {typeid(double).name(), UDA_TYPE_DOUBLE}};

    if (rank > 0) {

        switch (UDA_TYPE_MAP.at(type)) {
        case UDA_TYPE_INT: {
            auto vec_values = data[var].get<std::vector<int>>();
            err =
                imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec<int>(
                    data_block, vec_values);
            break;
        }
        case UDA_TYPE_FLOAT: {
            auto vec_values = data[var].get<std::vector<float>>();
            err = imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec<
                float>(data_block, vec_values);
            break;
        }
        case UDA_TYPE_DOUBLE: {
            auto vec_values = data[var].get<std::vector<double>>();
            err = imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec<
                double>(data_block, vec_values);
            break;
        }
        default: {
            break;
        }}

    } else if (rank == 0) {

        switch (UDA_TYPE_MAP.at(type)) {
        case UDA_TYPE_INT: {
            auto value = data[var].get<int>();
            err = imas_json_plugin::uda_helpers::setReturnDataScalarType<int>(
                data_block, value);
            break;
        }
        case UDA_TYPE_FLOAT: {
            auto value = data[var].get<float>();
            err = imas_json_plugin::uda_helpers::setReturnDataScalarType<float>(
                data_block, value);
            break;
        }
        case UDA_TYPE_DOUBLE: {
            auto value = data[var].get<double>();
            err =
                imas_json_plugin::uda_helpers::setReturnDataScalarType<double>(
                    data_block, value);
            break;
        }
        default: {
            break;
        }
        }
    }

    return err;
}

/**
 * @brief
 *
 * @param data_block
 * @param key
 * @param var
 * @param shot
 * @return
 */
int get_data(DATA_BLOCK* data_block, std::string_view key, std::string_view var,
             int shot) {

    int err{1};
    if (var.empty()) {
        return err;
    }
    auto local_json = read_json_data(key, shot);

    return set_return_data(data_block, local_json, var);
}
} // namespace DRaFT_plugin_helpers
