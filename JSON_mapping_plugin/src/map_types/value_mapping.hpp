#pragma once

#include "map_types/base_mapping.hpp"

class ValueMapping : public Mapping {
  public:
    ValueMapping() = delete;
    ~ValueMapping() override = default;
    ValueMapping(ValueMapping&&) = default;
    ValueMapping(const ValueMapping&) = default;
    ValueMapping& operator=(ValueMapping&&) = default;
    ValueMapping& operator=(const ValueMapping&) = default;

    explicit ValueMapping(nlohmann::json value) : m_value{std::move(value)} {};
    int map(const MapArguments& arguments) const override;

  private:
    nlohmann::json m_value;
    static int type_deduce_array(DATA_BLOCK* data_block, const nlohmann::json& arrValue) ;
    static int type_deduce_prim(DATA_BLOCK* data_block, const nlohmann::json& numValue, const nlohmann::json& global_data) ;
};