#include "value_mapping.hpp"

#include "utils/uda_plugin_helpers.hpp"

#include <inja/inja.hpp>

/**
 * @brief
 *
 * @param interface
 * @param entries
 * @param global_data
 * @param sig_type
 * @return
 */
int ValueMapping::map(const MapArguments& arguments) const {

    const auto temp_val = m_value;
    if (temp_val.is_discarded() or temp_val.is_binary() or temp_val.is_null()) {
        UDA_LOG(UDA_LOG_DEBUG, "ValueMapping::map unrecognised json value type");
        return 1;
    }

    int err{1};
    if (temp_val.is_array()) {
        // Check all members of array are numbers
        // (Add array of strings if necessary)
        const bool all_number =
            std::all_of(temp_val.begin(), temp_val.end(), [](const nlohmann::json& els) { return els.is_number(); });

        // deduce type if true
        if (all_number) {
            err = type_deduce_array(arguments.m_interface->data_block, temp_val);
        }

    } else if (temp_val.is_primitive()) {
        err = type_deduce_prim(arguments.m_interface->data_block, temp_val, arguments.m_global_data);
    } else {
        UDA_LOG(UDA_LOG_DEBUG, "ValueMapping::map not structured or primitive");
    }

    return err;
}

/**
 * @brief
 *
 * @param data_block
 * @param temp_val
 * @return
 */
int ValueMapping::type_deduce_array(DATA_BLOCK* data_block, const nlohmann::json& temp_val) {

    switch (temp_val.front().type()) {
    case nlohmann::json::value_t::number_float: {
        // Handle array of floats
        auto temp_vec = temp_val.get<std::vector<float>>();
        imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec<float>(data_block, temp_vec);
        break;
    }
    case nlohmann::json::value_t::number_integer: {
        // Handle array of ints
        auto temp_vec = temp_val.get<std::vector<int>>();
        imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec<int>(data_block, temp_vec);
        break;
    }
    case nlohmann::json::value_t::number_unsigned: {
        // Handle array of ints
        auto temp_vec = temp_val.get<std::vector<unsigned int>>();
        imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec<unsigned int>(data_block, temp_vec);
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
int ValueMapping::type_deduce_prim(DATA_BLOCK* data_block, const nlohmann::json& temp_val,
                                   const nlohmann::json& global_data) {

    switch (temp_val.type()) {
    case nlohmann::json::value_t::number_float:
        // Handle float
        imas_json_plugin::uda_helpers::setReturnDataScalarType<float>(data_block, temp_val.get<float>(), nullptr);
        break;
    case nlohmann::json::value_t::number_integer:
        // Handle int
        imas_json_plugin::uda_helpers::setReturnDataScalarType<int>(data_block, temp_val.get<int>(), nullptr);
        break;
    case nlohmann::json::value_t::number_unsigned:
        // Handle int
        imas_json_plugin::uda_helpers::setReturnDataScalarType<unsigned int>(data_block, temp_val.get<unsigned int>(),
                                                                             nullptr);
        break;
    case nlohmann::json::value_t::boolean:
        // Handle bool
        imas_json_plugin::uda_helpers::setReturnDataScalarType<bool>(data_block, temp_val.get<bool>(), nullptr);
        break;
    case nlohmann::json::value_t::string: {
        // Handle string
        // Double inja template execution
        std::string const post_inja_str{
            inja::render(inja::render(temp_val.get<std::string>(), global_data), global_data)};
        // try to convert to integer
        // catch exception - output as string
        // inja templating may replace with number
        try {
            const int i_str{std::stoi(post_inja_str)}; // throw
            imas_json_plugin::uda_helpers::setReturnDataScalarType<int>(data_block, i_str, nullptr);
        } catch (const std::invalid_argument& e) {
            UDA_LOG(UDA_LOG_DEBUG,
                    "ValueMapping::map failure to convert"
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