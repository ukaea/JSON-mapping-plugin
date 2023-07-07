#include "base_entry.hpp"

#include <clientserver/udaStructs.h>
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
int ValueEntry::map(
        IDAM_PLUGIN_INTERFACE* interface,
        const std::unordered_map<std::string, std::unique_ptr<Mapping>>& entries,
        const nlohmann::json& global_data, const SignalType sig_type) const {

    // TODO: change to switch case relying on nlohmann::json::value_t enum
    // or use nlohmann::json type deduction for implicit type mapping
    const auto temp_val = m_value;

    // assumption that any arrays of numbers, all the same type
    if (temp_val.is_structured()) {
        if (temp_val.is_array()) {
            type_deduc_array(interface->data_block, temp_val);
        } else {
            UDA_LOG(UDA_LOG_DEBUG,
                    "ValueEntry::map JSON object type currently unsupported");
            return 1;
        }
    } else {
        type_deduc_scalar(interface->data_block, temp_val, global_data);
    }
    return 0;
};

/**
 * @brief 
 *
 * @param data_block 
 * @param arrValue 
 * @return 
 */
int ValueEntry::type_deduc_array(DATA_BLOCK* data_block,
        const nlohmann::json& arrValue) const {

    bool all_type_number = std::all_of(arrValue.begin(), arrValue.end(),
        [](const nlohmann::json& els) { return els.is_number(); });

    if (all_type_number) {
        if (arrValue.front().is_number_float()) {
            auto temp_vec = arrValue.get<std::vector<float>>();
            imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec<
                float>(data_block, temp_vec);
        } else {
            auto temp_vec = arrValue.get<std::vector<int>>();
            imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec<
                int>(data_block, temp_vec);
        }
    } else {
        return 1;
    }

    return 0;
}

/**
 * @brief 
 *
 * @param data_block 
 * @param numValue 
 * @param global_data 
 * @return 
 */
int ValueEntry::type_deduc_scalar(DATA_BLOCK* data_block,
        const nlohmann::json& numValue,
        const nlohmann::json& global_data) const {

    if (numValue.is_number()) {
        if (numValue.is_number_float()) {
            imas_json_plugin::uda_helpers::setReturnDataScalarType<float>(
                    data_block, numValue.get<float>(), nullptr);
        } else {
            imas_json_plugin::uda_helpers::setReturnDataScalarType<int>(
                    data_block, numValue.get<int>(), nullptr);
        }
    } else if (numValue.is_string()) {
        std::string post_inja_str{
            inja::render( // Double inja::render
                inja::render(numValue.get<std::string>(), global_data),
                global_data
            )
        };
        try {
            const int i_str{std::stoi(post_inja_str)};
            imas_json_plugin::uda_helpers::setReturnDataScalarType<int>(
                    data_block, i_str, nullptr);
        } catch (const std::invalid_argument& e) {
            UDA_LOG(UDA_LOG_DEBUG,
                    "ValueEntry::type_deduc_scalar failure to convert"
                    "string to int in mapping : %s\n",
                    e.what());
            setReturnDataString(data_block,
                    post_inja_str.c_str(), nullptr);
        }
    } else {
        return 1;
    }

    return 0;

}
