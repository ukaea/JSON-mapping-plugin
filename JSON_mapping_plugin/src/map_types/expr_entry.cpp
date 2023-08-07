#include "map_types/expr_entry.hpp"

template int ExprEntry::eval_expr<float>(
    IDAM_PLUGIN_INTERFACE* interface,
    const std::unordered_map<std::string, std::unique_ptr<Mapping>>& entries,
    const nlohmann::json& global_data) const;

// template int ExprEntry::eval_expr<double>(IDAM_PLUGIN_INTERFACE* interface,
//         const std::unordered_map<std::string,std::unique_ptr<Mapping>>&
//         entries, const nlohmann::json& global_data) const;

/**
 * @brief Entry map function, overriden from parent Mapping class
 *
 * @note expression is only of float type for testing purposes
 * @param interface IDAM_PLUGIN_INTERFACE for access to request and data_block
 * @param entries unordered map of all mappings loaded for this experiment and
 * IDS
 * @param global_data global JSON object used in templating
 * @return int error_code
 */
int ExprEntry::map(
    IDAM_PLUGIN_INTERFACE* interface,
    const std::unordered_map<std::string, std::unique_ptr<Mapping>>& entries,
    const nlohmann::json& global_data) const {

    // Float only currently for testing purposes
    return eval_expr<float>(interface, entries, global_data);
};
