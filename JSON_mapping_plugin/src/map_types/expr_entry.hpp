#pragma once

#include "map_types/base_entry.hpp"
#include "utils/uda_plugin_helpers.hpp"

#include <algorithm>
#include <clientserver/initStructs.h>
#include <clientserver/udaStructs.h>
#include <exprtk/exprtk.hpp>
#include <inja/inja.hpp>
#include <plugins/pluginStructs.h>
#include <unordered_map>

class ExprEntry : public Mapping {
public:
    ExprEntry() = delete;
    ExprEntry(std::string func, std::unordered_map<std::string, std::string> parameters)
        : m_func{std::move(func)}, m_parameters{std::move(parameters)} {};

    int map(IDAM_PLUGIN_INTERFACE* interface, 
            const std::unordered_map<std::string,std::unique_ptr<Mapping>>& entries, 
            const nlohmann::json& global_data) const override;

private:
    std::string m_func;
    std::unordered_map<std::string, std::string> m_parameters;

    template <typename T>
    int eval_expr(IDAM_PLUGIN_INTERFACE* interface,
            const std::unordered_map<std::string,std::unique_ptr<Mapping>>& entries,
            const nlohmann::json& global_data) const;

};

/*template <typename T, typename... Vectors>
std::vector<T> concat(const std::vector<T> v1, Vectors... vectors) {

 std::size_t sizes {v1.size()};
 sizes += (vectors.size() + ...);

 std::vector<T> result{v1};
 result.reserve(sizes);
 (result.insert(result.end(), vectors.begin(), vectors.end()), ...);

 return result;
};*/

template <typename T>
int ExprEntry::eval_expr(IDAM_PLUGIN_INTERFACE* out_interface, 
        const std::unordered_map<std::string,std::unique_ptr<Mapping>>& entries, 
        const nlohmann::json& global_data) const { 
 
    exprtk::symbol_table<T> symbol_table; 
    exprtk::expression<T> expression;
    exprtk::parser<T> parser;

    // Copy original request name-value list to map (simplicity)
    std::unordered_map<std::string, std::string> orig_nvlist_map;
    const auto* orig_nvlist = &out_interface->request_data->nameValueList;
    for(int i=0; i < orig_nvlist->pairCount; i++) {
        orig_nvlist_map.insert({
            orig_nvlist->nameValue[i].name,
            orig_nvlist->nameValue[i].value
        });
    }

    std::vector<char*> parameters_ptrs(m_parameters.size());    
    bool vector_expr{false};
    bool first_vec_param{true};
    size_t result_size{1};

    symbol_table.add_constants();
    for(const auto& [key, json_name] : m_parameters) {

        initDataBlock(out_interface->data_block); // Reset datablock per param
        entries.at(json_name)->set_current_request_data_map(orig_nvlist_map);
        // Should really set data type also
        entries.at(json_name)->map(out_interface,entries,global_data);

        // No data for expr parameters, cannot evaluate, return 1;
        if (!out_interface->data_block->data) { return 1; }

        if(out_interface->data_block->data_n > 0) {
            symbol_table.add_vector(key,
                    reinterpret_cast<T*>(out_interface->data_block->data), 
                    out_interface->data_block->data_n);
            if (first_vec_param) {
                result_size = out_interface->data_block->data_n;
                first_vec_param = false;
            }
            vector_expr = true;
        } else {
            symbol_table.add_variable(key, *reinterpret_cast<T*>(out_interface->data_block->data));

        }
        // Collect vectors for deletion later
        parameters_ptrs.push_back(out_interface->data_block->data);
    }

    std::vector<T> result(result_size);
    if (vector_expr) {
        symbol_table.add_vector("RESULT", result);
    } else {
        symbol_table.add_variable("RESULT", result.front());
    }    
    expression.register_symbol_table(symbol_table);
    
    // replace patterns in expression if necessary, eg expression: RESULT:=X+Y
    std::string expr_string{"RESULT:="+inja::render(m_func, global_data)};
    parser.compile(expr_string, expression);
    expression.value(); // Evaluate expression

    if (vector_expr) {
        imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec(
                out_interface->data_block, result);
    } else {
        imas_json_plugin::uda_helpers::setReturnDataScalarType(
                out_interface->data_block, result.at(0));

    }

    // Free parameter memory from subsequent data_block requests
    for (auto& ptr : parameters_ptrs) {
        free(ptr);
        ptr = nullptr;
    }

    return 0;
};
