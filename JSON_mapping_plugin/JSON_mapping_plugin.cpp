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

    std::string const log_file_name = std::string{static_cast<const char*>(environment->logdir)} + "/JSON_plugin.log";
    std::ofstream jp_log_file;
    jp_log_file.open(log_file_name, std::ios_base::out | std::ios_base::app);
    std::time_t const time_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto timestamp = std::put_time(std::gmtime(&time_now), "%Y-%m-%d:%H:%M:%S"); // NOLINT(concurrency-mt-unsafe)
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
    jp_log_file << log_msg << "\n";
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
    int execute(IDAM_PLUGIN_INTERFACE* plugin_interface, const std::string& function);

    int init(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int reset(IDAM_PLUGIN_INTERFACE* plugin_interface);
    static int help(IDAM_PLUGIN_INTERFACE* plugin_interface);
    static int version(IDAM_PLUGIN_INTERFACE* plugin_interface);
    static int build_date(IDAM_PLUGIN_INTERFACE* plugin_interface);
    static int default_method(IDAM_PLUGIN_INTERFACE* plugin_interface);
    static int max_interface_version(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int get(IDAM_PLUGIN_INTERFACE* plugin_interface);

  private:
    bool m_init = false;
    // Loads, controls, stores mapping file lifetime
    MappingHandler m_mapping_handler;
    static SignalType deduce_signal_type(std::string_view element_back_str);
    static std::pair<std::vector<int>, std::deque<std::string>>
    extract_indices(const std::deque<std::string>& path_tokens);
    static int add_machine_specific_attributes(IDAM_PLUGIN_INTERFACE* plugin_interface, nlohmann::json& attributes);
    static std::string generate_map_path(std::deque<std::string>& path_tokens, IDSMapRegister_t& mappings);
};

std::pair<std::vector<int>, std::deque<std::string>>
JSONMappingPlugin::extract_indices(const std::deque<std::string>& path_tokens) {
    std::vector<int> indices;
    std::deque<std::string> processed_tokens;

    for (const auto& token : path_tokens) {
        size_t const pos = token.find('[');
        if (pos != std::string::npos) {
            auto sub_string = token.substr(pos + 1);
            int const index = std::stoi(sub_string);
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

    std::string const function = static_cast<const char*>(plugin_interface->request_data->function);

    if (!m_init || function == "init" || function == "initialise") {
        reset(plugin_interface);
    }

    std::string const map_dir = getenv("UDA_JSON_MAPPING_DIR"); // NOLINT(concurrency-mt-unsafe)
    if (!map_dir.empty()) {
        m_mapping_handler.set_map_dir(map_dir);
    } else {
        JSONMapping::JPLog(JSONMapping::JPLogLevel::ERROR, "JSONMappingPlugin::init: - JSON mapping locations not set");
        RAISE_PLUGIN_ERROR("JSONMappingPlugin::init: - JSON mapping locations not set")
    }
    m_mapping_handler.init();
    m_init = true;

    return 0;
}

/**
 * @brief
 *
 * @param plugin_interface Top-level UDA plugin interface
 * @return errorcode UDA convention to return int errorcode
 * 0 success, !0 failure
 */
int JSONMappingPlugin::reset(IDAM_PLUGIN_INTERFACE* /*plugin_interface*/) {
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
    } else if (element_back_str == "data") {
        sig_type = SignalType::DATA;
    } else if (element_back_str == "time") {
        sig_type = SignalType::TIME;
    } else if (element_back_str.find("error") != std::string::npos) {
        sig_type = SignalType::ERROR;
    }
    return sig_type;
}

int JSONMappingPlugin::add_machine_specific_attributes(IDAM_PLUGIN_INTERFACE* plugin_interface,
                                                       nlohmann::json& attributes) {

    for (int i = 0; i < plugin_interface->request_data->nameValueList.pairCount; ++i) {
        std::string const name = plugin_interface->request_data->nameValueList.nameValue[i].name;
        std::string const value = plugin_interface->request_data->nameValueList.nameValue[i].value;

        if (name == "mapping" || name == "path" || name == "rank" || name == "shape" || name == "datatype") {
            continue;
        }

        char* p_end = nullptr;
        long const i_value = std::strtol(value.c_str(), &p_end, 10);
        if (*p_end == '\0') {
            attributes[name] = i_value;
        } else {
            attributes[name] = value;
        }
    }

    return 0;
}

std::string JSONMappingPlugin::generate_map_path(std::deque<std::string>& path_tokens, IDSMapRegister_t& mappings) {
    std::string map_path = boost::algorithm::join(path_tokens, "/");
    JSONMapping::JPLog(JSONMapping::JPLogLevel::INFO, map_path);

    // Deduce signal_type
    const auto sig_type = deduce_signal_type(path_tokens.back());
    if (sig_type == SignalType::INVALID) {
        return {}; // Don't throw, go gentle into that good night
    }

    if (mappings.count(map_path) == 0) {
        JSONMapping::JPLog(JSONMapping::JPLogLevel::WARNING, "JSONMappingPlugin::get: - "
                                                             "IDS path not found in JSON mapping file");
        if (sig_type == SignalType::TIME or sig_type == SignalType::DATA) {
            path_tokens.pop_back();
            map_path = boost::algorithm::join(path_tokens, "/");
            if (mappings.count(map_path) == 0) {
                return {};
            }
        } else {
            return {};
        }
    }

    return map_path;
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

    const char* mapping = nullptr;
    const char* path = nullptr;

    FIND_REQUIRED_STRING_VALUE(request_data->nameValueList, mapping)
    FIND_REQUIRED_STRING_VALUE(request_data->nameValueList, path)

    std::deque<std::string> path_tokens;
    boost::split(path_tokens, path, boost::is_any_of("/"));
    if (path_tokens.empty()) {
        JSONMapping::JPLog(JSONMapping::JPLogLevel::ERROR, "JSONMappingPlugin::get: - IDS path could not be split");
        RAISE_PLUGIN_ERROR("JSONMappingPlugin::get: - IDS path could not be split")
    }

    std::vector<int> indices;
    std::tie(indices, path_tokens) = extract_indices(path_tokens);

    // Use first hash of the IDS path as the IDS name
    std::string const ids_name{path_tokens.front()};

    // Use lowercase machine name for find mapping files
    std::string machine_string = mapping;
    boost::to_lower(machine_string);

    // Load mappings based off IDS name
    // Returns a reference to IDS map objects and corresponding globals
    // Mapping object lifetime owned by mapping_handler
    const auto maybe_mappings = m_mapping_handler.read_mappings(machine_string, ids_name);

    if (!maybe_mappings) {
        JSONMapping::JPLog(JSONMapping::JPLogLevel::ERROR,
                           "JSONMappingPlugin::get: - JSON mapping not loaded, no map entries");
        RAISE_PLUGIN_ERROR("JSONMappingPlugin::get: - JSON mapping not loaded, no map entries")
    }

    const auto& [attributes, mappings] = maybe_mappings.value();

    // Remove IDS name from path and rejoin for hash map key
    // magnetics/coil/#/current -> coil/#/current
    path_tokens.pop_front();

    const auto sig_type = deduce_signal_type(path_tokens.back());
    std::string const map_path = generate_map_path(path_tokens, mappings);
    if (map_path.empty()) {
        return 1; // No mapping found, don't throw
    }

    // Add request indices to globals
    attributes["indices"] = indices;

    add_machine_specific_attributes(plugin_interface, attributes);

    MapArguments const map_arguments{plugin_interface, mappings, attributes, sig_type};

    return mappings.at(map_path)->map(map_arguments);
}

int JSONMappingPlugin::execute(IDAM_PLUGIN_INTERFACE* plugin_interface, const std::string& function) {
    int return_code = 0;
    if (function == "help") {
        return_code = JSONMappingPlugin::help(plugin_interface);
    } else if (function == "version") {
        return_code = JSONMappingPlugin::version(plugin_interface);
    } else if (function == "builddate") {
        return_code = JSONMappingPlugin::build_date(plugin_interface);
    } else if (function == "defaultmethod") {
        return_code = JSONMappingPlugin::default_method(plugin_interface);
    } else if (function == "maxinterfaceversion") {
        return_code = JSONMappingPlugin::max_interface_version(plugin_interface);
    } else if (function == "read" || function == "get") {
        return_code = get(plugin_interface);
    } else if (function == "close") {
        return_code = 0;
    } else {
        RAISE_PLUGIN_ERROR("Unknown function requested!")
    }
    return return_code;
}

/**
 * @brief Plugin entry function
 *
 * @param plugin_interface
 * @return
 */
[[maybe_unused]] int jsonMappingPlugin(IDAM_PLUGIN_INTERFACE* plugin_interface) {

    if (plugin_interface->interfaceVersion > THISPLUGIN_MAX_INTERFACE_VERSION) {
        RAISE_PLUGIN_ERROR("Plugin Interface Version Unknown to this plugin: Unable to execute the request!")
    }

    plugin_interface->pluginVersion = THISPLUGIN_VERSION;

    try {
        static JSONMappingPlugin plugin = {};
        std::string const function = plugin_interface->request_data->function;

        if (plugin_interface->housekeeping != 0 || function == "reset") {
            plugin.reset(plugin_interface);
            return 0;
        }

        //--------------------------------------
        // Initialise
        plugin.init(plugin_interface);
        if (function == "init" || function == "initialise") {
            return 0;
        }

        return plugin.execute(plugin_interface, function);
    } catch (const std::exception& ex) {
        RAISE_PLUGIN_ERROR_EX(ex.what(), { concatUdaError(&plugin_interface->error_stack); })
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
