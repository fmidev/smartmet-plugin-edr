#include "ApiDefinition.h"
#include "Config.h"
#include "EDRDefs.h"
#include "EDRMetaData.h"
#include "UtilityFunctions.h"
#include "VerticalReference.h"
#include <boost/algorithm/string/join.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace ApiDefinition
{
const char* const TIMESERIES_TITLE = "SmartMet TimeSeries API";
const char* const TIMESERIES_DESCRIPTION =
    "Time series of forecasts and observations at locations, areas and paths. The values to "
    "return are selected with the param option, see the documentation for the parameter names "
    "and functions.";

namespace
{
namespace OA = Spine::OpenAPI;
using JsonValue = ::Json::Value;

// OGC API - EDR version the conformance declaration claims
const char* const EDR_CONF = "http://www.opengis.net/spec/ogcapi-edr-1/1.2/conf/";
const char* const EDR10_CONF = "http://www.opengis.net/spec/ogcapi-edr-1/1.0/conf/";
const char* const COMMON_CONF = "http://www.opengis.net/spec/ogcapi-common-1/1.0/conf/";
const char* const COMMON2_CONF = "http://www.opengis.net/spec/ogcapi-common-2/1.0/conf/";

const char* const TAG_CAPABILITIES = "Capabilities";
const char* const TAG_METADATA = "Collection metadata";
const char* const TAG_DATA = "Data queries";
const char* const TAG_INSTANCES = "Instances";
const char* const TAG_LOCATIONS = "Locations";

// The query types, in the order they are listed in the definition
const std::vector<std::string> query_types{
    "position", "radius", "area", "cube", "trajectory", "corridor"};

// Identifiers accepted for the only supported CRS (see is_crs84 in EDRQueryParams.cpp)
const std::vector<std::string> crs84_aliases{"OGC:CRS84",
                                             "CRS:84",
                                             "CRS84",
                                             "[OGC:CRS84]",
                                             "urn:ogc:def:crs:OGC:1.3:CRS84",
                                             "urn:ogc:def:crs:OGC::CRS84",
                                             "http://www.opengis.net/def/crs/OGC/1.3/CRS84",
                                             "https://www.opengis.net/def/crs/OGC/1.3/CRS84",
                                             "http://www.opengis.net/def/crs/OGC/0/CRS84",
                                             "https://www.opengis.net/def/crs/OGC/0/CRS84"};

const std::vector<std::string> distance_units{"m", "km", "mi"};

// ----------------------------------------------------------------------
// What the definition needs to know about a collection
// ----------------------------------------------------------------------

struct Collection
{
  std::string id;
  std::string title;
  SourceEngine source = SourceEngine::Undefined;
  std::vector<std::string> queries;  // supported data query types, see query_types
  bool locations = false;            // locations query supported
  bool items = false;                // items of an aviation collection
  std::vector<std::string> formats;
  std::string default_format;
  std::vector<std::string> parameters;
  std::vector<std::string> levels;
  std::string level_type;
  std::vector<std::string> instances;  // instance ids, empty if there are none
  std::vector<std::string> location_ids;
};

const EDRMetaData& most_recent_instance(const std::vector<EDRMetaData>& emds)
{
  const EDRMetaData* ret = &emds.front();
  for (const auto& emd : emds)
    if (ret->temporal_extent.origin_time < emd.temporal_extent.origin_time)
      ret = &emd;
  return *ret;
}

// Collections in the same order and with the same rules as the /collections listing
std::vector<Collection> get_collections(const EngineMetaData& metadata)
{
  std::vector<Collection> ret;
  for (const auto& engine_item : metadata.getMetaData())
  {
    for (const auto& producer_item : engine_item.second)
    {
      const auto& emds = producer_item.second;
      if (emds.empty())
        continue;
      const auto& emd = most_recent_instance(emds);
      if (emd.parameter_names.empty())
        continue;

      Collection c;
      c.id = producer_item.first;
      c.source = emd.metadata_source;
      c.title = emd.collection_info_engine.title;
      if (c.title.empty() && emd.collection_info != nullptr)
        c.title = emd.collection_info->title;
      c.levels = emd.vertical_extent.levels;
      c.level_type = emd.vertical_extent.level_type;

      // Cube queries are available without levels too, the bounding box selects the data
      for (const auto& q : query_types)
        if (emd.data_queries.count(q) > 0)
          c.queries.push_back(q);
      c.locations = (emd.data_queries.count("locations") > 0);
      c.items = (emd.data_queries.count("items") > 0);

      c.formats.assign(emd.output_formats.begin(), emd.output_formats.end());
      c.default_format = emd.default_output_format;
      c.parameters.assign(emd.parameter_names.begin(), emd.parameter_names.end());

      if (emds.size() > 1)
      {
        std::set<std::string> ids;
        for (const auto& instance : emds)
          ids.insert(Fmi::to_iso_string(instance.temporal_extent.origin_time));
        c.instances.assign(ids.begin(), ids.end());
      }

      if (emd.locations)
      {
        // 'all' selects all the locations
        c.location_ids.emplace_back("all");
        for (const auto& loc : *emd.locations)
          c.location_ids.push_back(loc.first);
      }

      ret.push_back(std::move(c));
    }
  }
  return ret;
}

// Component names may contain only [A-Za-z0-9._-]
std::string component_name(const std::string& name)
{
  std::string ret = name;
  for (auto& c : ret)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '_' && c != '-')
      c = '_';
  return ret;
}

std::vector<std::string> union_of(const std::vector<Collection>& collections,
                                  std::vector<std::string> Collection::* member)
{
  std::set<std::string> values;
  for (const auto& c : collections)
    values.insert((c.*member).begin(), (c.*member).end());
  return {values.begin(), values.end()};
}

JsonValue string_array_schema(const std::vector<std::string>& values, std::size_t maxEnum)
{
  return OA::arraySchema(OA::enumSchema(values, maxEnum));
}

JsonValue integer_schema(int minimum)
{
  JsonValue ret;
  ret["type"] = "integer";
  ret["minimum"] = minimum;
  return ret;
}

JsonValue number_schema()
{
  JsonValue ret;
  ret["type"] = "number";
  return ret;
}

JsonValue boolean_schema()
{
  JsonValue ret;
  ret["type"] = "boolean";
  return ret;
}

JsonValue binary_schema()
{
  JsonValue ret = OA::stringSchema();
  ret["format"] = "binary";
  return ret;
}

// An FMI extension parameter
JsonValue extension(JsonValue parameter)
{
  parameter["x-fmi-extension"] = true;
  return parameter;
}

std::string mime_type(const std::string& format)
{
  return UtilityFunctions::output_format_mime_type(format);
}

// ----------------------------------------------------------------------
// EDR components
// ----------------------------------------------------------------------

void add_edr_schemas(OA::Document& doc)
{
  using OA::arraySchema;
  using OA::ref;
  using OA::stringSchema;

  JsonValue object;
  object["type"] = "object";

  // Extent of a collection
  JsonValue extent = object;
  auto& ep = extent["properties"];
  ep["spatial"] = object;
  ep["spatial"]["properties"]["bbox"] = arraySchema(arraySchema(number_schema()));
  ep["spatial"]["properties"]["crs"] = stringSchema();
  ep["temporal"] = object;
  ep["temporal"]["properties"]["interval"] = arraySchema(arraySchema(stringSchema()));
  ep["temporal"]["properties"]["values"] = arraySchema(stringSchema());
  ep["temporal"]["properties"]["trs"] = stringSchema();
  ep["vertical"] = object;
  ep["vertical"]["properties"]["interval"] = arraySchema(arraySchema(stringSchema()));
  ep["vertical"]["properties"]["values"] = arraySchema(stringSchema());
  ep["vertical"]["properties"]["vrs"] = stringSchema();
  ep["custom"] = arraySchema(object);
  doc.addComponent("schemas", "extent", extent);

  // Collection (and instance) metadata
  JsonValue collection = object;
  collection["required"].append("id");
  collection["required"].append("links");
  collection["required"].append("extent");
  auto& cp = collection["properties"];
  cp["id"] = stringSchema();
  cp["title"] = stringSchema();
  cp["description"] = stringSchema();
  cp["keywords"] = arraySchema(stringSchema());
  cp["links"] = arraySchema(ref("schemas", "link"));
  cp["extent"] = ref("schemas", "extent");
  cp["data_queries"] = object;
  cp["crs"] = arraySchema(stringSchema());
  cp["output_formats"] = arraySchema(stringSchema());
  cp["parameter_names"] = object;
  cp["parameter_names"]["additionalProperties"] = object;
  doc.addComponent("schemas", "collection", collection);

  JsonValue collections = object;
  collections["required"].append("links");
  collections["required"].append("collections");
  collections["properties"]["links"] = arraySchema(ref("schemas", "link"));
  collections["properties"]["collections"] = arraySchema(ref("schemas", "collection"));
  doc.addComponent("schemas", "collections", collections);

  JsonValue instances = object;
  instances["required"].append("links");
  instances["required"].append("instances");
  instances["properties"]["links"] = arraySchema(ref("schemas", "link"));
  instances["properties"]["instances"] = arraySchema(ref("schemas", "collection"));
  doc.addComponent("schemas", "instances", instances);

  // CoverageJSON document (Coverage or CoverageCollection)
  JsonValue covjson = object;
  covjson["required"].append("type");
  covjson["properties"]["type"] = OA::enumSchema({"Coverage", "CoverageCollection"}, 10);
  covjson["properties"]["domain"] = object;
  covjson["properties"]["parameters"] = object;
  covjson["properties"]["ranges"] = object;
  covjson["properties"]["coverages"] = arraySchema(object);
  covjson["description"] = "CoverageJSON (OGC 21-069r2)";
  doc.addComponent("schemas", "coverageJSON", covjson);

  // GeoJSON document
  JsonValue geojson = object;
  geojson["required"].append("type");
  geojson["properties"]["type"] = OA::enumSchema({"FeatureCollection", "Feature"}, 10);
  geojson["properties"]["features"] = arraySchema(object);
  geojson["properties"]["links"] = arraySchema(ref("schemas", "link"));
  geojson["properties"]["numberMatched"]["type"] = "integer";
  geojson["properties"]["numberReturned"]["type"] = "integer";
  geojson["description"] = "GeoJSON (RFC 7946)";
  doc.addComponent("schemas", "geoJSON", geojson);

  // Error responses specific to EDR
  auto error_response = [](const std::string& description)
  {
    JsonValue ret;
    ret["description"] = description;
    ret["content"]["application/problem+json"]["schema"] = ref("schemas", "exception");
    return ret;
  };
  doc.addComponent(
      "responses", "TooLarge", error_response("The request exceeds the configured limits."));
  doc.addComponent("responses", "Timeout", error_response("The request timed out."));
}

void add_edr_parameters(OA::Document& doc,
                        const std::vector<Collection>& collections,
                        const std::vector<std::string>& crs_values,
                        std::size_t maxEnum)
{
  using OA::parameter;
  using OA::stringSchema;

  std::vector<std::string> ids;
  for (const auto& c : collections)
    ids.push_back(c.id);

  doc.addComponent("parameters",
                   "collectionId",
                   parameter("collectionId",
                             "path",
                             "Identifier of a collection, see /collections",
                             OA::enumSchema(ids, maxEnum, "collections")));
  doc.addComponent(
      "parameters",
      "instanceId",
      parameter("instanceId",
                "path",
                "Identifier of an instance (model run) of the collection, the origin time of the "
                "data, see /collections/{collectionId}/instances",
                stringSchema()));
  doc.addComponent("parameters",
                   "locationId",
                   parameter("locationId",
                             "path",
                             "Identifier of a location of the collection, see "
                             "/collections/{collectionId}/locations",
                             stringSchema()));

  // coords for each kind of query
  doc.addComponent("parameters",
                   "coords.point",
                   parameter("coords",
                             "query",
                             "Position(s) as Well Known Text: POINT(x y), POINT(x y z), "
                             "MULTIPOINT((x y),...) or MULTIPOINTZ((x y z),...), longitude first",
                             stringSchema(),
                             true));
  doc.addComponent("parameters",
                   "coords.area",
                   parameter("coords",
                             "query",
                             "Area as Well Known Text: POLYGON((x y,...)) or "
                             "MULTIPOLYGON(((x y,...)),...), longitude first",
                             stringSchema(),
                             true));
  doc.addComponent(
      "parameters",
      "coords.line",
      parameter("coords",
                "query",
                "Path as Well Known Text: LINESTRING, LINESTRINGZ, LINESTRINGM or LINESTRINGZM, or "
                "the MULTILINESTRING variants. M values are Unix epoch seconds.",
                stringSchema(),
                true));

  doc.addComponent(
      "parameters",
      "datetime",
      parameter(
          "datetime",
          "query",
          "A time (RFC 3339), an interval start/end where either end may be '..', a comma "
          "separated list of times, or a repeating interval Rn/start/duration. Times without a "
          "UTC offset are UTC. By default the time of the data closest to the current time is "
          "used.",
          stringSchema()));
  doc.addComponent(
      "parameters",
      "z",
      parameter("z",
                "query",
                "Vertical level(s): a level, a comma separated list, a range min/max where either "
                "end may be '..', or a repeating interval Rn/first/step. Ignored for collections "
                "without a vertical extent.",
                stringSchema()));
  doc.addComponent(
      "parameters",
      "parameter-name",
      parameter("parameter-name",
                "query",
                "Comma separated list of parameter names, see parameter_names in the collection "
                "metadata",
                OA::arraySchema(stringSchema())));
  doc.addComponent("parameters",
                   "crs",
                   parameter("crs",
                             "query",
                             "Coordinate reference system of the coordinates: WGS84 "
                             "longitude/latitude (the default) or one of the CRSs listed in the "
                             "crs setting of the server.",
                             OA::enumSchema(crs_values, maxEnum)));
  doc.addComponent(
      "parameters",
      "f",
      parameter("f",
                "query",
                "Output format, see output_formats in the collection metadata. Without f the "
                "format is selected with the Accept header, or the collection's default is used. "
                "The format names are case insensitive.",
                OA::enumSchema(union_of(collections, &Collection::formats), maxEnum)));

  doc.addComponent("parameters",
                   "within",
                   parameter("within",
                             "query",
                             "Radius around the position(s), in within-units",
                             number_schema(),
                             true));
  doc.addComponent("parameters",
                   "within-units",
                   parameter("within-units",
                             "query",
                             "Unit of within",
                             OA::enumSchema(distance_units, maxEnum),
                             true));
  doc.addComponent("parameters",
                   "within.optional",
                   parameter("within",
                             "query",
                             "Search radius around the position(s) for the nearest data, in "
                             "within-units",
                             number_schema()));
  doc.addComponent(
      "parameters",
      "within-units.optional",
      parameter(
          "within-units", "query", "Unit of within", OA::enumSchema(distance_units, maxEnum)));
  doc.addComponent("parameters",
                   "corridor-width",
                   parameter("corridor-width",
                             "query",
                             "Width of the corridor, in width-units",
                             number_schema(),
                             true));
  doc.addComponent("parameters",
                   "width-units",
                   parameter("width-units",
                             "query",
                             "Unit of corridor-width",
                             OA::enumSchema(distance_units, maxEnum),
                             true));
  doc.addComponent("parameters",
                   "corridor-height",
                   parameter("corridor-height",
                             "query",
                             "Height of the corridor, in height-units. Only for collections with "
                             "a vertical extent.",
                             number_schema()));
  doc.addComponent("parameters",
                   "height-units",
                   parameter("height-units",
                             "query",
                             "Unit of corridor-height, or of the z values of a cube query",
                             stringSchema()));

  // Interpolation resolutions of the area, cube and corridor queries
  JsonValue resolution = integer_schema(0);
  resolution["maximum"] = 1000;
  const std::string grid_resolution =
      " of the bounding box, including the minimum and the maximum. Requires the other "
      "resolution parameter too. The data is interpolated to a regular longitude/latitude grid "
      "and the CoverageJSON output has a Grid domain. 0 means the native resolution (default). "
      "Valid values: 0 or 2-1000.";
  doc.addComponent("parameters",
                   "resolution-x",
                   parameter("resolution-x",
                             "query",
                             "Number of grid positions along the x-axis" + grid_resolution,
                             resolution));
  doc.addComponent("parameters",
                   "resolution-y",
                   parameter("resolution-y",
                             "query",
                             "Number of grid positions along the y-axis" + grid_resolution,
                             resolution));
  doc.addComponent("parameters",
                   "resolution-z",
                   parameter("resolution-z",
                             "query",
                             "Number of levels from the minimum to the maximum of the z range "
                             "(2-1000), the data is interpolated vertically. 0 means the levels "
                             "of the data.",
                             resolution));
  doc.addComponent("parameters",
                   "corridor-resolution-x",
                   parameter("resolution-x",
                             "query",
                             "Number of positions along the corridor path including both ends "
                             "(2-1000), 0 means the native resolution. Requires resolution-y.",
                             resolution));
  doc.addComponent("parameters",
                   "corridor-resolution-y",
                   parameter("resolution-y",
                             "query",
                             "Number of positions across the corridor from one edge to the other "
                             "(1-1000, 1 is the centre line only), 0 means the native resolution. "
                             "Requires resolution-x.",
                             resolution));
  doc.addComponent("parameters",
                   "corridor-resolution-z",
                   parameter("resolution-z",
                             "query",
                             "Number of levels from the minimum to the maximum of the corridor "
                             "height band (2-1000), the data is interpolated vertically. 0 means "
                             "the levels of the data.",
                             resolution));

  JsonValue bbox = OA::arraySchema(number_schema());
  bbox["minItems"] = 4;
  bbox["maxItems"] = 6;
  doc.addComponent("parameters",
                   "bbox",
                   parameter("bbox",
                             "query",
                             "Bounding box minx,miny,maxx,maxy, or minx,miny,minz,maxx,maxy,maxz "
                             "to give the vertical range too",
                             bbox,
                             true));

  JsonValue limit = integer_schema(1);
  limit["maximum"] = 10000;
  doc.addComponent("parameters",
                   "limit",
                   parameter("limit",
                             "query",
                             "Maximum number of locations to return; larger values are reduced to "
                             "10000. Without limit all locations are returned.",
                             limit));
  doc.addComponent("parameters",
                   "offset",
                   parameter("offset", "query", "Number of locations to skip", integer_schema(0)));

  // Items of the aviation collections
  JsonValue item_bbox = OA::arraySchema(number_schema());
  item_bbox["minItems"] = 4;
  item_bbox["maxItems"] = 4;
  doc.addComponent("parameters",
                   "items.bbox",
                   parameter("bbox",
                             "query",
                             "Bounding box minlon,minlat,maxlon,maxlat of the stations or areas",
                             item_bbox));
  JsonValue item_limit = integer_schema(1);
  item_limit["maximum"] = 10000;
  item_limit["default"] = 10;
  doc.addComponent("parameters",
                   "items.limit",
                   parameter("limit", "query", "Maximum number of items to return", item_limit));
  doc.addComponent("parameters",
                   "items.offset",
                   parameter("offset", "query", "Number of items to skip", integer_schema(0)));
  doc.addComponent("parameters",
                   "itemId",
                   parameter("itemId",
                             "path",
                             "Identifier of a message, ICAO_messagetime_messageid",
                             stringSchema(),
                             true));
  doc.addComponent("parameters",
                   "locationId.query",
                   parameter("locationId",
                             "query",
                             "Location identifier. With it the listing is a data query for the "
                             "location, like /locations/{locationId}.",
                             stringSchema()));

  // FMI extensions
  doc.addComponent(
      "parameters",
      "lang",
      extension(parameter(
          "lang", "query", "Language of the location names and metadata", stringSchema())));
  doc.addComponent(
      "parameters",
      "pretty",
      extension(parameter(
          "pretty", "query", "Indent the JSON output for readability", boolean_schema())));
  for (const char* dim : {"standard_name", "method", "duration", "level"})
    doc.addComponent(
        "parameters",
        dim,
        extension(parameter(dim,
                            "query",
                            std::string("Comma separated values of the custom "
                                        "dimension '") +
                                dim + "', see extent.custom in the collection metadata",
                            OA::arraySchema(stringSchema()))));
  doc.addComponent("parameters",
                   "step",
                   extension(parameter("step",
                                       "query",
                                       "Distance in kilometers between the points sampled along "
                                       "the path",
                                       number_schema())));
  doc.addComponent("parameters",
                   "timestep",
                   extension(parameter("timestep",
                                       "query",
                                       "Time step in minutes, see the timestep custom dimension in "
                                       "the collection metadata",
                                       integer_schema(0))));
}

// The parameters shared by all data queries, as references
void add_common_data_parameters(JsonValue& op)
{
  for (const char* name : {"datetime",
                           "parameter-name",
                           "z",
                           "crs",
                           "f",
                           "standard_name",
                           "method",
                           "duration",
                           "level",
                           "timestep",
                           "lang",
                           "pretty"})
    op["parameters"].append(OA::ref("parameters", name));
}

// Data query responses for the given output formats. The 200 response is shared by all
// operations with the same formats.
void add_data_responses(OA::Document& doc, JsonValue& op, const std::vector<std::string>& formats)
{
  const auto name = component_name("Data." + boost::algorithm::join(formats, "."));
  if (!doc.hasComponent("responses", name))
  {
    JsonValue ok;
    ok["description"] = "The data";
    auto& content = ok["content"];
    for (const auto& format : formats)
    {
      const auto media_type = mime_type(format);
      if (format == COVERAGE_JSON_FORMAT)
        content[media_type]["schema"] = OA::ref("schemas", "coverageJSON");
      else if (format == GEO_JSON_FORMAT)
        content[media_type]["schema"] = OA::ref("schemas", "geoJSON");
      else if (format == IWXXMZIP_FORMAT)
        content[media_type]["schema"] = binary_schema();
      else
        content[media_type]["schema"] = OA::stringSchema();
    }
    if (content.isNull())
      content["application/json"]["schema"]["type"] = "object";
    doc.addComponent("responses", name, ok);
  }

  op["responses"]["200"] = OA::ref("responses", name);
  OA::Document::addErrorResponses(op);
  op["responses"]["406"] = OA::ref("responses", "NotAcceptable");
  op["responses"]["413"] = OA::ref("responses", "TooLarge");
  op["responses"]["503"] = OA::ref("responses", "Timeout");
}

std::string query_summary(const std::string& query)
{
  static const std::map<std::string, std::string> summaries{
      {"position", "Data at the given position(s)"},
      {"radius", "Data within a radius of the given position(s)"},
      {"area", "Data within the given polygon(s)"},
      {"cube", "Data within a bounding box and vertical range"},
      {"trajectory", "Data along a path"},
      {"corridor", "Data within a corridor around a path"}};
  return summaries.at(query);
}

// Query specific parameters, as references
void add_query_parameters(JsonValue& op, const std::string& query)
{
  auto& params = op["parameters"];
  if (query == "position")
  {
    params.append(OA::ref("parameters", "coords.point"));
    params.append(OA::ref("parameters", "within.optional"));
    params.append(OA::ref("parameters", "within-units.optional"));
  }
  else if (query == "radius")
  {
    params.append(OA::ref("parameters", "coords.point"));
    params.append(OA::ref("parameters", "within"));
    params.append(OA::ref("parameters", "within-units"));
  }
  else if (query == "area")
  {
    params.append(OA::ref("parameters", "coords.area"));
    params.append(OA::ref("parameters", "resolution-x"));
    params.append(OA::ref("parameters", "resolution-y"));
  }
  else if (query == "cube")
  {
    params.append(OA::ref("parameters", "bbox"));
    params.append(OA::ref("parameters", "height-units"));
    params.append(OA::ref("parameters", "resolution-x"));
    params.append(OA::ref("parameters", "resolution-y"));
    params.append(OA::ref("parameters", "resolution-z"));
  }
  else if (query == "trajectory")
  {
    params.append(OA::ref("parameters", "coords.line"));
    params.append(OA::ref("parameters", "step"));
  }
  else if (query == "corridor")
  {
    params.append(OA::ref("parameters", "coords.line"));
    params.append(OA::ref("parameters", "corridor-width"));
    params.append(OA::ref("parameters", "width-units"));
    params.append(OA::ref("parameters", "corridor-height"));
    params.append(OA::ref("parameters", "height-units"));
    params.append(OA::ref("parameters", "step"));
    params.append(OA::ref("parameters", "corridor-resolution-x"));
    params.append(OA::ref("parameters", "corridor-resolution-y"));
    params.append(OA::ref("parameters", "corridor-resolution-z"));
  }
}

// Replace a parameter reference with a collection specific parameter
void replace_parameter(JsonValue& op, const std::string& name, const JsonValue& replacement)
{
  for (auto& p : op["parameters"])
    if (p.isMember("$ref") && p["$ref"].asString() == "#/components/parameters/" + name)
      p = replacement;
}

// e.g. getPosition, getInstancePosition_ecmwf_eurooppa_pinta
std::string operation_id(const std::string& prefix, const std::string& what, const std::string& id)
{
  std::string ret = prefix + what;
  if (!what.empty())
    ret[prefix.size()] = static_cast<char>(std::toupper(ret[prefix.size()]));
  if (!id.empty())
    ret += "_" + component_name(id);
  return ret;
}

struct PathPrefix
{
  std::string path;          // e.g. /collections/{collectionId} or /collections/ecmwf
  std::string idPrefix;      // operationId prefix
  JsonValue pathParameters;  // array of parameter objects or references
};

void add_data_query_operation(OA::Document& doc,
                              const PathPrefix& prefix,
                              const std::string& query,
                              const std::vector<std::string>& formats,
                              const std::string& operationSuffix)
{
  auto& op = doc.operation(prefix.path + "/" + query, "get");
  op["tags"].append(TAG_DATA);
  op["summary"] = query_summary(query);
  op["operationId"] = operation_id(prefix.idPrefix, query, operationSuffix);
  op["parameters"] = prefix.pathParameters;
  add_query_parameters(op, query);
  add_common_data_parameters(op);
  add_data_responses(doc, op, formats);
}

void add_locations_operations(OA::Document& doc,
                              const PathPrefix& prefix,
                              const std::vector<std::string>& formats,
                              const std::string& operationSuffix,
                              const JsonValue& locationIdParameter)
{
  auto& list = doc.operation(prefix.path + "/locations", "get");
  list["tags"].append(TAG_LOCATIONS);
  list["summary"] = "Locations of the collection";
  list["description"] =
      "A GeoJSON feature collection of the locations, which can be paged with limit and "
      "offset. With the locationId option the request is a data query for that location.";
  list["operationId"] = operation_id(prefix.idPrefix, "locations", operationSuffix);
  list["parameters"] = prefix.pathParameters;
  list["parameters"].append(OA::ref("parameters", "limit"));
  list["parameters"].append(OA::ref("parameters", "offset"));
  list["parameters"].append(OA::ref("parameters", "locationId.query"));
  add_common_data_parameters(list);
  auto formats_with_geojson = formats;
  if (std::find(formats.begin(), formats.end(), GEO_JSON_FORMAT) == formats.end())
    formats_with_geojson.emplace_back(GEO_JSON_FORMAT);
  add_data_responses(doc, list, formats_with_geojson);

  auto& data = doc.operation(prefix.path + "/locations/{locationId}", "get");
  data["tags"].append(TAG_DATA);
  data["summary"] = "Data at a location of the collection";
  data["operationId"] = operation_id(prefix.idPrefix, "location", operationSuffix);
  data["parameters"] = prefix.pathParameters;
  data["parameters"].append(locationIdParameter);
  add_common_data_parameters(data);
  add_data_responses(doc, data, formats);
}

void add_items_operations(OA::Document& doc,
                          const PathPrefix& prefix,
                          const std::string& operationSuffix)
{
  auto& list = doc.operation(prefix.path + "/items", "get");
  list["tags"].append(TAG_DATA);
  list["summary"] = "Messages of the collection";
  list["description"] =
      "The messages as GeoJSON features selected by datetime (message time, by default the "
      "temporal extent of the collection) and bbox, paged with limit and offset.";
  list["operationId"] = operation_id(prefix.idPrefix, "items", operationSuffix);
  list["parameters"] = prefix.pathParameters;
  list["parameters"].append(OA::ref("parameters", "datetime"));
  list["parameters"].append(OA::ref("parameters", "items.bbox"));
  list["parameters"].append(OA::ref("parameters", "items.limit"));
  list["parameters"].append(OA::ref("parameters", "items.offset"));
  add_data_responses(doc, list, {GEO_JSON_FORMAT});

  auto& item = doc.operation(prefix.path + "/items/{itemId}", "get");
  item["tags"].append(TAG_DATA);
  item["summary"] = "A message of the collection";
  item["operationId"] = operation_id(prefix.idPrefix, "item", operationSuffix);
  item["parameters"] = prefix.pathParameters;
  item["parameters"].append(OA::ref("parameters", "itemId"));
  add_data_responses(doc, item, {GEO_JSON_FORMAT});
}

void add_metadata_operation(OA::Document& doc,
                            const std::string& path,
                            const std::string& tag,
                            const std::string& summary,
                            const std::string& operationId,
                            const JsonValue& parameters,
                            const std::string& schema)
{
  auto& op = doc.operation(path, "get");
  op["tags"].append(tag);
  op["summary"] = summary;
  op["operationId"] = operationId;
  op["parameters"] = parameters;
  op["parameters"].append(OA::ref("parameters", "lang"));
  op["parameters"].append(OA::ref("parameters", "pretty"));
  auto& ok = op["responses"]["200"];
  ok["description"] = summary;
  ok["content"]["application/json"]["schema"] = OA::ref("schemas", schema);
  OA::Document::addErrorResponses(op);
}

// Generic paths with a {collectionId} template, for the features some collection supports
void add_generic_paths(OA::Document& doc, const std::vector<Collection>& collections)
{
  const auto formats = union_of(collections, &Collection::formats);
  const auto queries = union_of(collections, &Collection::queries);
  bool locations = false;
  bool items = false;
  bool instances = false;
  for (const auto& c : collections)
  {
    locations |= c.locations;
    items |= c.items;
    instances |= !c.instances.empty();
  }

  // In the order of query_types
  std::vector<std::string> supported;
  for (const auto& q : query_types)
    if (std::find(queries.begin(), queries.end(), q) != queries.end())
      supported.push_back(q);

  JsonValue collection_param(::Json::arrayValue);
  collection_param.append(OA::ref("parameters", "collectionId"));

  add_metadata_operation(doc,
                         "/collections/{collectionId}",
                         TAG_METADATA,
                         "Metadata of a collection",
                         "getCollection",
                         collection_param,
                         "collection");

  PathPrefix collection{"/collections/{collectionId}", "get", collection_param};
  for (const auto& query : supported)
    add_data_query_operation(doc, collection, query, formats, "");
  if (locations)
    add_locations_operations(doc, collection, formats, "", OA::ref("parameters", "locationId"));
  if (items)
    add_items_operations(doc, collection, "");

  if (!instances)
    return;

  add_metadata_operation(doc,
                         "/collections/{collectionId}/instances",
                         TAG_INSTANCES,
                         "Instances (model runs) of a collection",
                         "getInstances",
                         collection_param,
                         "instances");

  JsonValue instance_params = collection_param;
  instance_params.append(OA::ref("parameters", "instanceId"));
  add_metadata_operation(doc,
                         "/collections/{collectionId}/instances/{instanceId}",
                         TAG_INSTANCES,
                         "Metadata of an instance",
                         "getInstance",
                         instance_params,
                         "collection");

  PathPrefix instance{
      "/collections/{collectionId}/instances/{instanceId}", "getInstance", instance_params};
  for (const auto& query : supported)
    add_data_query_operation(doc, instance, query, formats, "");
  if (locations)
    add_locations_operations(doc, instance, formats, "", OA::ref("parameters", "locationId"));
}

// Literal paths of one collection with the exact values of the collection
void add_collection_paths(OA::Document& doc, const Collection& c, std::size_t maxEnum)
{
  const auto name = component_name(c.id);
  const auto base = "/collections/" + c.id;

  // Collection specific parameters
  doc.addComponent("parameters",
                   name + ".parameter-name",
                   OA::parameter("parameter-name",
                                 "query",
                                 "Comma separated list of parameter names of the collection",
                                 string_array_schema(c.parameters, maxEnum)));
  doc.addComponent("parameters",
                   name + ".f",
                   OA::parameter("f",
                                 "query",
                                 "Output format" + (c.default_format.empty()
                                                        ? std::string()
                                                        : ", the default is " + c.default_format),
                                 OA::enumSchema(c.formats, maxEnum)));
  std::string z_description =
      "Vertical level(s): a level, a comma separated list, a range "
      "min/max where either end may be '..', or a repeating interval "
      "Rn/first/step.";
  if (c.levels.empty())
    z_description = "Ignored, the collection has no vertical extent.";
  else if (c.levels.size() <= maxEnum)
    z_description += " The levels are " + boost::algorithm::join(c.levels, ", ") +
                     (c.level_type.empty() ? "" : " (" + c.level_type + ")") + ".";
  doc.addComponent(
      "parameters", name + ".z", OA::parameter("z", "query", z_description, OA::stringSchema()));
  doc.addComponent(
      "parameters",
      name + ".height-units",
      OA::parameter("height-units",
                    "query",
                    "Unit of corridor-height, or of the z values of a cube query",
                    OA::enumSchema(VerticalReference::height_units(c.level_type), maxEnum)));

  auto specialize = [&](JsonValue& op)
  {
    replace_parameter(op, "parameter-name", OA::ref("parameters", name + ".parameter-name"));
    replace_parameter(op, "f", OA::ref("parameters", name + ".f"));
    replace_parameter(op, "z", OA::ref("parameters", name + ".z"));
    replace_parameter(op, "height-units", OA::ref("parameters", name + ".height-units"));
  };

  const JsonValue no_params(::Json::arrayValue);

  add_metadata_operation(doc,
                         base,
                         TAG_METADATA,
                         "Metadata of collection " + c.id,
                         operation_id("getCollection", "", c.id),
                         no_params,
                         "collection");

  PathPrefix collection{base, "get", no_params};
  for (const auto& query : c.queries)
  {
    add_data_query_operation(doc, collection, query, c.formats, c.id);
    specialize(doc.operation(base + "/" + query, "get"));
  }

  JsonValue location_param = OA::parameter("locationId",
                                           "path",
                                           "Identifier of a location of the collection",
                                           OA::enumSchema(c.location_ids, maxEnum, "locations"));
  if (c.locations)
  {
    add_locations_operations(doc, collection, c.formats, c.id, location_param);
    specialize(doc.operation(base + "/locations", "get"));
    specialize(doc.operation(base + "/locations/{locationId}", "get"));
  }
  if (c.items)
    add_items_operations(doc, collection, c.id);

  if (c.instances.empty())
    return;

  add_metadata_operation(doc,
                         base + "/instances",
                         TAG_INSTANCES,
                         "Instances (model runs) of collection " + c.id,
                         operation_id("getInstances", "", c.id),
                         no_params,
                         "instances");

  JsonValue instance_params(::Json::arrayValue);
  instance_params.append(OA::parameter("instanceId",
                                       "path",
                                       "Identifier of an instance (model run) of the collection",
                                       OA::enumSchema(c.instances, maxEnum, "instances")));
  add_metadata_operation(doc,
                         base + "/instances/{instanceId}",
                         TAG_INSTANCES,
                         "Metadata of an instance of collection " + c.id,
                         operation_id("getInstance", "", c.id),
                         instance_params,
                         "collection");

  const auto instance_base = base + "/instances/{instanceId}";
  PathPrefix instance{instance_base, "getInstance", instance_params};
  for (const auto& query : c.queries)
  {
    add_data_query_operation(doc, instance, query, c.formats, c.id);
    specialize(doc.operation(instance_base + "/" + query, "get"));
  }
  if (c.locations)
  {
    add_locations_operations(doc, instance, c.formats, c.id, location_param);
    specialize(doc.operation(instance_base + "/locations", "get"));
    specialize(doc.operation(instance_base + "/locations/{locationId}", "get"));
  }
}

// ----------------------------------------------------------------------
// TimeSeries parameters
// ----------------------------------------------------------------------

struct TSParameter
{
  const char* name;
  const char* group;  // tag of the parameter group
  const char* type;   // string, integer, number, boolean, array
  const char* description;
};

// clang-format off
const std::vector<TSParameter> ts_parameters{
    // The model first: it determines the available parameters, times and areas. The
    // param option is inserted after these.
    {"producer", "Data", "string", "Producer(s) of the data. A comma separates producers by area, a semicolon by time."},
    {"model", "Data", "string", "Alias of producer"},
    {"source", "Data", "string", "Data source: querydata or grid"},
    // Time
    {"starttime", "Time", "string", "Start time: ISO 8601, YYYYMMDDHHMI, a relative time like +2h or -1d, or 'data' for the first time of the data. The default is the next full hour."},
    {"endtime", "Time", "string", "End time in the formats of starttime, or 'data'. The default is starttime plus 24 hours."},
    {"timestep", "Time", "string", "Time step in minutes, or 'data' to select the times of the data. The default is 60."},
    {"timesteps", "Time", "integer", "Number of times to return, an alternative to endtime"},
    {"startstep", "Time", "integer", "Number of time steps to skip from the start"},
    {"hour", "Time", "array", "Comma separated list of hours to select, e.g. 12,18"},
    {"time", "Time", "array", "Comma separated list of times HHMI to select, e.g. 1200,1230"},
    {"day", "Time", "array", "Comma separated list of days of the month to select"},
    {"weekday", "Time", "array", "Comma separated list of weekdays to select"},
    {"now", "Time", "string", "Override the current time"},
    {"tz", "Time", "string", "Time zone of the request and the response times, the default is 'localtime'"},
    {"origintime", "Time", "string", "Origin time of the model run to use, the latest by default"},
    {"usecurrenttime", "Time", "boolean", "Use the current time instead of the next full hour as the default start time"},
    {"interval", "Time", "string", "Time interval for observation queries"},
    // Locations
    {"place", "Location", "string", "Location name, optionally with a radius in km: place=Helsinki or place=Helsinki:20"},
    {"places", "Location", "array", "Comma separated list of location names"},
    {"lonlat", "Location", "array", "Longitude and latitude, optionally with a radius: lonlat=24.94,60.17 or lonlat=24.94,60.17:15"},
    {"lonlats", "Location", "array", "Comma separated list of longitude,latitude pairs"},
    {"latlon", "Location", "array", "Latitude and longitude: latlon=60.17,24.94"},
    {"latlons", "Location", "array", "Comma separated list of latitude,longitude pairs"},
    {"geoid", "Location", "string", "Location identifier of the location database"},
    {"geoids", "Location", "array", "Comma separated list of location identifiers"},
    {"keyword", "Location", "string", "Locations of a keyword of the location database, e.g. synop_fi"},
    {"inkeyword", "Location", "string", "Restrict location name searches to a keyword"},
    {"feature", "Location", "array", "Feature codes for limiting coordinate searches, e.g. SYNOP or PPL,PPLX"},
    {"area", "Location", "string", "Area name, optionally with a radius: area=Turku or area=Turku:10"},
    {"areas", "Location", "array", "Comma separated list of area names"},
    {"path", "Location", "string", "Path as a list of coordinates, optionally with a radius"},
    {"paths", "Location", "array", "List of paths"},
    {"bbox", "Location", "string", "Bounding box minlon,minlat,maxlon,maxlat, optionally with a radius"},
    {"bboxes", "Location", "array", "List of bounding boxes"},
    {"wkt", "Location", "string", "Geometry as Well Known Text, optionally with a radius"},
    {"x", "Location", "number", "X coordinate in the crs"},
    {"y", "Location", "number", "Y coordinate in the crs"},
    {"crs", "Location", "string", "Coordinate reference system of x and y, e.g. EPSG:3067"},
    {"fmisid", "Location", "array", "Comma separated list of FMI station identifiers"},
    {"wmo", "Location", "array", "Comma separated list of WMO station identifiers"},
    {"lpnn", "Location", "array", "Comma separated list of LPNN station identifiers"},
    {"rwsid", "Location", "array", "Comma separated list of road weather station identifiers"},
    {"sid", "Location", "array", "Comma separated list of station identifiers"},
    {"wsi", "Location", "array", "Comma separated list of WIGOS station identifiers"},
    {"maxdistance", "Location", "string", "Maximum distance to the data or the station, a number with an optional unit (km, m, mi, nmi, ...), e.g. 10km"},
    {"numberofstations", "Location", "integer", "Number of nearest stations to return per location"},
    {"stationtype", "Location", "string", "Observation station type"},
    {"stationgroups", "Location", "array", "Comma separated list of observation station groups"},
    {"groupareas", "Location", "boolean", "Group the results of an area (default 1)"},
    {"grouplocations", "Location", "boolean", "Group the given locations as one area"},
    {"areasource", "Location", "string", "Source of the area geometries"},
    {"step", "Location", "number", "Distance in kilometers between the points sampled along a path"},
    // Data
    {"level", "Data", "string", "Model level"},
    {"levels", "Data", "array", "Comma separated list of model levels"},
    {"levelrange", "Data", "string", "Range of model levels min,max"},
    {"leveltype", "Data", "string", "Type of the levels, e.g. pressure or model"},
    {"pressure", "Data", "number", "Pressure level in hPa"},
    {"pressures", "Data", "array", "Comma separated list of pressure levels in hPa"},
    {"height", "Data", "number", "Height in meters"},
    {"heights", "Data", "array", "Comma separated list of heights in meters"},
    {"findnearestvalid", "Data", "boolean", "Use the nearest grid point with valid data"},
    {"useDataCache", "Data", "boolean", "Use the observation data cache"},
    {"station_id", "Data", "string", "SQL style filter of the observation station identifiers"},
    {"data_quality", "Data", "string", "SQL style filter of the observation quality codes, e.g. le 5"},
    {"sounding_type", "Data", "string", "SQL style filter of the sounding types"},
    {"significance", "Data", "string", "SQL style filter of the sounding level significances"},
    {"lang", "Data", "string", "Language of the location names"},
    {"locale", "Data", "string", "Locale of the formatted values, e.g. fi_FI"},
    // Output
    {"format", "Output", "string", "Output format"},
    {"precision", "Output", "string", "Precision of the values: normal, double, full, auto or another configured name"},
    {"missingtext", "Output", "string", "Text of missing values, the default is nan"},
    {"timeformat", "Output", "string", "Format of the times: iso, timestamp, sql, xml, epoch, iso2, iso8601 or http"},
    {"timestring", "Output", "string", "strftime pattern of the timestring parameter"},
    {"separator", "Output", "string", "Field separator of the ascii format"},
    {"attributes", "Output", "array", "Fields output as attributes in xml, json, php and serial formats"},
    {"xmlstyle", "Output", "string", "Style of the xml format: attributes, tags or mixed"},
    {"version", "Output", "string", "Version of the wxml format"},
    {"startrow", "Output", "integer", "First row to output"},
    {"maxresults", "Output", "integer", "Maximum number of rows to output"},
    {"floatfield", "Output", "string", "Floating point format: none, fixed or scientific"},
    {"width", "Output", "integer", "Width of the numeric fields"},
    {"fill", "Output", "string", "Fill character of the numeric fields"},
    {"adjustfield", "Output", "string", "Alignment of the numeric fields: left, right or internal"},
    {"showpos", "Output", "boolean", "Show the + sign of positive numbers"},
    {"uppercase", "Output", "boolean", "Use uppercase in scientific notation"},
    {"debug", "Output", "boolean", "Return error details as HTML"}};
// clang-format on

// Output formats of the TimeSeries API and their media types
const std::vector<std::pair<std::string, std::string>> ts_formats{{"ascii", "text/plain"},
                                                                  {"csv", "text/csv"},
                                                                  {"debug", "text/html"},
                                                                  {"html", "text/html"},
                                                                  {"json", "application/json"},
                                                                  {"php", "text/plain"},
                                                                  {"serial", "text/plain"},
                                                                  {"wxml", "application/xml"},
                                                                  {"xml", "application/xml"}};

JsonValue ts_schema(const TSParameter& p)
{
  const std::string type = p.type;
  if (type == "array")
    return OA::arraySchema(OA::stringSchema());
  if (type == "integer")
    return integer_schema(0);
  if (type == "number")
    return number_schema();
  if (type == "boolean")
  {
    // SmartMet accepts 0/1 as well as true/false
    JsonValue ret = OA::stringSchema();
    ret["enum"].append("0");
    ret["enum"].append("1");
    ret["enum"].append("true");
    ret["enum"].append("false");
    return ret;
  }
  return OA::stringSchema();
}

}  // namespace

// ----------------------------------------------------------------------
// Landing page and conformance
// ----------------------------------------------------------------------

::Json::Value landingPage(const Config& config, const std::string& root)
{
  try
  {
    const auto& page = config.landingPage();
    JsonValue doc;
    doc["title"] = page.title;
    doc["description"] = page.description;
    if (!page.provider.isNull())
      doc["provider"] = page.provider;
    if (!page.contact.isNull())
      doc["contact"] = page.contact;

    auto& links = doc["links"];
    links.append(OA::link(root, "self", "application/json", "This document"));
    if (config.openAPISettings().enabled)
    {
      links.append(OA::serviceDescLink(root));
      if (config.openAPISettings().ui)
        links.append(OA::serviceDocLink(root));
    }
    links.append(OA::link(
        root + "/conformance", OA::REL_CONFORMANCE, "application/json", "Conformance classes"));
    links.append(
        OA::link(root + "/collections", OA::REL_DATA, "application/json", "Collections of data"));
    return doc;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate the EDR landing page");
  }
}

::Json::Value conformance()
{
  JsonValue doc;
  auto& classes = doc["conformsTo"];
  classes.append(std::string(COMMON_CONF) + "core");
  classes.append(std::string(COMMON2_CONF) + "collections");
  for (const char* name : {"core", "collections", "queries", "json", "geojson", "covjson"})
    classes.append(std::string(EDR_CONF) + name);
  // The EDR 1.0 classes for clients and test suites which look for them
  for (const char* name : {"core", "collections", "json", "geojson", "covjson"})
    classes.append(std::string(EDR10_CONF) + name);
  return doc;
}

// ----------------------------------------------------------------------
// EDR API definition
// ----------------------------------------------------------------------

Spine::OpenAPI::Document edr(const Config& config, const EngineMetaData& metadata)
{
  try
  {
    const auto& settings = config.openAPISettings();
    const auto& page = config.landingPage();
    const auto max_enum = settings.max_enum;

    OA::Document doc(page.title, page.description, page.version, settings);
    doc.addTag(TAG_CAPABILITIES, "The landing page, conformance declaration and API definition");
    doc.addTag(TAG_METADATA, "Metadata of the collections");
    doc.addTag(TAG_DATA, "Data queries");
    doc.addTag(TAG_INSTANCES, "Instances (model runs) of the collections");
    doc.addTag(TAG_LOCATIONS, "Locations of the collections");
    doc.json()["externalDocs"]["description"] = "OGC API - Environmental Data Retrieval";
    doc.json()["externalDocs"]["url"] = "https://ogcapi.ogc.org/edr/";

    const auto collections = get_collections(metadata);

    doc.addCommonComponents();
    add_edr_schemas(doc);
    // CRS84 and the CRSs of the crs setting, by identifier and URI
    std::vector<std::string> crs_values = crs84_aliases;
    for (const auto& crs : config.getCRSRegistry().all())
    {
      crs_values.push_back(crs->id);
      crs_values.push_back(crs->uri);
    }
    add_edr_parameters(doc, collections, crs_values, max_enum);

    doc.addCoreOperations(TAG_CAPABILITIES, JsonValue());

    add_metadata_operation(doc,
                           "/collections",
                           TAG_METADATA,
                           "Metadata of all collections",
                           "getCollections",
                           JsonValue(::Json::arrayValue),
                           "collections");

    add_generic_paths(doc, collections);

    if (settings.expand_collections)
      for (const auto& c : collections)
        add_collection_paths(doc, c, max_enum);

    return doc;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate the EDR API definition");
  }
}

// ----------------------------------------------------------------------
// TimeSeries API definition
// ----------------------------------------------------------------------

Spine::OpenAPI::Document timeseries(const Config& config,
                                    const EngineMetaData& metadata,
                                    const std::string& url)
{
  try
  {
    const auto& settings = config.openAPISettings();
    const auto max_enum = settings.max_enum;

    OA::Document doc(
        TIMESERIES_TITLE, TIMESERIES_DESCRIPTION, config.landingPage().version, settings);
    doc.addTag("TimeSeries", "Time series queries");
    doc.addTag(TAG_CAPABILITIES, "The API definition");
    doc.json()["externalDocs"]["description"] = "Using the TimeSeries API";
    doc.json()["externalDocs"]["url"] =
        "https://github.com/fmidev/smartmet-plugin-edr/blob/master/docs/"
        "Using-the-Timeseries-API.md";

    // Producers of the data sources the TimeSeries API reads
    std::set<std::string> producers;
    for (const auto& engine_item : metadata.getMetaData())
    {
      if (engine_item.first == SourceEngine::Avi)
        continue;
      for (const auto& producer_item : engine_item.second)
        producers.insert(producer_item.first);
    }

    auto& op = doc.operation(url, "get");
    op["tags"].append("TimeSeries");
    op["summary"] = "Time series data";
    op["description"] =
        "Returns the values listed in param for the requested locations and times. At least "
        "one location option and param are required.";
    op["operationId"] = "getTimeSeries" + component_name(url);

    auto& params = op["parameters"];

    JsonValue param_schema = OA::arraySchema(OA::stringSchema());
    param_schema["examples"].append("name,time,temperature");
    auto param = OA::parameter(
        "param",
        "query",
        "Comma separated list of the values to return: data parameter names of the producer, "
        "meta parameters such as name, time, lat, lon, fmisid, and functions such as "
        "mean_t(Temperature/24h/0h). See the documentation for the full syntax.",
        param_schema,
        true);
    param["x-meta-parameters"] = JsonValue(::Json::arrayValue);
    for (const char* meta :
         {"name",       "time",   "origintime", "lat",       "lon",         "latlon",  "lonlat",
          "geoid",      "fmisid", "wmo",        "elevation", "localtz",     "utctime", "localtime",
          "timestring", "level",  "model",      "producer",  "stationname", "distance"})
      param["x-meta-parameters"].append(meta);

    for (const auto& p : ts_parameters)
    {
      // The values to return follow the model, which determines what is available
      if (std::strcmp(p.group, "Time") == 0 && param.isObject())
      {
        params.append(param);
        param = JsonValue();
      }

      JsonValue schema;
      const std::string name = p.name;
      if (name == "producer" || name == "model")
      {
        // Producers have aliases and may be combined, so the names are not an enum
        schema = OA::stringSchema();
        if (producers.size() <= max_enum)
          for (const auto& producer : producers)
            schema["x-known-values"].append(producer);
      }
      else if (name == "format")
      {
        std::vector<std::string> formats;
        for (const auto& f : ts_formats)
          formats.push_back(f.first);
        schema = OA::enumSchema(formats, max_enum);
        schema["default"] = "ascii";
      }
      else
        schema = ts_schema(p);

      auto parameter = OA::parameter(name, "query", p.description, schema);
      parameter["x-group"] = p.group;
      if (name == "keyword" || name == "inkeyword")
      {
        // The documentation page shows the locations of the keyword on a map
        auto& locations = parameter["x-smartmet-locations"];
        locations["path"] = url;
        locations["parameter"] = "keyword";
        locations["language"] = "lang";
        locations["query"]["param"] = "name,geoid,fmisid,wmo,lat,lon,elevation,region,country,tz";
        locations["query"]["timesteps"] = "1";
        locations["query"]["format"] = "json";
      }
      params.append(parameter);
    }

    if (param.isObject())
      params.append(param);

    auto& ok = op["responses"]["200"];
    ok["description"] = "The time series in the requested format";
    for (const auto& f : ts_formats)
    {
      auto& content = ok["content"][f.second];
      if (f.second == "application/json")
      {
        // The shape depends on param and the grouping options
        JsonValue row;
        row["type"] = "object";
        row["additionalProperties"] = true;
        content["schema"]["oneOf"].append(OA::arraySchema(row));
        content["schema"]["oneOf"].append(OA::arraySchema(OA::arraySchema(row)));
        content["schema"]["oneOf"].append(row);
      }
      else
        content["schema"] = OA::stringSchema();
    }
    doc.addCommonComponents();
    OA::Document::addErrorResponses(op, false);

    // The producers and their parameters, times and areas for clients building queries
    auto& producers_op = doc.operation(url + "/api/producers", "get");
    producers_op["tags"].append(TAG_CAPABILITIES);
    producers_op["summary"] = "Producers and their parameters";
    producers_op["description"] =
        "The producers the API reads, with their parameters, areas, times and levels, and a "
        "dictionary of the parameter descriptions and units. Parameter names are lower case.";
    producers_op["operationId"] = "getProducers" + component_name(url);
    producers_op["parameters"].append(
        OA::parameter("source",
                      "query",
                      "Only the producers of this source",
                      OA::enumSchema({"querydata", "grid", "observation"}, max_enum)));
    auto& producers_ok = producers_op["responses"]["200"];
    producers_ok["description"] = "The producers";
    JsonValue producers_schema;
    producers_schema["type"] = "object";
    producers_schema["required"].append("producers");
    producers_schema["required"].append("parameters");
    producers_schema["properties"]["producers"]["type"] = "array";
    producers_schema["properties"]["parameters"]["type"] = "object";
    producers_ok["content"]["application/json"]["schema"] = producers_schema;
    OA::Document::addErrorResponses(producers_op, false);

    // What the requests of one producer may contain: for grid producers the model runs,
    // geometries, levels and ensemble members of the parameter keys
    auto& producer_op = doc.operation(url + "/api/producers/{producer}", "get");
    producer_op["tags"].append(TAG_CAPABILITIES);
    producer_op["summary"] = "One producer and the parameter keys of its grid data";
    producer_op["description"] =
        "The producer as listed by /api/producers, with a dictionary of its parameters. For grid "
        "producers also the model runs, and for the chosen run the geometries and per parameter "
        "the level types and levels, forecast types and ensemble members, aggregation and number "
        "of times, from which parameter keys name:producer:geometry:levelId:level:forecastType:"
        "forecastNumber are built. The level values are in the units of the levels option (hPa "
        "for pressure), the keys in the units of the parameter keys (Pa).";
    producer_op["operationId"] = "getProducer" + component_name(url);
    JsonValue string_schema;
    string_schema["type"] = "string";
    auto producer_param = OA::parameter("producer", "path", "Name of the producer", string_schema);
    producer_param["required"] = true;
    producer_op["parameters"].append(producer_param);
    producer_op["parameters"].append(
        OA::parameter("source",
                      "query",
                      "The source of the producer when several have the same name",
                      OA::enumSchema({"querydata", "grid", "observation"}, max_enum)));
    JsonValue time_schema = string_schema;
    time_schema["format"] = "date-time";
    producer_op["parameters"].append(
        OA::parameter("origintime",
                      "query",
                      "The model run of a grid producer, the latest by default",
                      time_schema));
    auto& producer_ok = producer_op["responses"]["200"];
    producer_ok["description"] = "The producer";
    JsonValue producer_schema;
    producer_schema["type"] = "object";
    producer_schema["required"].append("name");
    producer_schema["required"].append("source");
    producer_schema["required"].append("parameters");
    producer_schema["properties"]["parameters"]["type"] = "object";
    producer_schema["properties"]["runs"]["type"] = "array";
    producer_schema["properties"]["run"]["type"] = "object";
    producer_ok["content"]["application/json"]["schema"] = producer_schema;
    OA::Document::addErrorResponses(producer_op, false);

    // The definition and its documentation
    doc.addApiOperations(TAG_CAPABILITIES, url);

    return doc;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate the TimeSeries API definition");
  }
}

}  // namespace ApiDefinition
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
