/*---------------------------------------------------------------
 * v0.1 JSON Mappings Plugin:
 *
 * Input Arguments:	IDAM_PLUGIN_INTERFACE *idam_plugin_interface
 *
 * Returns:		0 if the plugin functionality was successful
 *			    otherwise a Error Code is returned
 *
 * Standard functionality:
 *	help	a description of what this plugin does together with a list of
 *functions available reset	frees all previously allocated heap, closes file
 *handles and resets all static parameters. This has the same functionality as
 *setting the housekeeping directive in the plugin interface data structure to
 *TRUE (1) init	Initialise the plugin: read all required data and process.
 *Retain staticly for future reference. read    Entry function for the signal
 *read from the IMAS interface.
 *
 *--------------------------------------------------------------*/
#include "JSON_mapping_plugin.h"
#include "handlers/mapping_handler.hpp"
#include "map_types/base_entry.hpp"

#include <boost/algorithm/string.hpp>
#include <clientserver/initStructs.h>
#include <clientserver/stringUtils.h>
#include <fstream>
#include <server/getServerEnvironment.h>

namespace JSONMapping {

enum class JPLogLevel { DEBUG, INFO, WARNING, ERROR };

int JPLogger(JPLogLevel log_level, std::string_view log_msg) {

    const ENVIRONMENT* environment = getServerEnvironment();

    std::string log_file_name =
        std::string{environment->logdir} + "plugin_logs/JSON_plugin.log";
    std::ofstream jp_log_file;
    jp_log_file.open(log_file_name, std::ios_base::out | std::ios_base::app);
    std::time_t time_now =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto timestamp =
        std::put_time(std::gmtime(&time_now), "%Y-%m-%d:%H:%M:%S");
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
};

}; // namespace JSONMapping

/**
 * @class JSONMappingPlugin
 * @brief
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
    int read(IDAM_PLUGIN_INTERFACE* plugin_interface);

  private:
    bool m_init = false;
    MappingHandler m_mapping_handler;
};

/**
 * @brief
 *
 * @param plugin_interface
 * @return
 */
int JSONMappingPlugin::init(IDAM_PLUGIN_INTERFACE* plugin_interface) {

    REQUEST_DATA* request_data = plugin_interface->request_data;
    if (!m_init || STR_IEQUALS(request_data->function, "init") ||
        STR_IEQUALS(request_data->function, "initialise")) {
        reset(plugin_interface);
    }

    std::string map_dir = getenv("JSON_MAPPING_DIR");
    if (!map_dir.empty()) {
        m_mapping_handler.set_map_dir(map_dir);
    } else {
        RAISE_PLUGIN_ERROR(
            "JSONMappingPlugin::init : JSON mapping directory not set");
    }
    m_mapping_handler.init();

    return 0;
}

/**
 * @brief
 *
 * @param plugin_interface
 * @return
 */
int JSONMappingPlugin::reset(IDAM_PLUGIN_INTERFACE* plugin_interface) {
    if (m_init) {
        // Free Heap & reset counters if initialised
        m_init = false;
    }
    return 0;
}

/**
 * @brief
 *
 * @param idam_plugin_interface
 * @return
 */
int JSONMappingPlugin::read(IDAM_PLUGIN_INTERFACE* idam_plugin_interface) {

    //////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////
    DATA_BLOCK* data_block = idam_plugin_interface->data_block;
    REQUEST_DATA* request_data = idam_plugin_interface->request_data;

    initDataBlock(data_block);
    data_block->rank = 0;
    data_block->dims = nullptr;

    //////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////
    const char* element{nullptr};
    const char* IDS_version{nullptr};
    const char* experiment{nullptr};
    FIND_REQUIRED_STRING_VALUE(request_data->nameValueList, element);
    FIND_REQUIRED_STRING_VALUE(request_data->nameValueList, IDS_version);
    FIND_STRING_VALUE(request_data->nameValueList, experiment);
    std::string element_str{element};

    int run{0};
    int shot{0};
    int dtype{0};
    FIND_INT_VALUE(request_data->nameValueList, run);
    FIND_REQUIRED_INT_VALUE(request_data->nameValueList, shot);
    FIND_REQUIRED_INT_VALUE(request_data->nameValueList, dtype);

    int* indices{nullptr};
    size_t nindices{0};
    FIND_REQUIRED_INT_ARRAY(request_data->nameValueList, indices);
    // Convert int* into std::vector<int>
    std::vector<int> vec_indices(indices, indices + nindices);
    if (nindices == 1 && vec_indices.at(0) == -1) {
        nindices = 0;
        free(indices);
        indices = nullptr;
    }
    //////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////

    std::deque<std::string> split_elem_vec;
    boost::split(split_elem_vec, element_str, boost::is_any_of("/"));
    if (split_elem_vec.empty()) {
        RAISE_PLUGIN_ERROR(
            "ImasMastuPlugin::read: - IDS path could not be split");
    }
    // Use first hash of the IDS path as the IDS name
    std::string current_ids{split_elem_vec.front()};

    // Load mappings based off current_ids name
    // Returns a reference to IDS map objects and corresponding globals
    // Mapping object lifetime owned by mapping_handler
    const auto& [ids_attrs_map, map_entries] =
        m_mapping_handler.read_mappings(current_ids);
    if (map_entries.empty()) {
        RAISE_PLUGIN_ERROR("ImasMastuPlugin::read: - JSON mapping not loaded,\
                 no map entries");
    }

    // Remove IDS name from path and rejoin for hash map key
    // magnetics/coil/#/current -> coil/#/current
    split_elem_vec.pop_front();
    element_str = boost::algorithm::join(split_elem_vec, "/");

    int err = map_entries[element_str]->map(idam_plugin_interface, map_entries,
                                            ids_attrs_map, SignalType::DATA);

    return err;
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
        RAISE_PLUGIN_ERROR("Plugin Interface Version Unknown to this plugin: "
                           "Unable to execute the request!");
    }

    plugin_interface->pluginVersion = THISPLUGIN_VERSION;
    REQUEST_DATA* request_data = plugin_interface->request_data;

    try {
        static JSONMappingPlugin plugin = {};
        const auto plugin_func = request_data->function;

        if (plugin_interface->housekeeping ||
            STR_IEQUALS(plugin_func, "reset")) {
            plugin.reset(plugin_interface);
            return 0;
        }
        //--------------------------------------
        // Initialise
        plugin.init(plugin_interface);
        if (STR_IEQUALS(plugin_func, "init") ||
            STR_IEQUALS(plugin_func, "initialise")) {
            return 0;
        }
        //--------------------------------------
        // Standard methods: version, builddate, defaultmethod,
        // maxinterfaceversion
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
            UDA_LOG(UDA_LOG_DEBUG, "calling read function \n");
            return plugin.read(plugin_interface);
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
 * @param idam_plugin_interface
 * @return
 */
int JSONMappingPlugin::help(IDAM_PLUGIN_INTERFACE* idam_plugin_interface) {
    const char* help = "\nJSONMappingPlugin: Add Functions Names, Syntax, and "
                       "Descriptions\n\n";
    const char* desc = "templatePlugin: help = description of this plugin";

    return setReturnDataString(idam_plugin_interface->data_block, help, desc);
}

/**
 * Plugin version
 * @param idam_plugin_interface
 * @return
 */
int JSONMappingPlugin::version(IDAM_PLUGIN_INTERFACE* idam_plugin_interface) {
    return setReturnDataIntScalar(idam_plugin_interface->data_block,
                                  THISPLUGIN_VERSION, "Plugin version number");
}

/**
 * Plugin Build Date
 * @param idam_plugin_interface
 * @return
 */
int JSONMappingPlugin::build_date(
    IDAM_PLUGIN_INTERFACE* idam_plugin_interface) {
    return setReturnDataString(idam_plugin_interface->data_block, __DATE__,
                               "Plugin build date");
}

/**
 * Plugin Default Method
 * @param idam_plugin_interface
 * @return
 */
int JSONMappingPlugin::default_method(
    IDAM_PLUGIN_INTERFACE* idam_plugin_interface) {
    return setReturnDataString(idam_plugin_interface->data_block,
                               THISPLUGIN_DEFAULT_METHOD,
                               "Plugin default method");
}

/**
 * Plugin Maximum Interface Version
 * @param idam_plugin_interface
 * @return
 */
int JSONMappingPlugin::max_interface_version(
    IDAM_PLUGIN_INTERFACE* idam_plugin_interface) {
    return setReturnDataIntScalar(idam_plugin_interface->data_block,
                                  THISPLUGIN_MAX_INTERFACE_VERSION,
                                  "Maximum Interface Version");
}
