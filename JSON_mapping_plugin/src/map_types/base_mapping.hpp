#pragma once

#include <nlohmann/json.hpp>

#include <clientserver/udaStructs.h>
#include <plugins/pluginStructs.h>
#include <plugins/udaPlugin.h>

#include "map_arguments.hpp"

enum class MappingType { VALUE, PLUGIN, SLICE, EXPR, CUSTOM, DIM };

NLOHMANN_JSON_SERIALIZE_ENUM(MappingType, {{MappingType::VALUE, "VALUE"},
                                           {MappingType::PLUGIN, "PLUGIN"},
                                           {MappingType::SLICE, "SLICE"},
                                           {MappingType::EXPR, "EXPR"},
                                           {MappingType::CUSTOM, "CUSTOM"},
                                           {MappingType::DIM, "DIMENSION"}});

class Mapping {
  public:
    Mapping() = default;
    virtual ~Mapping() = default;
    Mapping(Mapping&& other) = default;
    Mapping(const Mapping& other) = default;
    Mapping& operator=(Mapping&& other) = default;
    Mapping& operator=(const Mapping& other) = default;

    virtual int map(const MapArguments& arguments) const = 0;
};


