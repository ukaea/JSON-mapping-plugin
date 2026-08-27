// Lazy embedded-Python data-source support for the mapping plugin.
//
// Built only when MAPPING_PLUGIN_PYTHON is defined (requires Python3 + NumPy
// headers). See python_data_source.hpp for the rationale and lifetime model.

#include "python_data_source.hpp"

#ifdef MAPPING_PLUGIN_PYTHON

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <Python.h>

// Pull libpython into the global symbol namespace BEFORE Py_Initialize:
// UDA dlopens this plugin RTLD_LOCAL, so without this NumPy's C extensions
// cannot resolve Python symbols at import time.
#include <dlfcn.h>

#include <numpy/arrayobject.h>

#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <toml.hpp>

namespace mapping_plugin
{
namespace
{

// ---------------------------------------------------------------------------
// Python error reporting
// ---------------------------------------------------------------------------

std::string fetch_python_error()
{
    if (PyErr_Occurred() == nullptr) {
        return {};
    }
    PyObject *ptype = nullptr, *pvalue = nullptr, *ptraceback = nullptr;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);
    std::string message{"unknown Python error"};
    if (pvalue != nullptr) {
        PyObject* str = PyObject_Str(pvalue);
        if (str != nullptr) {
            const char* text = PyUnicode_AsUTF8(str);
            if (text != nullptr) {
                message = text;
            }
            Py_DECREF(str);
        }
    }
    Py_XDECREF(ptype);
    Py_XDECREF(pvalue);
    Py_XDECREF(ptraceback);
    return message;
}

// ---------------------------------------------------------------------------
// TypedDataArray -> NumPy (for custom-function inputs). Copying variant.
// ---------------------------------------------------------------------------

void free_wrapped_memory(void* data)
{
    std::free(data);
}

template <typename T, int NPY_TYPE>
PyObject* wrap_array_copy(const libtokamap::TypedDataArray& data)
{
    const auto& shape = data.shape();
    int ndim = static_cast<int>(shape.size());
    std::vector<npy_intp> dims(shape.begin(), shape.end());

    const size_t bytes = data.size() * sizeof(T);
    // malloc(0) may legitimately return nullptr, so never ask for zero bytes —
    // that keeps "nullptr means allocation failure" true for empty arrays too.
    void* copied = std::malloc(bytes != 0 ? bytes : 1);
    if (copied == nullptr) {
        PyErr_NoMemory();
        return nullptr;
    }
    if (bytes != 0) {
        std::memcpy(copied, data.data<T>(), bytes);
    }

    PyArray_Descr* descr = PyArray_DescrFromType(NPY_TYPE);
    if (descr == nullptr) {
        std::free(copied);
        return nullptr;
    }
    PyObject* array = PyArray_NewFromDescr(&PyArray_Type, descr, ndim, dims.data(), nullptr, copied,
                                           NPY_ARRAY_CARRAY, nullptr);
    if (array == nullptr) {
        std::free(copied);
        return nullptr;
    }
    PyObject* capsule = PyCapsule_New(copied, nullptr, [](PyObject* cap) { std::free(PyCapsule_GetPointer(cap, nullptr)); });
    if (capsule == nullptr) {
        Py_DECREF(array);
        return nullptr;
    }
    PyArray_SetBaseObject(reinterpret_cast<PyArrayObject*>(array), capsule);
    return array;
}

PyObject* typed_data_array_to_numpy(const libtokamap::TypedDataArray& data)
{
    using libtokamap::DataType;
    switch (data.data_type()) {
        case DataType::Double:
            return wrap_array_copy<double, NPY_FLOAT64>(data);
        case DataType::Float:
            return wrap_array_copy<float, NPY_FLOAT32>(data);
        case DataType::Int64:
            return wrap_array_copy<int64_t, NPY_INT64>(data);
        case DataType::Int32:
            return wrap_array_copy<int32_t, NPY_INT32>(data);
        case DataType::Int16:
            return wrap_array_copy<int16_t, NPY_INT16>(data);
        case DataType::Int8:
            return wrap_array_copy<int8_t, NPY_INT8>(data);
        case DataType::UInt64:
            return wrap_array_copy<uint64_t, NPY_UINT64>(data);
        case DataType::UInt32:
            return wrap_array_copy<uint32_t, NPY_UINT32>(data);
        case DataType::UInt16:
            return wrap_array_copy<uint16_t, NPY_UINT16>(data);
        case DataType::UInt8:
            return wrap_array_copy<uint8_t, NPY_UINT8>(data);
        default:
            Py_INCREF(Py_None);
            return Py_None;
    }
}

// ---------------------------------------------------------------------------
// Embedded interpreter lifecycle — lazy, once per uda_server process
// ---------------------------------------------------------------------------

std::once_flag g_python_once;

// The thread state Py_Initialize() leaves current on the initialising thread,
// kept only so that the hand-off of the GIL is explicit and greppable.
PyThreadState* g_main_thread_state = nullptr;

// RAII for the GIL. Re-entrant by design: on the thread that has just run
// Py_Initialize() the GIL is already held, so Ensure()/Release() are no-ops.
class GilLock
{
  public:
    GilLock() : m_state{PyGILState_Ensure()} {}
    ~GilLock() { PyGILState_Release(m_state); }

    GilLock(const GilLock&) = delete;
    GilLock& operator=(const GilLock&) = delete;
    GilLock(GilLock&&) = delete;
    GilLock& operator=(GilLock&&) = delete;

  private:
    PyGILState_STATE m_state;
};

// Py_Initialize() returns with the GIL held by the calling thread. Drop it on
// the way out of the initialisation — including via an exception — otherwise no
// Python thread can ever run while the plugin sits idle between requests, and a
// second thread calling PyGILState_Ensure() would block forever.
class MainThreadGilRelease
{
  public:
    explicit MainThreadGilRelease(bool armed) : m_armed{armed} {}
    ~MainThreadGilRelease()
    {
        if (m_armed) {
            g_main_thread_state = PyEval_SaveThread();
        }
    }

    MainThreadGilRelease(const MainThreadGilRelease&) = delete;
    MainThreadGilRelease& operator=(const MainThreadGilRelease&) = delete;
    MainThreadGilRelease(MainThreadGilRelease&&) = delete;
    MainThreadGilRelease& operator=(MainThreadGilRelease&&) = delete;

  private:
    bool m_armed;
};

// Prepends a colon-separated search path to sys.path, keeping the order of its
// entries the way CPython does it — leftmost entry wins.
void prepend_search_path(PyObject* sys_path, const char* search_path)
{
    if (search_path == nullptr || *search_path == '\0') {
        return;
    }
    const std::string paths{search_path};
    Py_ssize_t index = 0;
    size_t pos = 0;
    while (pos <= paths.size()) {
        const size_t sep = paths.find(':', pos);
        const std::string entry = paths.substr(pos, sep == std::string::npos ? sep : sep - pos);
        if (!entry.empty()) {
            PyObject* pyentry = PyUnicode_FromString(entry.c_str());
            if (pyentry != nullptr) {
                if (PyList_Insert(sys_path, index, pyentry) == 0) {
                    ++index;
                }
                Py_DECREF(pyentry);
            }
        }
        if (sep == std::string::npos) {
            break;
        }
        pos = sep + 1;
    }
}

void ensure_python_interpreter()
{
    std::call_once(g_python_once, [] {
        bool initialised_here = false;
        if (Py_IsInitialized() == 0) {
            // Default the libpython path from the compile-time Python, but
            // allow the deployment to override it.
            const char* libpython = std::getenv("MAPPING_PLUGIN_LIBPYTHON");
            if (libpython == nullptr || *libpython == '\0') {
                libpython = MAPPING_PLUGIN_DEFAULT_LIBPYTHON;
            }
            if (libpython != nullptr && *libpython != '\0') {
                if (dlopen(libpython, RTLD_NOW | RTLD_GLOBAL) == nullptr) {
                    throw std::runtime_error{std::string{"failed to load libpython: "} + dlerror()};
                }
            }
            Py_Initialize(); // returns with the GIL held by this thread
            if (Py_IsInitialized() == 0) {
                throw std::runtime_error{"Py_Initialize failed"};
            }
            initialised_here = true;
        }

        // Declared first so it is destroyed last: the GIL taken by
        // Py_Initialize() is handed back only once everything below is done.
        MainThreadGilRelease main_thread_gil{initialised_here};
        // Everything below touches interpreter state, so it needs the GIL held.
        // That includes the path where somebody else (the host process, another
        // UDA plugin) initialised CPython and this thread holds nothing.
        GilLock gil;

        if (initialised_here) {
            // An embedded interpreter sees neither the caller's PYTHONPATH nor
            // the venv paths, so fold both into sys.path explicitly.
            PyObject* sys_path = PySys_GetObject("path"); // borrowed ref
            if (sys_path != nullptr) {
                prepend_search_path(sys_path, std::getenv("PYTHONPATH"));
                // Applied second, so it takes precedence over PYTHONPATH.
                prepend_search_path(sys_path, std::getenv("MAPPING_PLUGIN_PYTHONPATH"));
            }
        }

        if (_import_array() != 0) {
            throw std::runtime_error{"NumPy C API initialisation failed: " + fetch_python_error()};
        }
    });
}

// ---------------------------------------------------------------------------
// Python/C++ value conversions (mirrors clibtokamap)
// ---------------------------------------------------------------------------

// Returns a new reference, or nullptr with the Python error indicator set.
PyObject* json_to_pyobject(const nlohmann::json& value)
{
    if (value.is_object()) {
        PyObject* dict = PyDict_New();
        if (dict == nullptr) {
            return nullptr;
        }
        for (auto& [key, sub] : value.items()) {
            PyObject* py_sub = json_to_pyobject(sub);
            if (py_sub == nullptr) {
                Py_DECREF(dict);
                return nullptr;
            }
            const int rc = PyDict_SetItemString(dict, key.c_str(), py_sub);
            Py_DECREF(py_sub);
            if (rc != 0) {
                Py_DECREF(dict);
                return nullptr;
            }
        }
        return dict;
    }
    if (value.is_array()) {
        PyObject* list = PyList_New(static_cast<Py_ssize_t>(value.size()));
        if (list == nullptr) {
            return nullptr;
        }
        for (size_t i = 0; i < value.size(); ++i) {
            PyObject* item = json_to_pyobject(value[i]);
            if (item == nullptr) {
                Py_DECREF(list); // list dealloc copes with the unfilled slots
                return nullptr;
            }
            PyList_SET_ITEM(list, static_cast<Py_ssize_t>(i), item); // steals
        }
        return list;
    }
    if (value.is_string()) {
        return PyUnicode_FromString(value.get<std::string>().c_str());
    }
    if (value.is_boolean()) {
        return PyBool_FromLong(value.get<bool>() ? 1 : 0);
    }
    if (value.is_number_integer()) {
        return PyLong_FromLongLong(value.get<long long>());
    }
    if (value.is_number_float()) {
        return PyFloat_FromDouble(value.get<double>());
    }
    if (value.is_null()) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(value.dump().c_str());
}

// Builds a Python dict out of any map of string -> nlohmann::json (the mapping
// arguments, a data source's constructor kwargs). Returns a new reference, or
// nullptr with the Python error indicator set.
template <typename JsonMap>
PyObject* json_map_to_pydict(const JsonMap& values)
{
    PyObject* dict = PyDict_New();
    if (dict == nullptr) {
        return nullptr;
    }
    for (const auto& [key, value] : values) {
        PyObject* py_value = json_to_pyobject(value);
        if (py_value == nullptr) {
            Py_DECREF(dict);
            return nullptr;
        }
        const int rc = PyDict_SetItemString(dict, key.c_str(), py_value);
        Py_DECREF(py_value);
        if (rc != 0) {
            Py_DECREF(dict);
            return nullptr;
        }
    }
    return dict;
}

bool numpy_to_typed_data_array(PyObject* object, libtokamap::TypedDataArray& out, std::string& error)
{
    if (PyUnicode_Check(object)) {
        const char* text = PyUnicode_AsUTF8(object);
        if (text == nullptr) {
            error = "failed to convert Python str to UTF-8";
            return false;
        }
        out = libtokamap::TypedDataArray{std::string{text}};
        return true;
    }
    if (PyArray_Check(object) == 0) {
        error = "Python data source must return a NumPy array or str";
        return false;
    }
    auto* array = reinterpret_cast<PyArrayObject*>(object);
    // ISCARRAY_RO, not ISCARRAY: the data is copied below, so a read-only array
    // (np.broadcast_to, an mmap view, a cache deliberately marked read-only) is
    // perfectly usable and must not be rejected for not being writeable.
    if (PyArray_ISCARRAY_RO(array) == 0) {
        error = PyArray_IS_C_CONTIGUOUS(array) == 0
                    ? "Python data source returned a non-C-contiguous NumPy array"
                    : "Python data source returned a misaligned NumPy array";
        return false;
    }
    void* data = PyArray_DATA(array);
    auto size = static_cast<size_t>(PyArray_SIZE(array));
    int rank = PyArray_NDIM(array);
    npy_intp* shape = PyArray_DIMS(array);
    std::vector<size_t> shape_vec(shape, shape + rank);

    // The TypedDataArray(T*, size, shape) constructor copies the data, so the
    // NumPy array can be released as soon as this call returns.
    switch (PyArray_TYPE(array)) {
        case NPY_BOOL: {
            auto* bool_data = reinterpret_cast<npy_bool*>(data);
            std::vector<uint8_t> values(bool_data, bool_data + size);
            out = libtokamap::TypedDataArray{values, shape_vec};
            return true;
        }
        case NPY_INT8:
            out = libtokamap::TypedDataArray{reinterpret_cast<int8_t*>(data), size, shape_vec};
            return true;
        case NPY_INT16:
            out = libtokamap::TypedDataArray{reinterpret_cast<int16_t*>(data), size, shape_vec};
            return true;
        case NPY_INT32:
            out = libtokamap::TypedDataArray{reinterpret_cast<int32_t*>(data), size, shape_vec};
            return true;
        case NPY_INT64:
            out = libtokamap::TypedDataArray{reinterpret_cast<int64_t*>(data), size, shape_vec};
            return true;
        case NPY_UINT8:
            out = libtokamap::TypedDataArray{reinterpret_cast<uint8_t*>(data), size, shape_vec};
            return true;
        case NPY_UINT16:
            out = libtokamap::TypedDataArray{reinterpret_cast<uint16_t*>(data), size, shape_vec};
            return true;
        case NPY_UINT32:
            out = libtokamap::TypedDataArray{reinterpret_cast<uint32_t*>(data), size, shape_vec};
            return true;
        case NPY_UINT64:
            out = libtokamap::TypedDataArray{reinterpret_cast<uint64_t*>(data), size, shape_vec};
            return true;
        case NPY_FLOAT32:
            out = libtokamap::TypedDataArray{reinterpret_cast<float*>(data), size, shape_vec};
            return true;
        case NPY_FLOAT64:
            out = libtokamap::TypedDataArray{reinterpret_cast<double*>(data), size, shape_vec};
            return true;
        default:
            error = "Python data source returned data with unsupported dtype";
            return false;
    }
}

// ---------------------------------------------------------------------------
// PythonDataSource — a libtokamap::DataSource whose get() calls a Python
// object's .get(args) method. Mirrors libtokamap's own (clibtokamap) bridge.
// ---------------------------------------------------------------------------

class PythonDataSource final : public libtokamap::DataSource
{
  public:
    explicit PythonDataSource(PyObject* instance) : m_instance{instance} {}

    ~PythonDataSource() override
    {
        // uda_server exits after its connection so the interpreter is still up
        // in practice, but take the GIL defensively for the decref.
        if (Py_IsInitialized() != 0 && m_instance != nullptr) {
            PyGILState_STATE gil = PyGILState_Ensure();
            Py_DECREF(m_instance);
            PyGILState_Release(gil);
        }
    }

    PythonDataSource(PythonDataSource&&) = delete;
    PythonDataSource& operator=(PythonDataSource&&) = delete;
    PythonDataSource(const PythonDataSource&) = delete;
    PythonDataSource& operator=(const PythonDataSource&) = delete;

    libtokamap::TypedDataArray get(const libtokamap::DataSourceArgs& map_args,
                                   const libtokamap::MapArguments& /*arguments*/,
                                   libtokamap::RamCache* /*ram_cache*/) override
    {
        PyGILState_STATE gil = PyGILState_Ensure();

        libtokamap::TypedDataArray array;
        std::string error;

        PyObject* args_dict = json_map_to_pydict(map_args);
        if (args_dict == nullptr) {
            error = "failed to build the arguments dict: " + fetch_python_error();
        } else {
            PyObject* result = PyObject_CallMethod(m_instance, "get", "O", args_dict);
            Py_DECREF(args_dict);

            if (result == nullptr) {
                error = fetch_python_error();
            } else {
                numpy_to_typed_data_array(result, array, error);
                Py_DECREF(result);
            }
        }
        PyGILState_Release(gil);

        if (!error.empty()) {
            throw libtokamap::DataSourceError{"python data source get failed: " + error};
        }
        return array;
    }

  private:
    PyObject* m_instance;
};

// ---------------------------------------------------------------------------
// PythonCustomFunction — wraps a Python callable as a libtokamap
// LibraryFunctionWrapper: call(inputs, params) -> function(inputs, params).
// ---------------------------------------------------------------------------

class PythonCustomFunction final : public libtokamap::LibraryFunctionWrapper
{
  public:
    explicit PythonCustomFunction(PyObject* function) : m_function{function} {}

    ~PythonCustomFunction() override
    {
        if (Py_IsInitialized() != 0 && m_function != nullptr) {
            PyGILState_STATE gil = PyGILState_Ensure();
            Py_DECREF(m_function);
            PyGILState_Release(gil);
        }
    }

    libtokamap::TypedDataArray operator()(libtokamap::CustomMappingInputs& inputs,
                                          const libtokamap::CustomMappingParams& params) const override
    {
        PyGILState_STATE gil = PyGILState_Ensure();

        libtokamap::TypedDataArray array;
        std::string error;

        PyObject* inputs_dict = PyDict_New();
        if (inputs_dict == nullptr) {
            error = "failed to allocate the inputs dict: " + fetch_python_error();
        }
        for (auto& [key, value] : inputs) {
            if (!error.empty()) {
                break;
            }
            // A failed conversion returns nullptr; handing that to
            // PyDict_SetItemString would dereference it.
            PyObject* py_array = typed_data_array_to_numpy(value);
            if (py_array == nullptr) {
                error = "failed to convert input '" + key + "' to a NumPy array: " + fetch_python_error();
                break;
            }
            const int rc = PyDict_SetItemString(inputs_dict, key.c_str(), py_array);
            Py_DECREF(py_array);
            if (rc != 0) {
                error = "failed to add input '" + key + "' to the inputs dict: " + fetch_python_error();
                break;
            }
        }

        if (error.empty()) {
            PyObject* params_dict = json_to_pyobject(params);
            PyObject* call_args = params_dict == nullptr ? nullptr : PyTuple_Pack(2, inputs_dict, params_dict);
            Py_XDECREF(params_dict);
            if (call_args == nullptr) {
                error = "failed to build the call arguments: " + fetch_python_error();
            } else {
                PyObject* result = PyObject_CallObject(m_function, call_args);
                Py_DECREF(call_args);

                if (result == nullptr) {
                    error = fetch_python_error();
                } else {
                    numpy_to_typed_data_array(result, array, error);
                    Py_DECREF(result);
                }
            }
        }
        Py_XDECREF(inputs_dict);
        PyGILState_Release(gil);

        if (!error.empty()) {
            throw libtokamap::DataSourceError{"python custom function failed: " + error};
        }
        return array;
    }

  private:
    PyObject* m_function;
};

// ---------------------------------------------------------------------------
// Config reading
// ---------------------------------------------------------------------------

struct PythonDataSourceSpec
{
    std::string name;
    std::string module;
    std::string class_name;
    std::unordered_map<std::string, nlohmann::json> args;
};

struct PythonCustomFunctionSpec
{
    std::string library;
    std::string module;
    std::vector<std::string> functions;
};

nlohmann::json load_python_config(const std::filesystem::path& config_path)
{
    std::ifstream stream{config_path};
    if (!stream) {
        throw std::runtime_error{"failed to open MAPPING_PLUGIN_PYTHON_CONFIG: " + config_path.string()};
    }

    if (config_path.extension() == ".toml") {
        try {
            const auto toml_config = toml::parse(stream, config_path.string());
            std::stringstream json_stream;
            json_stream << toml::json_formatter{toml_config};
            return nlohmann::json::parse(json_stream);
        } catch (const toml::parse_error& e) {
            throw std::runtime_error{"failed to parse " + config_path.string() + " as TOML: " + e.what()};
        } catch (const nlohmann::json::parse_error& e) {
            throw std::runtime_error{"failed to convert " + config_path.string() + " from TOML to JSON: " +
                                     e.what()};
        }
    }

    if (config_path.extension() == ".json") {
        try {
            // Keep JSONC compatibility for deployments that use explanatory
            // comments in the plugin config.
            return nlohmann::json::parse(stream, nullptr, true, true);
        } catch (const nlohmann::json::parse_error& e) {
            throw std::runtime_error{"failed to parse " + config_path.string() + " as JSON: " + e.what()};
        }
    }

    throw std::runtime_error{"unsupported MAPPING_PLUGIN_PYTHON_CONFIG extension for " + config_path.string() +
                             "; expected .toml or .json"};
}

// Reads a required non-empty string member. Returns false with `error` set when
// it is missing, not a string, or empty.
bool required_string(const nlohmann::json& object, const std::string& path, const char* key, std::string& out,
                     std::string& error)
{
    const auto member = object.find(key);
    if (member == object.end() || !member->is_string() || member->get<std::string>().empty()) {
        error = path + " requires a non-empty '" + key + "' string";
        return false;
    }
    out = member->get<std::string>();
    return true;
}

bool parse_python_data_sources(const nlohmann::json& config, std::vector<PythonDataSourceSpec>& specs,
                               std::string& error)
{
    const auto table = config.find("python_data_sources");
    if (table == config.end()) {
        return true; // nothing configured
    }
    if (!table->is_object()) {
        error = "python_data_sources must be an object";
        return false;
    }

    for (const auto& [key, node] : table->items()) {
        const std::string path = "python_data_sources." + key;
        if (!node.is_object()) {
            error = path + " must be an object";
            return false;
        }
        PythonDataSourceSpec spec;
        spec.name = key;
        if (!required_string(node, path, "module", spec.module, error)) {
            return false;
        }
        // 'class_name', or the shorter 'class', defaulting to <name>DataSource.
        spec.class_name = spec.name + "DataSource";
        for (const char* alias : {"class", "class_name"}) { // class_name wins: checked last
            const auto member = node.find(alias);
            if (member == node.end()) {
                continue;
            }
            if (!member->is_string() || member->get<std::string>().empty()) {
                error = path + "." + alias + " must be a non-empty string";
                return false;
            }
            spec.class_name = member->get<std::string>();
        }
        const auto args = node.find("args");
        if (args != node.end()) {
            if (!args->is_object()) {
                error = path + ".args must be an object";
                return false;
            }
            // Any JSON value goes through: json_to_pyobject renders nested
            // objects and arrays as dicts and lists.
            for (const auto& [arg_name, value] : args->items()) {
                spec.args[arg_name] = value;
            }
        }
        specs.push_back(std::move(spec));
    }
    return true;
}

bool parse_python_custom_functions(const nlohmann::json& config, std::vector<PythonCustomFunctionSpec>& specs,
                                   std::string& error)
{
    const auto table = config.find("python_custom_functions");
    if (table == config.end()) {
        return true; // nothing configured
    }
    if (!table->is_object()) {
        error = "python_custom_functions must be an object";
        return false;
    }

    for (const auto& [key, node] : table->items()) {
        const std::string path = "python_custom_functions." + key;
        if (!node.is_object()) {
            error = path + " must be an object";
            return false;
        }
        PythonCustomFunctionSpec spec;
        spec.library = key;
        if (!required_string(node, path, "module", spec.module, error)) {
            return false;
        }
        const auto functions = node.find("functions");
        if (functions == node.end() || !functions->is_array()) {
            error = path + " requires a 'functions' array";
            return false;
        }
        for (const auto& name : *functions) {
            if (!name.is_string() || name.get<std::string>().empty()) {
                error = path + ".functions must be non-empty strings";
                return false;
            }
            spec.functions.push_back(name.get<std::string>());
        }
        specs.push_back(std::move(spec));
    }
    return true;
}

} // namespace

void init_python_data_sources_if_configured(libtokamap::MappingHandler& mapping_handler)
{
    const char* config_env = std::getenv("MAPPING_PLUGIN_PYTHON_CONFIG");
    if (config_env == nullptr || *config_env == '\0') {
        return; // no Python config declared anywhere — interpreter stays off
    }
    const std::filesystem::path config_path{config_env};
    if (!std::filesystem::exists(config_path)) {
        throw std::runtime_error{"MAPPING_PLUGIN_PYTHON_CONFIG points to a missing file: " + config_path.string()};
    }

    const nlohmann::json config = load_python_config(config_path);
    if (!config.is_object()) {
        throw std::runtime_error{config_path.string() + " must contain a configuration object at the top level"};
    }

    std::vector<PythonDataSourceSpec> source_specs;
    std::vector<PythonCustomFunctionSpec> function_specs;
    std::string error;
    if (!parse_python_data_sources(config, source_specs, error) ||
        !parse_python_custom_functions(config, function_specs, error)) {
        throw std::runtime_error{error};
    }
    if (source_specs.empty() && function_specs.empty()) {
        return; // nothing Python-backed configured — interpreter stays off
    }

    ensure_python_interpreter();

    PyGILState_STATE gil = PyGILState_Ensure();
    for (const auto& spec : source_specs) {
        PyObject* module = PyImport_ImportModule(spec.module.c_str());
        if (module == nullptr) {
            std::string message = fetch_python_error();
            PyGILState_Release(gil);
            throw std::runtime_error{"failed to import '" + spec.module + "': " + message};
        }
        PyObject* cls = PyObject_GetAttrString(module, spec.class_name.c_str());
        Py_DECREF(module);
        if (cls == nullptr) {
            std::string message = fetch_python_error();
            PyGILState_Release(gil);
            throw std::runtime_error{"failed to find class '" + spec.class_name + "' in " + spec.module + ": " +
                                     message};
        }
        PyObject* kwargs = json_map_to_pydict(spec.args);
        if (kwargs == nullptr) {
            std::string message = fetch_python_error();
            Py_DECREF(cls);
            PyGILState_Release(gil);
            throw std::runtime_error{"failed to build the arguments for " + spec.module + "." + spec.class_name + ": " +
                                     message};
        }
        PyObject* empty_args = PyTuple_New(0);
        PyObject* instance = empty_args == nullptr ? nullptr : PyObject_Call(cls, empty_args, kwargs);
        Py_XDECREF(empty_args);
        Py_DECREF(kwargs);
        Py_DECREF(cls);
        if (instance == nullptr) {
            std::string message = fetch_python_error();
            PyGILState_Release(gil);
            throw std::runtime_error{"failed to construct " + spec.module + "." + spec.class_name + ": " + message};
        }
        // The registry takes ownership; the Python object stays alive for the
        // process lifetime via the reference held in PythonDataSource.
        //
        // Drop any previous registration of this name first: register_data_source
        // throws on a duplicate, and this function can legitimately run again in
        // the same process (a retry after a partially failed init). erase() is a
        // no-op when the name is absent.
        mapping_handler.unregister_data_source(spec.name);
        mapping_handler.register_data_source(spec.name, std::make_unique<PythonDataSource>(instance));
    }

    for (const auto& spec : function_specs) {
        PyObject* module = PyImport_ImportModule(spec.module.c_str());
        if (module == nullptr) {
            std::string message = fetch_python_error();
            PyGILState_Release(gil);
            throw std::runtime_error{"failed to import '" + spec.module + "': " + message};
        }
        for (const auto& function_name : spec.functions) {
            PyObject* function = PyObject_GetAttrString(module, function_name.c_str());
            if (function == nullptr) {
                std::string message = fetch_python_error();
                Py_DECREF(module);
                PyGILState_Release(gil);
                throw std::runtime_error{"failed to find function '" + function_name + "' in " + spec.module + ": " +
                                         message};
            }
            // As above: drop a previous registration so a repeated init replaces
            // it instead of stacking a second entry that shadows the first.
            // unregister_custom_function throws when there is nothing to drop.
            try {
                mapping_handler.unregister_custom_function(spec.library, function_name);
            } catch (const std::exception&) { // not registered yet — nothing to drop
            }
            mapping_handler.register_custom_function(libtokamap::LibraryFunction{
                spec.library, function_name, std::make_unique<PythonCustomFunction>(function)});
        }
        Py_DECREF(module);
    }
    PyGILState_Release(gil);
}

} // namespace mapping_plugin

#else // !MAPPING_PLUGIN_PYTHON — stub: no Python dependency compiled in

#include <filesystem>
#include <stdexcept>

namespace mapping_plugin
{

void init_python_data_sources_if_configured(libtokamap::MappingHandler& /*mapping_handler*/)
{
    // Configurations that declare Python data sources cannot run on a build
    // without Python support — fail loudly rather than silently serve nothing.
    const char* config_env = std::getenv("MAPPING_PLUGIN_PYTHON_CONFIG");
    if (config_env == nullptr || *config_env == '\0') {
        return;
    }
    if (std::filesystem::exists(std::filesystem::path{config_env})) {
        throw std::runtime_error{std::string{"MAPPING_PLUGIN_PYTHON_CONFIG is set ("} + config_env +
                                 ") but this plugin was built without MAPPING_PLUGIN_PYTHON; "
                                 "rebuild with -DMAPPING_PLUGIN_PYTHON=ON"};
    }
}

} // namespace mapping_plugin

#endif // MAPPING_PLUGIN_PYTHON
