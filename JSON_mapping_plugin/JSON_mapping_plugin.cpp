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

#include <boost/algorithm/string.hpp>
#include <clientserver/initStructs.h>
#include <clientserver/stringUtils.h>
#include <fstream>

#include <handlers/mapping_handler.hpp>

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
    bool init_ = false;
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
    if (!init_ || STR_IEQUALS(request_data->function, "init") ||
        STR_IEQUALS(request_data->function, "initialise")) {
        reset(plugin_interface);
    }

    m_mapping_handler.init();
    std::string map_dir = getenv("JSON_MAPPING_DIR");
    if (map_dir.empty()) {
        m_mapping_handler.set_map_dir(map_dir);
    } else {
        RAISE_PLUGIN_ERROR(
            "JSONMappingPlugin::init : JSON mapping directory not set");
    }

    return 0;
}

/**
 * @brief 
 *
 * @param plugin_interface 
 * @return 
 */
int JSONMappingPlugin::reset(IDAM_PLUGIN_INTERFACE* plugin_interface) {
    if (init_) {
        // Free Heap & reset counters if initialised
        init_ = false;
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

    std::ofstream my_log_file;
    my_log_file.open("/Users/aparker/UDADevelopment/adam.log",
                     std::ios_base::app);
    my_log_file << "AJP: Hello World" << std::endl;
    my_log_file << "Mapping Dir: " << getenv("JSON_MAPPING_DIR") << std::endl;
    my_log_file.close();

    return setReturnDataIntScalar(data_block, 42, nullptr);

    // const int err{0};
    // return err;
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
