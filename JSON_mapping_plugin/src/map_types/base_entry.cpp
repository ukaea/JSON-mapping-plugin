#include "map_types/base_entry.hpp"
#include "utils/uda_plugin_helpers.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <inja/inja.hpp>
#include <unordered_map>

/**
 * @brief
 *
 * @param nvlist
 * @return
 */
int Mapping::set_current_request_data_map(
    std::unordered_map<std::string, std::string>& nvlist) {

    //////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////
    if (nvlist.empty()) {
        return 1;
    }

    int err{0};
    std::vector<int> vec_indices;
    std::vector<std::string> vec_indices_str;
    boost::split(vec_indices_str, nvlist.at("indices"), boost::is_any_of(";"));
    // Subtract 1 from each indices element, IMAS is 1-based
    // and other machines (MAST-intU for example) are zero-based
    std::for_each(vec_indices_str.begin(), vec_indices_str.end(),
                  [&](std::string& i_str) {
                      try {
                          // Subtract 1 from each indices element, IMAS is
                          // 1-based and other machines (MAST-U for example) are
                          // zero-based
                          vec_indices.push_back(std::stoi(i_str) - 1);
                          err = 0;
                      } catch (const std::invalid_argument& e) {
                          UDA_LOG(
                              UDA_LOG_DEBUG,
                              "\nMapping::set_current_request_data_map - %s -"
                              "Cannot convert string indices to int\n",
                              e.what());
                          err = 1;
                      }
                  });

    // Set request info
    // Replace hardcoded values after IMAS-plugin request change
    _request_data.host = "uda2.hpc.l";
    _request_data.port = 56565;
    _request_data.shot = std::stoi(nvlist.at("shot")); // ask forgiveness
    _request_data.indices = vec_indices;
    _request_data.sig_type = SignalType::DEFAULT;
    //////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////

    return err;
}
/**
 * @brief
 *
 * @param nvlist
 * @return
 */
int Mapping::set_current_request_data(NAMEVALUELIST* nvlist) {

    //////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////
    int run{0};
    int shot{0};
    int dtype{0};
    FIND_INT_VALUE(*nvlist, run);
    FIND_REQUIRED_INT_VALUE(*nvlist, shot);
    FIND_REQUIRED_INT_VALUE(*nvlist, dtype);

    int* indices{nullptr};
    size_t nindices{0};
    FIND_REQUIRED_INT_ARRAY(*nvlist, indices);
    // Convert int* into std::vector<int>
    std::vector<int> vec_indices(indices, indices + nindices);
    if (nindices == 1 && vec_indices.at(0) == -1) {
        nindices = 0;
        free(indices); // Legacy C UDA, replace if possible
        indices = nullptr;
    }
    // Subtract 1 from each indices element, IMAS is 1-based
    // and other machines (MAST-U for example) are zero-based
    std::for_each(vec_indices.begin(), vec_indices.end(),
                  [](int& n) { n -= 1; });

    // Set request info
    // Replace hardcoded values after IMAS-plugin request change
    _request_data.host = "uda2.hpc.l";
    _request_data.port = 56565;
    _request_data.shot = shot;
    _request_data.indices = vec_indices;
    _request_data.sig_type = SignalType::DEFAULT;
    //////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////

    return 0;
}

/**
 * @brief
 *
 * @param interface
 * @param entries
 * @param global_data
 * @param sig_type
 * @return
 */
int ValueEntry::map(
    IDAM_PLUGIN_INTERFACE* interface,
    const std::unordered_map<std::string, std::unique_ptr<Mapping>>& entries,
    const nlohmann::json& global_data) const {

    const auto temp_val = _value;
    if (temp_val.is_discarded() or temp_val.is_binary() or temp_val.is_null()) {
        UDA_LOG(UDA_LOG_DEBUG, "ValueEntry::map unrecognised json value type");
        return 1;
    }

    int err{1};
    if (temp_val.is_array()) {
        // Check all members of array are numbers
        // (Add array of strings if necessary)
        bool all_number = std::all_of(
            temp_val.begin(), temp_val.end(),
            [](const nlohmann::json& els) { return els.is_number(); });

        // deduce type if true
        if (all_number) {
            err = type_deduc_array(interface->data_block, temp_val);
        }

    } else if (temp_val.is_primitive()) {
        err = type_deduc_prim(interface->data_block, temp_val, global_data);
    } else {
        UDA_LOG(UDA_LOG_DEBUG, "ValueEntry::map not structured or primitive");
    }

    return err;
};

/**
 * @brief
 *
 * @param data_block
 * @param temp_val
 * @return
 */
int ValueEntry::type_deduc_array(DATA_BLOCK* data_block,
                                 const nlohmann::json& temp_val) const {

    switch (temp_val.front().type()) {
    case nlohmann::json::value_t::number_float: {
        // Handle array of floats
        auto temp_vec = temp_val.get<std::vector<float>>();
        imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec<float>(
            data_block, temp_vec);
        break;
    }
    case nlohmann::json::value_t::number_integer: {
        // Handle array of ints
        auto temp_vec = temp_val.get<std::vector<int>>();
        imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec<int>(
            data_block, temp_vec);
        break;
    }
    case nlohmann::json::value_t::number_unsigned: {
        // Handle array of ints
        auto temp_vec = temp_val.get<std::vector<unsigned int>>();
        imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec<unsigned int>(
            data_block, temp_vec);
        break;
    }
    default:
        return 1;
    }

    return 0;
}

/**
 * @brief
 *
 * @param data_block
 * @param temp_val
 * @param global_data
 * @return
 */
int ValueEntry::type_deduc_prim(DATA_BLOCK* data_block,
                                const nlohmann::json& temp_val,
                                const nlohmann::json& global_data) const {

    switch (temp_val.type()) {
    case nlohmann::json::value_t::number_float:
        // Handle float
        imas_json_plugin::uda_helpers::setReturnDataScalarType<float>(
            data_block, temp_val.get<float>(), nullptr);
        break;
    case nlohmann::json::value_t::number_integer:
        // Handle int
        imas_json_plugin::uda_helpers::setReturnDataScalarType<int>(
            data_block, temp_val.get<int>(), nullptr);
        break;
    case nlohmann::json::value_t::number_unsigned:
        // Handle int
        imas_json_plugin::uda_helpers::setReturnDataScalarType<unsigned int>(
            data_block, temp_val.get<unsigned int>(), nullptr);
        break;
    case nlohmann::json::value_t::boolean:
        // Handle bool
        imas_json_plugin::uda_helpers::setReturnDataScalarType<bool>(
            data_block, temp_val.get<bool>(), nullptr);
        break;
    case nlohmann::json::value_t::string: {
        // Handle string
        // Double inja template execution
        std::string post_inja_str{
            inja::render(inja::render(temp_val.get<std::string>(), global_data),
                         global_data)};
        // try to convert to integer
        // catch exception - output as string
        // inja templating may replace with number
        try {
            const int i_str{std::stoi(post_inja_str)}; // throw
            imas_json_plugin::uda_helpers::setReturnDataScalarType<int>(
                data_block, i_str, nullptr);
        } catch (const std::invalid_argument& e) {
            UDA_LOG(UDA_LOG_DEBUG,
                    "ValueEntry::map failure to convert"
                    "string to int in mapping : %s\n",
                    e.what());
            setReturnDataString(data_block, post_inja_str.c_str(), nullptr);
        }
        break;
    }
    default:
        return 1;
    }

    return 0;
}
