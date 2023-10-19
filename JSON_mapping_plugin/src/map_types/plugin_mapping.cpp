/**
 * @file
 * @brief
 */

#include "plugin_mapping.hpp"

#include "utils/scale_offset.hpp"
#include "utils/uda_plugin_helpers.hpp"
#include <inja/inja.hpp>
#include <plugins/udaPlugin.h>
#include <utils/ram_cache.hpp>

// TODO:
//  - print data array before and after caching
//  - print whole datablock before and after caching -- differences? (other params?)
//  - handle compressed dims
//  - handle error arrays (how to determine not empty?)

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

    for (const auto& flag : m_map_flags) {
        string_stream << delim << flag;
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


   /*
    *
    * CACHING GOES HERE
    *
    */
    // check cache for request string and only get data if it's not already there
    // currently copies whole datablock (data, error, and dims)
    std::optional<DATA_BLOCK*> maybe_db = m_ram_cache->copy_from_cache(request_str, arguments.m_interface->data_block);
    if (maybe_db)
    {
        ram_cache::log(ram_cache::LogLevel::INFO, "Adding cached datablock onto plugin_interface");

        // arguments.m_interface->data_block = maybe_db.value();
        // extra plugin_interface requirements? 
        // arguments.m_interface->data_block->signal_rec = (SIGNAL*)malloc(sizeof(SIGNAL));
        // initSignal(arguments.m_interface->data_block->signal_rec);
        // arguments.m_interface->data_block->data_system = (DATA_SYSTEM*)malloc(sizeof(DATA_SYSTEM));
        // initDataSystem(arguments.m_interface->data_block->data_system);
        // initSignalDesc(arguments.m_interface->signal_desc);
        // initDataSource(arguments.m_interface->data_source);

        ram_cache::log(ram_cache::LogLevel::INFO, "data on plugin_interface (data_n): " + std::to_string(arguments.m_interface->data_block->data_n));
        // ram_cache::log_datablock_status(arguments.m_interface->data_block, "data_block on return interface structure");

        return 0;

        // DATA_BLOCK* data_block = arguments.m_interface->data_block;
        // size_t shape = data_block->data_n;
        // switch(data_block->data_type)
        // {
        //     case UDA_TYPE_DOUBLE:
        //         err = setReturnDataDoubleArray(data_block, (double*)data_block->data, data_block->rank, &shape, nullptr);
        //         return err;
        //     case UDA_TYPE_FLOAT:
        //         err = setReturnDataFloatArray(data_block, (float*)data_block->data, data_block->rank, &shape, nullptr);
        //         return err;

        // }
    }
    else 
    {
        err = callPlugin(arguments.m_interface->pluginList, request_str.c_str(), arguments.m_interface);
        if (err) {
            // add check of int udaNumErrors() and if more than one, don't wipe
            // 220 situation when UDA tries to get data and cannot find it
            if (err == 220)
                closeUdaError();
            return err;
        } // return code if failure, no need to proceed

        // Add retrieved datablock to cache. data is copied from datablock into a new ram_cache::data_entry. original data remains
        // on block (on plugin_interface structure) for return.
        std::shared_ptr<ram_cache::DataEntry> new_cache_entry = m_ram_cache->make_data_entry(arguments.m_interface->data_block);
        m_ram_cache->add(request_str, new_cache_entry);

    }
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

int PluginMapping::map(const MapArguments& arguments) const {

    int err = call_plugins(arguments);
    // temporary solution to the slice functionality returning arrays of 1 element
    if (arguments.m_interface->data_block->rank == 1 and arguments.m_interface->data_block->data_n == 1) {
        arguments.m_interface->data_block->rank = 0;
    }
    return err;
}
