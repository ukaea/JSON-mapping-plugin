/**
 * @file
 * @brief
 */

#include "map_entry.hpp"

#include "utils/scale_offset.hpp"
#include "utils/uda_plugin_helpers.hpp"
#include <boost/format.hpp>
#include <inja/inja.hpp>

/**
 * @brief
 *
 * eg. UDA::get(signal=/AMC/ROGEXT/P1U, source=45460,
 *              host=uda2.hpc.l, port=56565)
 * eg. GEOMETRY::get(signal=/magnetics/pfcoil/d1_upper, Config=1);
 * eg. JSONDataReader::get(signal=/APC/plasma_current);
 *
 * @param json_globals
 * @return
 */
std::string
MapEntry::get_request_str(const nlohmann::json& json_globals) const {

    // TODO: replace dependence on boost in the future
    // stringstream?
    std::string request_str = m_plugin.second + "::get(";

    // m_map_args 'field' currently nlohmann json
    // parse to string/bool
    // TODO: change, however std::any/std::variant functionality for free
    for (const auto& [key, field] : m_map_args) {
        if (field.is_string()) {
            request_str +=
                (boost::format("%s=%s, ") % key %
                 inja::render( // Double inja
                     inja::render(field.get<std::string>(), json_globals),
                     json_globals))
                    .str();
        } else if (field.is_boolean()) {
            request_str += (boost::format("%s, ") % key).str();
        } else {
            continue;
        }
    }
    request_str +=
        (boost::format("source=%i, host=%s, port=%i)") % m_request_data.shot %
         m_request_data.host % m_request_data.port)
            .str();

    // Add slice to request (when implemented)
    // if (m_slice.has_value()) {
    //     request_str += (boost::format("[%s]") % m_slice).str();
    // }

    UDA_LOG(UDA_LOG_DEBUG, "AJP Request : %s\n", request_str.c_str());
    return request_str;
}

int MapEntry::call_plugins(IDAM_PLUGIN_INTERFACE* interface,
                           const nlohmann::json& json_globals) const {

    int err{1};
    auto request_str = get_request_str(json_globals);
    if (request_str.empty()) {
        return err;
    } // Return 1 if no request receieved

    err = callPlugin(interface->pluginList, request_str.c_str(), interface);
    if (err) {
        return err;
    } // return code if failure, no need to proceed

    if (m_request_data.sig_type == SignalType::TIME) {
        // Opportunity to handle time differently
        // Return time SignalType early, no need to scale/offset
        if (m_plugin.first == PluginType::UDA) {
            err = imas_json_plugin::uda_helpers::setReturnTimeArray(
                interface->data_block);
        }
        return err;
    }

    if (m_scale.has_value()) {
        err = JMP::map_transform::transform_scale(interface->data_block,
                                                  m_scale.value());
    }
    if (m_offset.has_value()) {
        err = JMP::map_transform::transform_offset(interface->data_block,
                                                   m_offset.value());
    }

    return err;
}

int MapEntry::map(
    IDAM_PLUGIN_INTERFACE* interface,
    const std::unordered_map<std::string, std::unique_ptr<Mapping>>& entries,
    const nlohmann::json& json_globals) const {

    return call_plugins(interface, json_globals);
};
