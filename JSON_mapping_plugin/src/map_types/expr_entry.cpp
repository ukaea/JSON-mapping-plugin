#include "map_types/expr_entry.hpp"

template int ExprEntry::eval_expr<float>(IDAM_PLUGIN_INTERFACE* interface, 
        const std::unordered_map<std::string,std::unique_ptr<Mapping>>& entries, 
        const nlohmann::json& global_data) const;

// template int ExprEntry::eval_expr<double>(IDAM_PLUGIN_INTERFACE* interface, 
//         const std::unordered_map<std::string,std::unique_ptr<Mapping>>& entries, 
//         const nlohmann::json& global_data) const;

int ExprEntry::map(IDAM_PLUGIN_INTERFACE* interface, 
        const std::unordered_map<std::string, std::unique_ptr<Mapping>>& entries, 
        const nlohmann::json& global_data) const { 

    // Float only currently for testing purposes
    return eval_expr<float>(interface, entries, global_data);
};
