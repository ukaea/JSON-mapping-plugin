#include "DRaFT_data_reader.h"

#include <clientserver/initStructs.h>
#include <clientserver/stringUtils.h>
#include <clientserver/udaStructs.h>
#include <clientserver/udaTypes.h>
#include <logging/logging.h>
#include <plugins/udaPlugin.h>

#ifdef __GNUC__
#include <strings.h>
#endif

#include "nlohmann/json.hpp"
#include <fstream>

class DRaFTDataReaderPlugin {
  public:
    void init(IDAM_PLUGIN_INTERFACE* plugin_interface) {
        REQUEST_DATA* request = plugin_interface->request_data;
        if (!init_ || STR_IEQUALS(request->function, "init") || STR_IEQUALS(request->function, "initialise")) {
            reset(plugin_interface);
            // Initialise plugin
            init_ = true;
        }
    }
    void reset(IDAM_PLUGIN_INTERFACE* plugin_interface) {
        if (!init_) {
            // Not previously initialised: Nothing to do!
            return;
        }
        // Free Heap & reset counters
        init_ = false;
    }

    int help(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int version(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int build_date(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int default_method(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int max_interface_version(IDAM_PLUGIN_INTERFACE* plugin_interface);
    int get(IDAM_PLUGIN_INTERFACE* plugin_interface);

  private:
    int return_DRaFT_data(DATA_BLOCK* data_block, int shot, std::string signal);
    int return_DRaFT_data_time(DATA_BLOCK* data_block, int shot, const std::string& signal);
    nlohmann::json read_json_data(const std::string& signal, int shot);
    bool init_ = false;
};

int DRaFTDataReaderPlugin::get(IDAM_PLUGIN_INTERFACE* interface) {

    //////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////
    DATA_BLOCK* data_block = interface->data_block;
    REQUEST_DATA* request_data = interface->request_data;

    initDataBlock(data_block);
    data_block->rank = 0;
    data_block->dims = nullptr;

    // TODO: put into plugin relevant structure
    int source{0};
    FIND_REQUIRED_INT_VALUE(request_data->nameValueList, source);
    const char* signal{nullptr};
    FIND_REQUIRED_STRING_VALUE(request_data->nameValueList, signal);
    std::string const signal_str{signal};
    bool time_sig{false}; // False by default
    // FIND_INT_VALUE(request_data->nameValueList, time_sig); // TO FIX
    if (signal_str.find("time") != std::string::npos) {
        time_sig = true;
    }

    // (0) parse needed arguments
    // (1) access experiment data
    // (2) deduce rank + type (if applicable)
    // (3) set return data (may be dependent on time or data)
    return time_sig ? return_DRaFT_data_time(interface->data_block, source, signal_str)
                    : return_DRaFT_data(interface->data_block, source, signal_str);
}

int DRaFTDataReaderPlugin::return_DRaFT_data_time(DATA_BLOCK* data_block, int shot, const std::string& signal) {

    const auto data = read_json_data(signal, shot);
    auto vec_values = data.get<std::vector<float>>();
    const size_t shape{vec_values.size()};
    int err = setReturnDataFloatArray(data_block, vec_values.data(), 1, &shape, nullptr);
    // Cleanup to make things behave
    data_block->order = -1;
    data_block->dims[0].dim = nullptr;
    data_block->dims[0].data_type = UDA_TYPE_UNSIGNED_INT;
    data_block->dims[0].compressed = 1;
    data_block->dims[0].method = 0;
    data_block->dims[0].dim0 = 0.0;
    data_block->dims[0].diff = 1.0;
    return err;
}

int DRaFTDataReaderPlugin::return_DRaFT_data(DATA_BLOCK* data_block, int shot, std::string signal) {

    int err{1};
    const auto data = read_json_data(signal, shot);
    const auto type = read_json_data(signal + "_type", shot).get<std::string>();
    const auto rank = read_json_data(signal + "_rank", shot).get<int>();

    const std::unordered_map<std::string, UDA_TYPE> UDA_TYPE_MAP{
        {"int", UDA_TYPE_INT}, {"float", UDA_TYPE_FLOAT}, {"string", UDA_TYPE_STRING}};
    // All DRaFT data is rank 1 : other experiments would need to expand
    if (rank > 0) {
        switch (UDA_TYPE_MAP.at(type)) {
        case UDA_TYPE_INT: {
            auto vec_values = data.get<std::vector<int>>();
            const size_t shape{vec_values.size()};
            err = setReturnDataIntArray(data_block, vec_values.data(), rank, &shape, nullptr);
            break;
        }
        case UDA_TYPE_FLOAT: {
            auto vec_values = data.get<std::vector<float>>();
            const size_t shape{vec_values.size()};
            err = setReturnDataFloatArray(data_block, vec_values.data(), rank, &shape, nullptr);
            break;
        }
        default:
            break;
        }

    } else if (rank == 0) {
        switch (UDA_TYPE_MAP.at(type)) {
        case UDA_TYPE_INT: {
            auto value = data.get<int>();
            err = setReturnDataIntScalar(data_block, value, nullptr);
            break;
        }
        case UDA_TYPE_FLOAT: {
            auto value = data.get<float>();
            err = setReturnDataFloatScalar(data_block, value, nullptr);
            break;
        }
        case UDA_TYPE_STRING: {
            auto value = data.get<std::string>();
            err = setReturnDataString(data_block, value.c_str(), nullptr);
            break;
        }
        default:
            break;
        }
    }

    return err;
}

nlohmann::json DRaFTDataReaderPlugin::read_json_data(const std::string& signal, int shot) {

    std::string const map_dir = getenv("DRaFT_DATA_DIR");
    std::string const data_path = map_dir + "/" + std::to_string(shot) + ".json";
    std::ifstream json_file(data_path);
    auto temp_json = nlohmann::json::parse(json_file);
    json_file.close();

    try {
        temp_json = temp_json.at(signal);
    } catch (std::exception& e) {
        temp_json = nlohmann::json::parse("[]");
    }

    return temp_json;
}

int DRaFTDataReader(IDAM_PLUGIN_INTERFACE* plugin_interface) {
    //----------------------------------------------------------------------------------------
    // Standard v1 Plugin Interface

    if (plugin_interface->interfaceVersion > THISPLUGIN_MAX_INTERFACE_VERSION) {
        RAISE_PLUGIN_ERROR("Plugin Interface Version Unknown to this plugin: Unable to execute the request!");
    }

    plugin_interface->pluginVersion = THISPLUGIN_VERSION;
    REQUEST_DATA* request = plugin_interface->request_data;

    //----------------------------------------------------------------------------------------
    // Heap Housekeeping

    // Plugin must maintain a list of open file handles and sockets: loop over and close all files and sockets
    // Plugin must maintain a list of plugin functions called: loop over and reset state and free heap.
    // Plugin must maintain a list of calls to other plugins: loop over and call each plugin with the housekeeping
    // request Plugin must destroy lists at end of housekeeping

    // A plugin only has a single instance on a server. For multiple instances, multiple servers are needed.
    // Plugins can maintain state so recursive calls (on the same server) must respect this.
    // If the housekeeping action is requested, this must be also applied to all plugins called.
    // A list must be maintained to register these plugin calls to manage housekeeping.
    // Calls to plugins must also respect access policy and user authentication policy

    try {
        static DRaFTDataReaderPlugin plugin = {};
        auto* const plugin_func = request->function;

        if (plugin_interface->housekeeping || STR_IEQUALS(plugin_func, "reset")) {
            plugin.reset(plugin_interface);
            return 0;
        }

        //----------------------------------------------------------------------------------------
        // Initialise
        plugin.init(plugin_interface);
        if (STR_IEQUALS(plugin_func, "init") || STR_IEQUALS(plugin_func, "initialise")) {
            return 0;
        }

        //----------------------------------------------------------------------------------------
        // Plugin Functions
        //----------------------------------------------------------------------------------------

        //----------------------------------------------------------------------------------------
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
        } else if (STR_IEQUALS(plugin_func, "get")) {
            return plugin.get(plugin_interface);
        } else {
            RAISE_PLUGIN_ERROR("Unknown function requested!");
        }
    } catch (const std::exception& ex) {
        RAISE_PLUGIN_ERROR(ex.what());
    }
}

/**
 * Help: A Description of library functionality
 * @param interface
 * @return
 */
int DRaFTDataReaderPlugin::help(IDAM_PLUGIN_INTERFACE* interface) {
    const char* help = "\ntemplatePlugin: Add Functions Names, Syntax, and Descriptions\n\n";
    const char* desc = "templatePlugin: help = description of this plugin";

    return setReturnDataString(interface->data_block, help, desc);
}

/**
 * Plugin version
 * @param interface
 * @return
 */
int DRaFTDataReaderPlugin::version(IDAM_PLUGIN_INTERFACE* interface) {
    return setReturnDataIntScalar(interface->data_block, THISPLUGIN_VERSION, "Plugin version number");
}

/**
 * Plugin Build Date
 * @param interface
 * @return
 */
int DRaFTDataReaderPlugin::build_date(IDAM_PLUGIN_INTERFACE* interface) {
    return setReturnDataString(interface->data_block, __DATE__, "Plugin build date");
}

/**
 * Plugin Default Method
 * @param interface
 * @return
 */
int DRaFTDataReaderPlugin::default_method(IDAM_PLUGIN_INTERFACE* interface) {
    return setReturnDataString(interface->data_block, THISPLUGIN_DEFAULT_METHOD, "Plugin default method");
}

/**
 * Plugin Maximum Interface Version
 * @param interface
 * @return
 */
int DRaFTDataReaderPlugin::max_interface_version(IDAM_PLUGIN_INTERFACE* interface) {
    return setReturnDataIntScalar(interface->data_block, THISPLUGIN_MAX_INTERFACE_VERSION, "Maximum Interface Version");
}
