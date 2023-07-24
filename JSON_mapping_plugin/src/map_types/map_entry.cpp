#include "map_entry.hpp"
#include "base_entry.hpp"

// #include <mastu_plugin_helpers.hpp>
#include "helpers/DRaFT_plugin_helpers.hpp"
#include "helpers/uda_plugin_helpers.hpp"
#include <boost/format.hpp>
#include <inja/inja.hpp>

std::string MapEntry::get_request(const nlohmann::json& json_globals) const {

    std::string request;
    std::string post_inja{inja::render(m_key, json_globals)};
    post_inja = inja::render(post_inja, json_globals); //!! Double inja render

    if (m_plugin == PluginType::UDA) {

        // eg. UDA::get(signal=/AMC/ROGEXT/P1U, source=45460,
        //              host=uda2.hpc.l, port=56565)
        request =
            (boost::format("UDA::get(signal=%s, source=%d, host=%s, port=%d)") %
             post_inja % m_request_data.shot % m_request_data.host %
             m_request_data.port)
                .str();

    } else if (m_plugin == PluginType::GEOMETRY) {

        // eg. GEOMETRY::get(signal=/magnetics/pfcoil/d1_upper, Config=1);
        std::transform(post_inja.begin(), post_inja.end(), post_inja.begin(),
                       ::tolower);
        request = (boost::format("GEOM::get(signal=%1%, Config=1)") % post_inja)
                      .str();

    } else if (m_plugin == PluginType::JSONReader) {

        // Return post_inja key, JSONReader --> code call, rather than plugin
        request = post_inja;
    }

    UDA_LOG(UDA_LOG_DEBUG, "AJP Request : %s\n", request.c_str());
    return request;
}

int MapEntry::call_plugins(IDAM_PLUGIN_INTERFACE* interface,
                           const nlohmann::json& json_globals) const {

    int err{1};
    auto request = get_request(json_globals);
    if (request.empty()) {
        return err; // Return 1 if no request retrieved
    }

    if (m_plugin == PluginType::UDA) { // UDA

        switch (m_request_data.sig_type) {
        case SignalType::DEFAULT: // fallthrough
            [[fallthrough]];
        case SignalType::DATA:
            err = callPlugin(interface->pluginList, request.c_str(), interface);
            break;
        case SignalType::TIME:
            if (!callPlugin(interface->pluginList, request.c_str(),
                            interface)) {
                err = imas_json_plugin::uda_helpers::setReturnTimeArray(
                    interface->data_block);
            }
            break;
        case SignalType::ERROR:
            // To implement
            break;
        case SignalType::DIM:
            // if (!callPlugin(interface->pluginList, request.c_str(),
            // interface)) {
            // AJP: setReturnDataScalar<int>(interface,
            // interface->data_block->data_n, nullptr);
            // }
            break;
        default:
            break;
        }

    } else if (m_plugin == PluginType::GEOMETRY) {

        err = 1; // Change to error enum class

    } else if (m_plugin == PluginType::JSONReader) { // JSONReader

        err = DRaFT_plugin_helpers::get_data(interface->data_block, request,
                                             m_var.value_or(""),
                                             m_request_data.shot);
    }

    return err;
}

int MapEntry::map(
    IDAM_PLUGIN_INTERFACE* interface,
    const std::unordered_map<std::string, std::unique_ptr<Mapping>>& entries,
    const nlohmann::json& json_globals) const {

    return call_plugins(interface, json_globals);
};
