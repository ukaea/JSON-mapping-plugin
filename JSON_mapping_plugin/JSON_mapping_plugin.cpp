#include "JSON_mapping_plugin.h"
#include "handlers/mapping_handler.hpp"
#include "map_types/base_mapping.hpp"

#include <boost/algorithm/string.hpp>
#include <fstream>

#include <clientserver/initStructs.h>
#include <clientserver/stringUtils.h>
#include <clientserver/udaTypes.h>
#include <server/getServerEnvironment.h>

namespace JSONMapping {

enum class JPLogLevel { DEBUG, INFO, WARNING, ERROR };

/**
 * @brief Temporary logging function for JSON_mapping_plugin, outputs
 * to UDA_HOME/etc/
 *
 * @param log_level The JPLogLevel (INFO, WARNING, ERROR, DEBUG)
 * @param log_msg The message to be logged
 * @return
 */
int JPLog(JPLogLevel log_level, std::string_view log_msg) {

    const ENVIRONMENT* environment = getServerEnvironment();

    std::string log_file_name = std::string{environment->logdir} + "/JSON_plugin.log";
    std::ofstream jp_log_file;
    jp_log_file.open(log_file_name, std::ios_base::out | std::ios_base::app);
    std::time_t time_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto timestamp = std::put_time(std::gmtime(&time_now), "%Y-%m-%d:%H:%M:%S");
    if (!jp_log_file) {
        return 1;
    }

    switch (log_level) {
    case JPLogLevel::DEBUG:
        jp_log_file << timestamp << ":DEBUG - ";
        break;
    case JPLogLevel::INFO:
        jp_log_file << timestamp << ":INFO - ";
        break;
    case JPLogLevel::WARNING:
        jp_log_file << timestamp << ":WARNING - ";
        break;
    case JPLogLevel::ERROR:
        jp_log_file << timestamp << ":ERROR - ";
        break;
    default:
        jp_log_file << "LOG_LEVEL NOT DEFINED";
    }
    jp_log_file << log_msg << std::endl;
    jp_log_file.close();

    return 0;
}

} // namespace JSONMapping

/**
 * @class JSONMappingPlugin
 * @brief UDA plugin to map Tokamak data
 *
 * UDA plugin to allow the mapping of experimental fusion data to IMAS data
 * format for a given data dictionary version. Mappings are available in
 * JSON format, which are then parsed and used to
 * (1) read data,
 * (2) apply transformations, and
 * (3) return the data in a format the IDS is expecting
 *
 */
class JSONMappingPlugin {

  public:
    int init(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int reset(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int help(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int version(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int build_date(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int default_method(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int max_interface_version(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int get(IDAM_PLUGIN_INTERFACE* plugin_interface);

  private:
    bool m_init = false;
    // Loads, controls, stores mapping file lifetime
    MappingHandler m_mapping_handler;
    SignalType deduce_signal_type(std::string_view element_back_str);
    std::pair<std::vector<int>, std::deque<std::string>> extract_indices(const std::deque<std::string>& path_tokens);
};

std::pair<std::vector<int>, std::deque<std::string>>
JSONMappingPlugin::extract_indices(const std::deque<std::string>& path_tokens) {
    std::vector<int> indices;
    std::deque<std::string> processed_tokens;

    for (const auto& token : path_tokens) {
        size_t pos = token.find("[");
        if (pos != std::string::npos) {
            auto sub_string = token.substr(pos + 1);
            int index = std::stoi(sub_string);
            auto new_token = token.substr(0, pos) + "[#]";
            indices.push_back(index);
            processed_tokens.push_back(new_token);
        } else {
            processed_tokens.push_back(token);
        }
    }

    return {indices, processed_tokens};
}

/**
 * @brief Initialise the JSON_mapping_plugin
 *
 * Set mapping directory and load mapping files into mapping_handler
 * RAISE_PLUGIN_ERROR if JSON mapping file location is not set
 *
 * @param plugin_interface Top-level UDA plugin interface
 * @return errorcode UDA convention to return int errorcode
 * 0 success, !0 failure
 */
int JSONMappingPlugin::init(IDAM_PLUGIN_INTERFACE* plugin_interface) {

    REQUEST_DATA* request_data = plugin_interface->request_data;
    if (!m_init || STR_IEQUALS(request_data->function, "init") || STR_IEQUALS(request_data->function, "initialise")) {
        reset(plugin_interface);
    }

    std::string map_dir = getenv("JSON_MAPPING_DIR");
    if (!map_dir.empty()) {
        m_mapping_handler.set_map_dir(map_dir);
    } else {
        JSONMapping::JPLog(JSONMapping::JPLogLevel::ERROR, "JSONMappingPlugin::init: - JSON mapping locations not set");
        RAISE_PLUGIN_ERROR("JSONMappingPlugin::init: - JSON mapping locations not set");
    }
    m_mapping_handler.init();

    return 0;
}

/**
 * @brief
 *
 * @param plugin_interface Top-level UDA plugin interface
 * @return errorcode UDA convention to return int errorcode
 * 0 success, !0 failure
 */
int JSONMappingPlugin::reset(IDAM_PLUGIN_INTERFACE* plugin_interface) {
    if (m_init) {
        // Free Heap & reset counters if initialised
        m_init = false;
    }
    return 0;
}

/**
 * @brief Deduce the type of signal being requested/mapped,
 * currently using string comparisons
 *
 * @param element_back_str requested IDS path suffix (eg. data, time, error).
 * @note if no string is supplied, SignalType set to invalid
 * @return SignalType Enum class containing the current signal type
 * [DEFAULT, INVALID, DATA, TIME, ERROR]
 */
SignalType JSONMappingPlugin::deduce_signal_type(std::string_view element_back_str) {

    // SignalType useful in determining for MAST-U
    SignalType sig_type{SignalType::DEFAULT};
    if (element_back_str.empty()) {
        UDA_LOG(UDA_LOG_DEBUG, "\nImasMastuPlugin::sig_type_check - Empty element suffix\n");
        sig_type = SignalType::INVALID;
    } else if (!element_back_str.compare("data")) { // implicit conversion
        sig_type = SignalType::DATA;
    } else if (!element_back_str.compare("time")) { // implicit conversion
        sig_type = SignalType::TIME;
    } else if (element_back_str.find("error") != std::string::npos) {
        sig_type = SignalType::ERROR;
    }
    return sig_type;
}

/**
 * @brief Main data/mapping function called from class entry function
 *
 * Arguments:
 *  - machine   string      the name of the machine to map data for
 *  - path      string      the IDS path we need to map data for
 *  - rank      int         the rank of the data expected
 *  - shape     int array   the shape of the data expected
 *  - datatype  UDA_TYPE    the type of the data expected
 *  - <machine specific args> any remaining arguments are specific to the machine and have been passed via query
 *    arguments on the URI, i.e. imas://server/uda?machine=MASTU&shot=30420&run=1 would pass shot and run as additional
 *    arguments
 *
 * @param plugin_interface Top-level UDA plugin interface
 * @return UDA convention to return int error code (0 success, !0 failure)
 */
int JSONMappingPlugin::get(IDAM_PLUGIN_INTERFACE* plugin_interface) {

    DATA_BLOCK* data_block = plugin_interface->data_block;
    REQUEST_DATA* request_data = plugin_interface->request_data;

    initDataBlock(data_block);
    data_block->rank = 0;
    data_block->dims = nullptr;

    const char* machine = nullptr;
    const char* path = nullptr;

    FIND_REQUIRED_STRING_VALUE(request_data->nameValueList, machine);
    FIND_REQUIRED_STRING_VALUE(request_data->nameValueList, path);

    std::deque<std::string> split_elem_vec;
    boost::split(split_elem_vec, path, boost::is_any_of("/"));
    if (split_elem_vec.empty()) {
        JSONMapping::JPLog(JSONMapping::JPLogLevel::ERROR, "JSONMappingPlugin::get: - IDS path could not be split");
        RAISE_PLUGIN_ERROR("JSONMappingPlugin::get: - IDS path could not be split");
    }

    std::vector<int> indices;
    std::tie(indices, split_elem_vec) = extract_indices(split_elem_vec);

    // Use first hash of the IDS path as the IDS name
    std::string current_ids{split_elem_vec.front()};

    // Load mappings based off current_ids name
    // Returns a reference to IDS map objects and corresponding globals
    // Mapping object lifetime owned by mapping_handler
    const auto maybe_mappings = m_mapping_handler.read_mappings(machine, current_ids);

    if (!maybe_mappings) {
        JSONMapping::JPLog(JSONMapping::JPLogLevel::ERROR,
                           "JSONMappingPlugin::get: - JSON mapping not loaded, no map entries");
        RAISE_PLUGIN_ERROR("JSONMappingPlugin::get: - JSON mapping not loaded, no map entries");
    }

    const auto& [ids_attrs_map, map_entries] = maybe_mappings.value();

    // Remove IDS name from path and rejoin for hash map key
    // magnetics/coil/#/current -> coil/#/current
    split_elem_vec.pop_front();
    std::string map_path = boost::algorithm::join(split_elem_vec, "/");
    JSONMapping::JPLog(JSONMapping::JPLogLevel::INFO, map_path);

    // Deduce signal_type
    const auto sig_type = deduce_signal_type(split_elem_vec.back());
    if (sig_type == SignalType::INVALID) {
        return 1; // Don't throw, go gentle into that good night
    }

    if (!map_entries.count(map_path)) { // implicit conversion
        JSONMapping::JPLog(JSONMapping::JPLogLevel::WARNING, "JSONMappingPlugin::get: - "
                                                             "IDS path not found in JSON mapping file");
        if (sig_type == SignalType::TIME or sig_type == SignalType::DATA) {
            split_elem_vec.pop_back();
            map_path = boost::algorithm::join(split_elem_vec, "/");
            if (!map_entries.count(map_path)) { // implicit conversion
                return 1;                       // No mapping found, don't throw
            }
        }
    }

    // Add request indices to globals
    ids_attrs_map["indices"] = indices;

    // TODO: need to make this generic for any machine
    int run{-1};
    int shot{0};
    FIND_INT_VALUE(plugin_interface->request_data->nameValueList, run);
    FIND_REQUIRED_INT_VALUE(plugin_interface->request_data->nameValueList, shot)

    ids_attrs_map["shot"] = indices;
    ids_attrs_map["run"] = indices;

    MapArguments arguments{plugin_interface, map_entries, ids_attrs_map};

    // For mapping object perform mapping
    return map_entries[map_path]->map(arguments);
}

/**
 * @brief Plugin entry function
 *
 * @param plugin_interface
 * @return
 */
int jsonMappingPlugin(IDAM_PLUGIN_INTERFACE* plugin_interface) {

    //----------------------------------------------------------------------------------------
    // Standard v1 Plugin Interface
    if (plugin_interface->interfaceVersion > THISPLUGIN_MAX_INTERFACE_VERSION) {
        RAISE_PLUGIN_ERROR("Plugin Interface Version Unknown to this plugin: Unable to execute the request!");
    }

    plugin_interface->pluginVersion = THISPLUGIN_VERSION;
    REQUEST_DATA* request_data = plugin_interface->request_data;

    try {
        static JSONMappingPlugin plugin = {};
        auto* const plugin_func = request_data->function;

        if (plugin_interface->housekeeping || STR_IEQUALS(plugin_func, "reset")) {
            plugin.reset(plugin_interface);
            return 0;
        }
        //--------------------------------------
        // Initialise
        plugin.init(plugin_interface);
        if (STR_IEQUALS(plugin_func, "init") || STR_IEQUALS(plugin_func, "initialise")) {
            return 0;
        }
        //--------------------------------------
        // Standard methods: version, builddate, defaultmethod, maxinterfaceversion
        if (STR_IEQUALS(plugin_func, "help")) {
            return plugin.help(plugin_interface);
        } else if (STR_IEQUALS(plugin_func, "version")) {
            return plugin.version(plugin_interface);
        } else if (STR_IEQUALS(plugin_func, "builddate")) {
            return plugin.build_date(plugin_interface);
        } else if (STR_IEQUALS(plugin_func, "defaultmethod")) {
            return plugin.default_method(plugin_interface);
        } else if (STR_IEQUALS(plugin_func, "maxinterfaceversion")) {
            return plugin.max_interface_version(plugin_interface);
        } else if (STR_IEQUALS(plugin_func, "read")) {
            UDA_LOG(UDA_LOG_DEBUG, "calling get function (read given) \n");
            return plugin.get(plugin_interface);
        } else if (STR_IEQUALS(plugin_func, "get")) {
            UDA_LOG(UDA_LOG_DEBUG, "calling get function \n");
            return plugin.get(plugin_interface);
        } else if (STR_IEQUALS(plugin_func, "close")) {
            UDA_LOG(UDA_LOG_DEBUG, "calling close function \n");
            return 0;
        } else {
            RAISE_PLUGIN_ERROR("Unknown function requested!");
        }
    } catch (const std::exception& ex) {
        RAISE_PLUGIN_ERROR(ex.what());
    }
}
/**
 * Help: A Description of library functionality
 * @param plugin_interface
 * @return
 */
int JSONMappingPlugin::help(IDAM_PLUGIN_INTERFACE* plugin_interface) {
    const char* help = "\nJSONMappingPlugin: Add Functions Names, Syntax, and "
                       "Descriptions\n\n";
    const char* desc = "templatePlugin: help = description of this plugin";

    return setReturnDataString(plugin_interface->data_block, help, desc);
}

/**
 * Plugin version
 * @param plugin_interface
 * @return
 */
int JSONMappingPlugin::version(IDAM_PLUGIN_INTERFACE* plugin_interface) {
    return setReturnDataIntScalar(plugin_interface->data_block, THISPLUGIN_VERSION, "Plugin version number");
}

/**
 * Plugin Build Date
 * @param plugin_interface
 * @return
 */
int JSONMappingPlugin::build_date(IDAM_PLUGIN_INTERFACE* plugin_interface) {
    return setReturnDataString(plugin_interface->data_block, __DATE__, "Plugin build date");
}

/**
 * Plugin Default Method
 * @param plugin_interface
 * @return
 */
int JSONMappingPlugin::default_method(IDAM_PLUGIN_INTERFACE* plugin_interface) {
    return setReturnDataString(plugin_interface->data_block, THISPLUGIN_DEFAULT_METHOD, "Plugin default method");
}

/**
 * Plugin Maximum Interface Version
 * @param plugin_interface
 * @return
 */
int JSONMappingPlugin::max_interface_version(IDAM_PLUGIN_INTERFACE* plugin_interface) {
    return setReturnDataIntScalar(plugin_interface->data_block, THISPLUGIN_MAX_INTERFACE_VERSION,
                                  "Maximum Interface Version");
}
