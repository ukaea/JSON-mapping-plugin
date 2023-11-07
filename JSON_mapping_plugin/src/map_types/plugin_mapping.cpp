/**
 * @file
 * @brief
 */

#include "plugin_mapping.hpp"

#include "utils/scale_offset.hpp"
#include "utils/uda_plugin_helpers.hpp"
#include <inja/inja.hpp>

/**
 * @brief
 *
 * eg. UDA::get(signal=/AMC/ROGEXT/P1U, source=45460, host=uda2.hpc.l, port=56565)
 * eg. GEOM::get(signal=/magnetics/pfcoil/d1_upper, Config=1);
 * eg. JSONDataReader::get(signal=/APC/plasma_current);
 *
 * @param json_globals
 * @return
 */
std::string PluginMapping::get_request_str(const MapArguments& arguments) const {

    std::stringstream string_stream;
    string_stream << m_plugin << "::" << m_function.value_or("get") << "(";

    // m_map_args 'field' currently nlohmann json
    // parse to string/bool
    // TODO: change, however std::any/std::variant functionality for free
    const char* delim = "";
    for (const auto& [key, field] : m_map_args) {
        if (field.is_string()) {
            // Double inja
            try {
                auto value = inja::render(inja::render(field.get<std::string>(), arguments.m_global_data),
                                          arguments.m_global_data);
                string_stream << delim << key << "=" << value;
            } catch (std::exception& e) {
                UDA_LOG(UDA_LOG_DEBUG, "Inja template error in request : %s\n", e.what());
                return {};
            }
        } else if (field.is_boolean()) {
            string_stream << delim << key;
        } else {
            continue;
        }
        delim = ", ";
    }
    string_stream << ")";

    if (m_slice.has_value() && arguments.m_sig_type != SignalType::DIM) {
        string_stream << inja::render(inja::render(m_slice.value(), arguments.m_global_data), arguments.m_global_data);
    }

    auto request = string_stream.str();
    UDA_LOG(UDA_LOG_DEBUG, "Plugin Mapping Request : %s\n", request.c_str());
    return request;
}

int PluginMapping::call_plugins(const MapArguments& arguments) const {

    int err{1};
    auto request_str = get_request_str(arguments);
    if (request_str.empty()) {
        return err;
    } // Return 1 if no request receieved

    err = callPlugin(arguments.m_interface->pluginList, request_str.c_str(), arguments.m_interface);
    if (err) {
        // add check of int udaNumErrors() and if more than one, don't wipe
        // 220 situation when UDA tries to get data and cannot find it
        if (err == 220)
            closeUdaError();
        return err;
    } // return code if failure, no need to proceed

    if (m_plugin == "UDA" && arguments.m_sig_type == SignalType::TIME) {
        // Opportunity to handle time differently
        // Return time SignalType early, no need to scale/offset
        err = imas_json_plugin::uda_helpers::setReturnTimeArray(arguments.m_interface->data_block);
        return err;
    }

    // scale takes precedence
    if (m_scale.has_value()) {
        err = JMP::map_transform::transform_scale(arguments.m_interface->data_block, m_scale.value());
    }
    if (m_offset.has_value()) {
        err = JMP::map_transform::transform_offset(arguments.m_interface->data_block, m_offset.value());
    }

    return err;
}

int PluginMapping::map(const MapArguments& arguments) const { return call_plugins(arguments); }
