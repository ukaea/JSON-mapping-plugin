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
#include <utils/subset.hpp>
#include <clientserver/makeRequestBlock.h>
// TODO:
//  - handle compressed dims
//  - handle error arrays (how to determine not empty?)
//  - only read required data out of cache for each request (i.e. data, error, or a single dim)

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
     * generate subset info then remove subset syntax copy_from_cache
     *  request string
     *
     */

    
    IDAM_PLUGIN_INTERFACE* plugin_interface = arguments.m_interface;
    REQUEST_DATA request = *plugin_interface->request_data;

    request.source[0] = '\0';
    strcpy(request.signal, request_str.c_str());
    makeRequestData(&request, *plugin_interface->pluginList, plugin_interface->environment);
    subset::log_request_status(&request, "request block before interception: ");


    // better maybe to test if final char is ']' then find correspongind '[' from rfind
    // or to use regex...

    std::size_t subset_syntax_position = request_str.find('[');
    if (subset_syntax_position!=std::string::npos)
    {
        ram_cache::log(ram_cache::LogLevel::DEBUG, "request before alteration: " + request_str);
        request_str.erase(subset_syntax_position);
        ram_cache::log(ram_cache::LogLevel::DEBUG, "request after alteration: " + request_str);
    }

    REQUEST_DATA request2 = *plugin_interface->request_data;

    request2.source[0] = '\0';
    strcpy(request2.signal, request_str.c_str());
    makeRequestData(&request2, *plugin_interface->pluginList, plugin_interface->environment);
    subset::log_request_status(&request2, "request block after interception: ");



    std::string key_found = m_ram_cache->has_entry(request_str) ? "True" : "False";
    ram_cache::log(ram_cache::LogLevel::DEBUG, "key, \"" + request_str + "\" in cache? " + key_found);
    

   /*
    *
    * CACHING GOES HERE
    *
    */
    // replace subset block with an empty one so we can cache un-sliced data and apply different subsets on each subsequent call
    // subset::log_request_status(arguments.m_interface->request_data, "request block status:");
    
    SUBSET datasubset = request.datasubset;
    
    // disbale subsetting one leevel up
    arguments.m_interface->request_data->datasubset = SUBSET(); 
    arguments.m_interface->request_data->datasubset.nbound = 0; 
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

        // return 0;
        err = 0;

    }
    else 
    {
        err = callPlugin(arguments.m_interface->pluginList, request_str.c_str(), arguments.m_interface);
        subset::log_request_status(arguments.m_interface->request_data, "request block status:");

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

    // this is the line causing unexpected subsetting... 
    arguments.m_interface->request_data->datasubset = datasubset;
    if (datasubset.nbound > 0)
    {
        subset::apply_subsetting(arguments.m_interface, 1.0, 0.0);
    }
    arguments.m_interface->request_data->datasubset = SUBSET(); 
    arguments.m_interface->request_data->datasubset.nbound = 0; 


    if (m_plugin == "UDA" && arguments.m_sig_type == SignalType::TIME) {
        // Opportunity to handle time differently
        // Return time SignalType early, no need to scale/offset
        err = imas_json_plugin::uda_helpers::setReturnTimeArray(arguments.m_interface->data_block);
        return err;
    }

    // // scale takes precedence
    // if (m_scale.has_value()) {
    //     err = JMP::map_transform::transform_scale(arguments.m_interface->data_block, m_scale.value());
    // }
    // if (m_offset.has_value()) {
    //     err = JMP::map_transform::transform_offset(arguments.m_interface->data_block, m_offset.value());
    // }

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
