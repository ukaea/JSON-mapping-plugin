/**
 * @file
 * @brief
 */

#include "plugin_mapping.hpp"

#include "utils/scale_offset.hpp"
#include "utils/uda_plugin_helpers.hpp"
#include <clientserver/stringUtils.h>
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
     * generate subset info then remove subset syntax from
     *  request string
     *
     */

    
    IDAM_PLUGIN_INTERFACE* plugin_interface = arguments.m_interface;
    REQUEST_DATA request = *plugin_interface->request_data;

    request.source[0] = '\0';
    strcpy(request.signal, request_str.c_str());
    makeRequestData(&request, *plugin_interface->pluginList, plugin_interface->environment);
    SUBSET datasubset = request.datasubset;
    subset::log_request_status(&request, "request block before interception: ");


    // assume subsetting is requested if the final part of the request string is
    // in sqaure bracktes
    if (request_str.back() == ']' and request_str.rfind('[')!=std::string::npos)
    {
        std::size_t subset_syntax_position = request_str.rfind('[');
        ram_cache::log(ram_cache::LogLevel::INFO, "request before alteration: " + request_str);
        request_str.erase(subset_syntax_position);
        ram_cache::log(ram_cache::LogLevel::INFO, "request after alteration: " + request_str);
    }

    std::string key_found = m_ram_cache->has_entry(request_str) ? "True" : "False";
    ram_cache::log(ram_cache::LogLevel::DEBUG, "key, \"" + request_str + "\" in cache? " + key_found);
    

   /*
    *
    * CACHING GOES HERE
    *
    */
    
    // disbale subsetting one leevel up
    arguments.m_interface->request_data->datasubset = SUBSET(); 
    arguments.m_interface->request_data->datasubset.nbound = 0; 

    // check cache for request string and only get data if it's not already there
    // currently copies whole datablock (data, error, and dims)
    std::optional<DATA_BLOCK*> maybe_db = m_ram_cache->copy_from_cache(request_str, arguments.m_interface->data_block);
    if (maybe_db)
    {
        ram_cache::log(ram_cache::LogLevel::INFO, "Adding cached datablock onto plugin_interface");

        // // currently modifying datablock in-place
        // arguments.m_interface->data_block = maybe_db.value();

        /* 
         * CAN PROBABLY DELETE 
         *
         * but would be nice to get cache read working without changing datablock in-place
         *
         * TODO: change copy_from_cache not to modify data_block in-place
         *
        // extra plugin_interface requirements? 
        // arguments.m_interface->data_block->signal_rec = (SIGNAL*)malloc(sizeof(SIGNAL));
        // initSignal(arguments.m_interface->data_block->signal_rec);
        // arguments.m_interface->data_block->data_system = (DATA_SYSTEM*)malloc(sizeof(DATA_SYSTEM));
        // initDataSystem(arguments.m_interface->data_block->data_system);
        // initSignalDesc(arguments.m_interface->signal_desc);
        // initDataSource(arguments.m_interface->data_source);
         *
         *
         */

        ram_cache::log(ram_cache::LogLevel::INFO, "data on plugin_interface (data_n): " + std::to_string(arguments.m_interface->data_block->data_n));
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
        // TODO: update RamCache interface to add entry in one line only, taking key and datablock as arguments.
        std::shared_ptr<ram_cache::DataEntry> new_cache_entry = m_ram_cache->make_data_entry(arguments.m_interface->data_block);
        m_ram_cache->add(request_str, new_cache_entry);

    }

    // this is the line means the subset block is now populated on the plugin_interface
    // struct. we can choose to either use this for our new dedicated subset method 
    // within this plugin, or let the serverSubset routine do this from the calling 
    // scope in serverGetData.cpp
    arguments.m_interface->request_data->datasubset = datasubset;

    const char* subset_method = getenv("UDA_JSON_MAPPING_SUBSET_METHOD");
    
    // set serverside subsetting as default unless new method is specifically requested. 
    bool use_plugin_subset = (subset_method != nullptr) and (StringIEquals(subset_method, "PLUGIN_SUBSET")); 

    if (datasubset.nbound > 0 and use_plugin_subset)
    {
        // TODO: m_scale not populated when expected here...
        auto scale_value = m_scale.has_value() ? m_scale.value() : 1.0;
        subset::log(subset::LogLevel::INFO, "scale factor is: " + std::to_string(scale_value));
        auto offset_value = m_offset.has_value() ? m_offset.value() : 0.0;
        subset::log(subset::LogLevel::INFO, "offset factor is: " + std::to_string(offset_value));
        subset::apply_subsetting(arguments.m_interface, scale_value, offset_value);
        
        // after plugin-based subset routine we don't want the serverside subsetting 
        // to go ahead after we return from this function. disable by removing
        // subsetting details from the request block.
        arguments.m_interface->request_data->datasubset = SUBSET(); 
        arguments.m_interface->request_data->datasubset.nbound = 0; 
    }
    else 
    {
        // if using serverside subsetting then this is the option to reduce 
        // rank when a dim length is only 1. i.e. vector -> scaler for 1d slice.
        arguments.m_interface->request_data->datasubset.reform = 1;

        /*
         * scale and subset added to subset routine above 
         * so we only have to iterate through the data array once.
         * still need to apply separately if we haven't called subsetting this way
         *
         * TODO: think about dim scaling, which will be required in some instances... 
         */
        // scale takes precedence
        if (m_scale.has_value()) 
        {
            err = JMP::map_transform::transform_scale(arguments.m_interface->data_block, m_scale.value());
        }
        if (m_offset.has_value())
        {
            err = JMP::map_transform::transform_offset(arguments.m_interface->data_block, m_offset.value());
        }
    }


    if (m_plugin == "UDA" && arguments.m_sig_type == SignalType::TIME) {
        // Opportunity to handle time differently
        // Return time SignalType early, no need to scale/offset
        err = imas_json_plugin::uda_helpers::setReturnTimeArray(arguments.m_interface->data_block);
        return err;
    }

    

    return err;
}

int PluginMapping::map(const MapArguments& arguments) const {

    int err = call_plugins(arguments);
    // temporary solution to the slice functionality returning arrays of 1 element
    if (arguments.m_interface->data_block->rank == 1 and arguments.m_interface->data_block->data_n == 1) {
        arguments.m_interface->data_block->rank = 0;

        // imas won't care about order here, but for testing this 
        // avoids a segfault in the client if you try to
        // interrogate the result.time attribute
        arguments.m_interface->data_block->order = -1;
    }
    return err;
}
