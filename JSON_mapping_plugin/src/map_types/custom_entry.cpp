#include "map_types/custom_entry.hpp"
#include <clientserver/udaStructs.h>
#include <plugins/udaPlugin.h>

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
int CustomEntry::map(IDAM_PLUGIN_INTERFACE* interface,
            const std::unordered_map<std::string, std::unique_ptr<Mapping>>& entries,
            const nlohmann::json& global_data) const {

    int err{1};
    switch (m_custom_type) {
    case CustomMapType_t::MASTU_helloworld:
        err = MASTU_helloworld(interface->data_block);
        break;
    case CustomMapType_t::DRAFT_helloworld:
        err = DRAFT_helloworld(interface->data_block);
        break;
    case CustomMapType_t::INVALID:
        break;
    }

    return err;
}

int CustomEntry::MASTU_helloworld(DATA_BLOCK* data_block) const {
    return setReturnDataString(
            data_block, "Hello World from MASTU", nullptr);
}

int CustomEntry::DRAFT_helloworld(DATA_BLOCK* data_block) const {
    return setReturnDataString(
            data_block, "Hello World from DRAFT", nullptr);
}
