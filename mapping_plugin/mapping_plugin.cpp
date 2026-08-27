#include "mapping_plugin.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <fstream>
#include <iomanip>
#include <ios>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>

// LibTokaMap includes
#include <libtokamap.hpp>

// UDA includes
#include <clientserver/errorLog.h>
#include <clientserver/initStructs.h>
#include <clientserver/stringUtils.h>
#include <clientserver/udaStructs.h>
#include <clientserver/udaTypes.h>
#include <logging/logging.h>
#include <plugins/pluginStructs.h>
#include <plugins/udaPlugin.h>
#include <server/getServerEnvironment.h>

#include "uda_plugin_helpers.hpp"
#include "python_data_source.hpp"

namespace
{

void add_machine_specific_attributes(IDAM_PLUGIN_INTERFACE* plugin_interface, nlohmann::json& attributes)
{
    for (int i = 0; i < plugin_interface->request_data->nameValueList.pairCount; ++i) {
        const std::string name = plugin_interface->request_data->nameValueList.nameValue[i].name;
        const std::string value = plugin_interface->request_data->nameValueList.nameValue[i].value;

        char* p_end = nullptr;
        long const i_value = std::strtol(value.c_str(), &p_end, 10);
        if (*p_end == '\0') {
            attributes[name] = i_value;
        } else {
            attributes[name] = value;
        }
    }
}

/**
 * @class MappingPlugin
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
class MappingPlugin
{
  public:
    int entry_handle(IDAM_PLUGIN_INTERFACE* plugin_interface);

    ~MappingPlugin()
    {
        if (m_init) {
            reset(nullptr);
        }
    }

  private:
    int execute(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int init(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int reset(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int get(IDAM_PLUGIN_INTERFACE* plugin_interface);

    static int help(IDAM_PLUGIN_INTERFACE* plugin_interface);
    static int version(IDAM_PLUGIN_INTERFACE* plugin_interface);
    static int build_date(IDAM_PLUGIN_INTERFACE* plugin_interface);
    static int default_method(IDAM_PLUGIN_INTERFACE* plugin_interface);
    static int max_interface_version(IDAM_PLUGIN_INTERFACE* plugin_interface);

    bool m_init = false;
    bool m_python_init = false;
    std::string m_request_function;
    libtokamap::MappingHandler m_mapping_handler;
};

} // anon namespace

/**
 * @brief Initialise the libtokamap_plugin
 *
 * Set mapping directory and load mapping files into mapping_handler
 * RAISE_PLUGIN_ERROR if JSON mapping file location is not set
 *
 * @param plugin_interface Top-level UDA plugin interface
 * @return errorcode UDA convention to return int errorcode
 * 0 success, !0 failure
 */
int MappingPlugin::init(IDAM_PLUGIN_INTERFACE* plugin_interface)
{
    if (m_init) {
        return 0;
    }

    const char* config_path = getenv("UDA_MAPPING_CONFIG_PATH");
    if (config_path != nullptr) {
        m_mapping_handler.init(std::filesystem::path{config_path});
        // Register any Python data sources / custom functions declared in the
        // MAPPING_PLUGIN_PYTHON_CONFIG file. Starts the embedded interpreter
        // only when such a config exists; no-op otherwise (and a compile-time
        // no-op when built without MAPPING_PLUGIN_PYTHON).
        //
        // Done at most once per process. reset() clears m_init, but the mapping
        // handler keeps its registered data sources (just as it keeps its
        // experiment register, which is why its own init() is a no-op on the
        // second call), so re-running this would re-import the Python modules
        // and reconnect the sources for nothing.
        if (!m_python_init) {
            mapping_plugin::init_python_data_sources_if_configured(m_mapping_handler);
            m_python_init = true;
        }
    } else {
        throw std::runtime_error{"UDA_MAPPING_CONFIG_PATH not specified"};
    }

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
int MappingPlugin::reset(IDAM_PLUGIN_INTERFACE* /*plugin_interface*/) // silence unused warning
{
    if (m_init) {
        // Free Heap & reset counters if initialised
        m_init = false;
    }
    return 0;
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
int MappingPlugin::get(IDAM_PLUGIN_INTERFACE* plugin_interface)
{
    DATA_BLOCK* data_block = plugin_interface->data_block;
    REQUEST_DATA* request_data = plugin_interface->request_data;

    initDataBlock(data_block);
    data_block->rank = 0;
    data_block->dims = nullptr;

    const char* mapping = nullptr;
    const char* path = nullptr;
    int datatype = UDA_TYPE_UNKNOWN;
    int rank = -1;

    FIND_REQUIRED_STRING_VALUE(request_data->nameValueList, mapping)
    FIND_REQUIRED_STRING_VALUE(request_data->nameValueList, path)
    FIND_REQUIRED_INT_VALUE(request_data->nameValueList, datatype);
    if (datatype < 0 || datatype > UDA_TYPE_CAPNP) {
        throw std::runtime_error{"Invalid datatype"};
    }
    FIND_REQUIRED_INT_VALUE(request_data->nameValueList, rank);
    if (rank < 0) {
        throw std::runtime_error{"Invalid rank"};
    }

    nlohmann::json extra_attributes = {};
    add_machine_specific_attributes(plugin_interface, extra_attributes);

    auto libtokamap_type = mapping_plugin::uda_to_libtokamap_map(static_cast<UDA_TYPE>(datatype));

    libtokamap::TypedDataArray array;
    try {
        array = m_mapping_handler.map(mapping, path, libtokamap_type, rank, extra_attributes);
    } catch (const libtokamap::MappingError& e) {
        // When we get logging, will debug log
        return 1;
    }
    mapping_plugin::set_data_block(data_block, array);

    return 0;
}

int MappingPlugin::execute(IDAM_PLUGIN_INTERFACE* plugin_interface)
{
    int return_code = 0;
    if (m_request_function == "help") {
        return_code = MappingPlugin::help(plugin_interface);
    } else if (m_request_function == "version") {
        return_code = MappingPlugin::version(plugin_interface);
    } else if (m_request_function == "builddate") {
        return_code = MappingPlugin::build_date(plugin_interface);
    } else if (m_request_function == "defaultmethod") {
        return_code = MappingPlugin::default_method(plugin_interface);
    } else if (m_request_function == "maxinterfaceversion") {
        return_code = MappingPlugin::max_interface_version(plugin_interface);
    } else if (m_request_function == "read" || m_request_function == "get") {
        return_code = get(plugin_interface);
    } else if (m_request_function == "close") {
        return_code = 0;
    } else {
        throw std::runtime_error{"Unknown function requested"};
    }
    return return_code;
}

/**
 * entry_handle: plugin C++ class entry point from C access
 * and set current function call to plugin
 * @param plugin_interface
 * @return
 */
int MappingPlugin::entry_handle(IDAM_PLUGIN_INTERFACE* plugin_interface)
{
    // set current function
    m_request_function = static_cast<const char*>(plugin_interface->request_data->function);

    // housekeeping
    if (plugin_interface->housekeeping != 0 || m_request_function == "reset") {
        reset(plugin_interface);
        return 0;
    }

    // Initialise
    init(plugin_interface);
    if (m_request_function == "init" || m_request_function == "initialise") {
        return 0;
    }

    return execute(plugin_interface);
}

/**
 * Help: A Description of library functionality
 * @param plugin_interface
 * @return
 */
int MappingPlugin::help(IDAM_PLUGIN_INTERFACE* plugin_interface)
{
    const char* help = "\nMappingPlugin: Add Functions Names, Syntax, and "
                       "Descriptions\n\n";
    const char* desc = "templatePlugin: help = description of this plugin";

    return setReturnDataString(plugin_interface->data_block, help, desc);
}

/**
 * Plugin version
 * @param plugin_interface
 * @return
 */
int MappingPlugin::version(IDAM_PLUGIN_INTERFACE* plugin_interface)
{
    return setReturnDataIntScalar(plugin_interface->data_block, THISPLUGIN_VERSION, "Plugin version number");
}

/**
 * Plugin Build Date
 * @param plugin_interface
 * @return
 */
int MappingPlugin::build_date(IDAM_PLUGIN_INTERFACE* plugin_interface)
{
    return setReturnDataString(plugin_interface->data_block, __DATE__, "Plugin build date");
}

/**
 * Plugin Default Method
 * @param plugin_interface
 * @return
 */
int MappingPlugin::default_method(IDAM_PLUGIN_INTERFACE* plugin_interface)
{
    return setReturnDataString(plugin_interface->data_block, THISPLUGIN_DEFAULT_METHOD, "Plugin default method");
}

/**
 * Plugin Maximum Interface Version
 * @param plugin_interface
 * @return
 */
int MappingPlugin::max_interface_version(IDAM_PLUGIN_INTERFACE* plugin_interface)
{
    return setReturnDataIntScalar(plugin_interface->data_block, THISPLUGIN_MAX_INTERFACE_VERSION,
                                  "Maximum Interface Version");
}

/**
 * @brief Plugin entry function
 *
 * @param plugin_interface
 * @return
 */
[[maybe_unused]] int mappingPlugin(IDAM_PLUGIN_INTERFACE* plugin_interface)
{
    if (plugin_interface->interfaceVersion > THISPLUGIN_MAX_INTERFACE_VERSION) {
        RAISE_PLUGIN_ERROR("Plugin Interface Version Unknown to this plugin: Unable to execute the request!")
    }

    plugin_interface->pluginVersion = THISPLUGIN_VERSION;

    try {
        static MappingPlugin plugin = {};
        return plugin.entry_handle(plugin_interface);
    } catch (const std::exception& ex) {
        RAISE_PLUGIN_ERROR_EX(ex.what(), { concatUdaError(&plugin_interface->error_stack); })
    }
}
