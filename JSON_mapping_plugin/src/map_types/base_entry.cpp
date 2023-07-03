#include "base_entry.hpp"

#include <inja/inja.hpp>

int ValueEntry::map(
    IDAM_PLUGIN_INTERFACE* interface,
    const std::unordered_map<std::string, std::unique_ptr<Mapping>>& entries,
    const nlohmann::json& global_data, const SignalType sig_type) const {
    // Needs to be optimised
    const auto temp_val = m_value;
    // assumption that any arrays of numbers, all the same type
    if (temp_val.is_structured()) {
        if (temp_val.is_array()) {
            if (std::all_of(
                    temp_val.begin(), temp_val.end(),
                    [](const nlohmann::json& el) { return el.is_number(); })) {
                if (temp_val.front().is_number_float()) {
                    auto temp_vec = temp_val.get<std::vector<float>>();
                    imas_mastu_plugin::uda_helpers::setReturnDataArrayType_Vec<
                        float>(interface->data_block, temp_vec);
                } else {
                    auto temp_vec = temp_val.get<std::vector<int>>();
                    imas_mastu_plugin::uda_helpers::setReturnDataArrayType_Vec<
                        int>(interface->data_block, temp_vec);
                }
            } else {
                return 1;
            }
        } else {
            return 1;
        }
    } else {
        if (temp_val.is_number()) {
            if (temp_val.is_number_float()) {
                imas_mastu_plugin::uda_helpers::setReturnDataScalarType<float>(
                    interface->data_block, temp_val.get<float>(), nullptr);
            } else {
                imas_mastu_plugin::uda_helpers::setReturnDataScalarType<int>(
                    interface->data_block, temp_val.get<int>(), nullptr);
            }
        } else if (temp_val.is_string()) {
            std::string post_inja_str{inja::render(
                inja::render(temp_val.get<std::string>(), global_data),
                global_data)};
            try {
                const int i_str{std::stoi(post_inja_str)};
                imas_mastu_plugin::uda_helpers::setReturnDataScalarType<int>(
                    interface->data_block, i_str, nullptr);
            } catch (const std::invalid_argument& e) {
                UDA_LOG(UDA_LOG_DEBUG,
                        "ValueEntry::map failure to convert string to int in "
                        "mapping : %s\n",
                        e.what());
                setReturnDataString(interface->data_block,
                                    post_inja_str.c_str(), nullptr);
            }
        } else {
            return 1;
        }
    }
    return 0;
};
