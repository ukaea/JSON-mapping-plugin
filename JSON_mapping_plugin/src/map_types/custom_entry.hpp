#pragma once

#include "map_types/base_entry.hpp"

enum class CustomMapType_t { MASTU_helloworld, DRAFT_helloworld, INVALID };
NLOHMANN_JSON_SERIALIZE_ENUM(
    CustomMapType_t, {{CustomMapType_t::INVALID, nullptr},
                      {CustomMapType_t::MASTU_helloworld, "MASTU_helloworld"},
                      {CustomMapType_t::DRAFT_helloworld, "DRAFT_helloworld"}});

/**
 * @class CustomEntry
 * @brief Mapping class CustomEntry to hold the CUSTOM MAP_TYPE from the JSON
 * mapping files. Custom functions are able to be executed using the
 * CustomMapType_t variable, any unrecognised string reverts to INVALID type and
 * returns 1
 *
 */
class CustomEntry : public Mapping {
  public:
    CustomEntry() = delete;
    ~CustomEntry() override = default;
    explicit CustomEntry(CustomMapType_t custom_type)
        : _custom_type(custom_type){};
    int map(IDAM_PLUGIN_INTERFACE* interface,
            const std::unordered_map<std::string, std::unique_ptr<Mapping>>&
                entries,
            const nlohmann::json& global_data) const override;

  private:
    CustomMapType_t _custom_type;

    int MASTU_helloworld(DATA_BLOCK* data_block) const;
    int DRAFT_helloworld(DATA_BLOCK* data_block) const;
};
