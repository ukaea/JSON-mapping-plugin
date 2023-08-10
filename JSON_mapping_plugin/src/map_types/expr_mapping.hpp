#pragma once

#include "map_types/base_mapping.hpp"
#include "utils/uda_plugin_helpers.hpp"

#include <algorithm>
#include <clientserver/initStructs.h>
#include <clientserver/udaStructs.h>
#include <exprtk/exprtk.hpp>
#include <inja/inja.hpp>
#include <plugins/pluginStructs.h>
#include <unordered_map>

/**
 * @class ExprMapping
 * @brief ExprMapping class to the hold the EXPR MAP_TYPE after parsing from the
 * JSON mapping file
 *
 * The class holds an expression std::string 'm_expr' for evaluation and
 * computation when mapping. The string is evaluated by the templating library
 * pantor/inja on object creation. 'm_parameters' holds the std::strings of the
 * variables needed to evaluate the expression, eg. X+Y requires knowledge or
 * data retrieval of X and Y.
 *
 * The evaluation and computation of the expression is done using the expression
 * toolkit library exprtk, this is done in the templated function 'eval_expr'.
 * The needed parameters are required to be entries in the JSON mapping file for
 * the current IDS. Retrieval of the data is done as if the mapping was being
 * retrieved regardless of the expression operation.
 *
 */
class ExprMapping : public Mapping {
  public:
    ExprMapping() = delete;
    ExprMapping(std::string expr, std::unordered_map<std::string, std::string> parameters)
        : m_expr{std::move(expr)}, m_parameters{std::move(parameters)} {};

    int map(const MapArguments& arguments) const override;

  private:
    std::string m_expr;
    std::unordered_map<std::string, std::string> m_parameters;

    template <typename T>
    int eval_expr(const MapArguments& arguments) const;
};

/**
 * @brief Function to
 * (1) perform the evaulation and computation of the expression string
 * using the exprtk library
 * (2) output the data in the correct format to the data_block
 *
 * @tparam T expression parameters template type, in theory the expression can
 * be evaluated with any floating-point type. However this is currently
 * hard-coded to use float.
 *
 * @param out_interface IDAM_PLUGIN_INTERFACE for access to request and
 * data_block
 * @param entries unordered map of all mappings loaded for this experiment and
 * IDS
 * @param global_data global JSON object used in templating
 * @return int error_code
 */
template <typename T>
int ExprMapping::eval_expr(const MapArguments& arguments) const {

    exprtk::symbol_table<T> symbol_table;
    exprtk::expression<T> expression;
    exprtk::parser<T> parser;

    // Copy original request name-value list to map (simplicity)
    std::unordered_map<std::string, std::string> orig_nv_list_map;
    const auto* orig_nv_list = &arguments.m_interface->request_data->nameValueList;
    for (int i = 0; i < orig_nv_list->pairCount; i++) {
        orig_nv_list_map.insert(
            {orig_nv_list->nameValue[i].name, orig_nv_list->nameValue[i].value});
    }

    std::vector<char*> parameters_ptrs(m_parameters.size());
    bool vector_expr{false};
    bool first_vec_param{true};
    size_t result_size{1};

    symbol_table.add_constants();
    for (const auto& [key, json_name] : m_parameters) {

        initDataBlock(arguments.m_interface->data_block); // Reset datablock per param
        arguments.m_entries.at(json_name)->map(arguments);

        // No data for expr parameters, cannot evaluate, return 1;
        if (!arguments.m_interface->data_block->data) {
            return 1;
        }

        if (arguments.m_interface->data_block->data_n > 0) {
            symbol_table.add_vector(
                key, reinterpret_cast<T*>(arguments.m_interface->data_block->data),
                arguments.m_interface->data_block->data_n);
            if (first_vec_param) {
                result_size = arguments.m_interface->data_block->data_n;
                first_vec_param = false;
            }
            vector_expr = true;
        } else {
            symbol_table.add_variable(
                key, *reinterpret_cast<T*>(arguments.m_interface->data_block->data));
        }
        // Collect vectors for deletion later
        parameters_ptrs.push_back(arguments.m_interface->data_block->data);
    }

    std::vector<T> result(result_size);
    if (vector_expr) {
        symbol_table.add_vector("RESULT", result);
    } else {
        symbol_table.add_variable("RESULT", result.front());
    }
    expression.register_symbol_table(symbol_table);

    // replace patterns in expression if necessary, e.g. expression: RESULT:=X+Y
    std::string expr_string{"RESULT:=" + inja::render(m_expr, arguments.m_global_data)};
    parser.compile(expr_string, expression);
    expression.value(); // Evaluate expression

    if (vector_expr) {
        imas_json_plugin::uda_helpers::setReturnDataArrayType_Vec(arguments.m_interface->data_block, result);
    } else {
        imas_json_plugin::uda_helpers::setReturnDataScalarType(arguments.m_interface->data_block, result.at(0));
    }

    // Free parameter memory from subsequent data_block requests
    for (auto& ptr : parameters_ptrs) {
        free(ptr);
        ptr = nullptr;
    }

    return 0;
};
