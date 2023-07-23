/**
 * @file
 * @brief
 */

#include "helpers/uda_plugin_helpers.hpp"
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
int deduc_type_rank() { return 0; }

/**
 * @brief
 *
 * @return
 */
int set_return_data() {

    deduc_type_rank();
    return 0;
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

    // int err{0};
    if (key == "/AMC/PLASMA_CURRENT") { // temporary plasma_current test
        auto local_json = read_json_data(key, shot);
        auto vec_values = local_json[var].get<std::vector<float>>();
        imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec<float>(
            data_block, vec_values);
    }
    return 0;
}
} // namespace DRaFT_plugin_helpers
