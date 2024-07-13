#pragma once

#include "map_types/base_mapping.hpp"

enum class CustomMapType_t { MASTU_helloworld, DRAFT_helloworld, INVALID };
NLOHMANN_JSON_SERIALIZE_ENUM(CustomMapType_t, {{CustomMapType_t::INVALID, nullptr},
                                               {CustomMapType_t::MASTU_helloworld, "MASTU_helloworld"},
                                               {CustomMapType_t::DRAFT_helloworld, "DRAFT_helloworld"}})

/**
 * @class CustomMapping
 * @brief Mapping class CustomMapping to hold the CUSTOM MAP_TYPE from the JSON
 * mapping files. Custom functions are able to be executed using the
 * CustomMapType_t variable, any unrecognised string reverts to INVALID type and
 * returns 1
 *
 */
class CustomMapping : public Mapping
{
  public:
    CustomMapping() = delete;
    ~CustomMapping() override = default;
    explicit CustomMapping(CustomMapType_t custom_type) : m_custom_type(custom_type) {};
    [[nodiscard]] int map(const MapArguments& arguments) const override;

  private:
    CustomMapType_t m_custom_type;

    static int MASTU_helloworld(DATA_BLOCK* data_block);
    static int DRAFT_helloworld(DATA_BLOCK* data_block);
};
