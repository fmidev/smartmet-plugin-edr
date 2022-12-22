#include "EDRQuery.h"
#include <iostream>

namespace SmartMet {
namespace Plugin {
namespace EDR {

std::string to_string(EDRQueryId id) {
  if (id == EDRQueryId::AllCollections)
    return "EDRQueryId::AllCollections";
  if (id == EDRQueryId::SpecifiedCollection)
    return "EDRQueryId::SpecifiedCollection";
  if (id == EDRQueryId::SpecifiedCollectionAllInstances)
    return "EDRQueryId::SpecifiedCollectionAllInstances";
  if (id == EDRQueryId::SpecifiedCollectionSpecifiedInstance)
    return "EDRQueryId::SpecifiedCollectionSpecifiedInstance";
  if (id == EDRQueryId::SpecifiedCollectionLocations)
    return "EDRQueryId::SpecifiedCollectionLocations";
  if (id == EDRQueryId::DataQuery)
    return "EDRQueryId::DataQuery";

  return "";
}

std::string to_string(EDRQueryType query_type) {
  if (query_type == EDRQueryType::Position)
    return "Position";
  if (query_type == EDRQueryType::Radius)
    return "Radius";
  if (query_type == EDRQueryType::Area)
    return "Area";
  if (query_type == EDRQueryType::Cube)
    return "Cube";
  if (query_type == EDRQueryType::Trajectory)
    return "Trajectory";
  if (query_type == EDRQueryType::Corridor)
    return "Corridor";
  if (query_type == EDRQueryType::Items)
    return "Items";
  if (query_type == EDRQueryType::Locations)
    return "Locations";
  if (query_type == EDRQueryType::Instances)
    return "Instances";

  return "InvalidQueryType";
}

EDRQueryType to_query_type_id(const std::string &query_type) {
  if (query_type == "position")
    return EDRQueryType::Position;
  if (query_type == "radius")
    return EDRQueryType::Radius;
  if (query_type == "area")
    return EDRQueryType::Area;
  if (query_type == "cube")
    return EDRQueryType::Cube;
  if (query_type == "trajectory")
    return EDRQueryType::Trajectory;
  if (query_type == "corridor")
    return EDRQueryType::Corridor;
  if (query_type == "items")
    return EDRQueryType::Items;
  if (query_type == "locations")
    return EDRQueryType::Locations;
  if (query_type == "instances")
    return EDRQueryType::Instances;

  return EDRQueryType::InvalidQueryType;
}

std::ostream &operator<<(std::ostream &out, const EDRQuery &edrQ) {
  out << "EDR query:";
  if (!edrQ.data_queries.empty()) {
    out << "\ndata_queries:";

    for (const auto &item : edrQ.data_queries) {
      std::string data_queries;
      for (const auto &item2 : item.second) {
        if (!data_queries.empty())
          data_queries.append(", ");
        data_queries.append(to_string(item2));
      }
      out << "\n " << item.first << ": [" + data_queries + "]";
    }
  }

  out << "\nhost: " << edrQ.host << "\ncollection_id: " << edrQ.collection_id
      << "\ninstance_id: " << edrQ.instance_id
      << "\nquery_type: " << to_string(edrQ.query_type)
      << "\nquery_id: " << to_string(edrQ.query_id) << std::endl;

  return out;
}

} // namespace EDR
} // namespace Plugin
} // namespace SmartMet

// ======================================================================
