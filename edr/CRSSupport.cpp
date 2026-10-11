#include "CRSSupport.h"
#include <boost/algorithm/string.hpp>
#include <gis/CoordinateTransformation.h>
#include <gis/SpatialReference.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>
#include <cstring>
#include <set>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace
{
// EPSG code of an identifier such as EPSG:3067, urn:ogc:def:crs:EPSG::3067 or
// http://www.opengis.net/def/crs/EPSG/0/3067
std::optional<int> epsg_code(const std::string& crs)
{
  static const std::vector<std::string> prefixes{"EPSG:",
                                                 "urn:ogc:def:crs:EPSG::",
                                                 "http://www.opengis.net/def/crs/EPSG/0/",
                                                 "https://www.opengis.net/def/crs/EPSG/0/"};
  for (const auto& prefix : prefixes)
  {
    if (!boost::algorithm::istarts_with(crs, prefix))
      continue;
    const auto code = crs.substr(prefix.size());
    if (code.empty() || code.find_first_not_of("0123456789") != std::string::npos)
      return {};
    return Fmi::stoi(code);
  }
  return {};
}

// Removes the space between the geometry type and the dimensions written by OGR
// ("LINESTRING ZM (" -> "LINESTRINGZM("), the coords parsers expect the compact form
std::string compact_wkt(std::string wkt)
{
  for (const char* dims : {" ZM (", " Z (", " M ("})
  {
    auto pos = wkt.find(dims);
    if (pos != std::string::npos)
    {
      std::string replacement(dims + 1);
      replacement.erase(replacement.find(' '), 1);
      wkt.replace(pos, std::strlen(dims), replacement);
    }
  }
  boost::algorithm::replace_all(wkt, " (", "(");
  return wkt;
}

}  // namespace

bool is_crs84(const std::string& crs)
{
  static const std::set<std::string> aliases{
      "",
      "OGC:CRS84",
      "CRS:84",
      "CRS84",
      "[OGC:CRS84]",
      "urn:ogc:def:crs:OGC:1.3:CRS84",
      "urn:ogc:def:crs:OGC::CRS84",
      "http://www.opengis.net/def/crs/OGC/1.3/CRS84",
      "https://www.opengis.net/def/crs/OGC/1.3/CRS84",
      "http://www.opengis.net/def/crs/OGC/0/CRS84",
      "https://www.opengis.net/def/crs/OGC/0/CRS84"};
  return aliases.find(crs) != aliases.end();
}

void CRSRegistry::init(const std::vector<std::string>& crs_list)
{
  try
  {
    for (const auto& name : crs_list)
    {
      auto code = epsg_code(name);
      if (!code)
        throw Fmi::Exception(BCP, "Unsupported crs setting '" + name + "', EPSG:n expected");

      auto info = std::make_shared<CRSInfo>();
      info->id = "EPSG:" + Fmi::to_string(*code);
      info->uri = "http://www.opengis.net/def/crs/EPSG/0/" + Fmi::to_string(*code);

      Fmi::SpatialReference srs(*code);
      info->wkt = srs.WKT();
      info->geographic = srs.isGeographic();
      // The CRS objects use the longitude/latitude (easting/northing) order, OGC requires
      // the axis order of the authority
      // EPSG:4326 coordinates are given in the longitude/latitude order like CRS84, as the
      // OGC API-EDR examples and test suite do
      info->swapped =
          (*code != 4326 &&
           (srs.get()->EPSGTreatsAsLatLong() || srs.get()->EPSGTreatsAsNorthingEasting()));
      itsCRSs.push_back(info);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to initialize the supported CRSs");
  }
}

CRSInfoPtr CRSRegistry::find(const std::string& crs) const
{
  auto code = epsg_code(crs);
  if (!code)
    return {};
  const auto id = "EPSG:" + Fmi::to_string(*code);
  for (const auto& info : itsCRSs)
    if (info->id == id)
      return info;
  return {};
}

namespace
{
// Parses a WKT, sizes of radius and corridor queries may be appended as ":value"
std::unique_ptr<OGRGeometry> parse_wkt(const std::string& wkt, std::string& suffix)
{
  std::string geometry_wkt = wkt;
  auto pos = wkt.rfind(')');
  if (pos != std::string::npos && pos + 1 < wkt.size())
  {
    suffix = wkt.substr(pos + 1);
    geometry_wkt = wkt.substr(0, pos + 1);
  }

  OGRGeometry* geom = nullptr;
  if (OGRGeometryFactory::createFromWkt(geometry_wkt.c_str(), nullptr, &geom) != OGRERR_NONE ||
      geom == nullptr)
    throw Fmi::Exception(BCP, "Failed to parse coordinates '" + wkt + "'");
  return std::unique_ptr<OGRGeometry>(geom);
}
}  // namespace

std::string CRSInfo::toXY(const std::string& wkt) const
{
  try
  {
    if (!swapped)
      return wkt;
    std::string suffix;
    auto geometry = parse_wkt(wkt, suffix);
    geometry->swapXY();
    return compact_wkt(geometry->exportToWkt()) + suffix;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string CRSInfo::toWGS84(const std::string& wkt) const
{
  try
  {
    std::string suffix;
    auto geometry = parse_wkt(wkt, suffix);

    // Keep the shape of long area edges in projected coordinates, paths are not densified
    // since their vertices define the trajectory
    if (!geographic && geometry->getDimension() == 2)
    {
      OGREnvelope envelope;
      geometry->getEnvelope(&envelope);
      const double size = std::max(envelope.MaxX - envelope.MinX, envelope.MaxY - envelope.MinY);
      if (size > 0 && geometry->getDimension() > 0)
        geometry->segmentize(size / 50);
    }

    Fmi::CoordinateTransformation to_wgs84(id, "WGS84");
    if (!to_wgs84.transform(*geometry))
      throw Fmi::Exception(BCP, "Failed to transform coordinates '" + wkt + "' from " + id);

    return compact_wkt(geometry->exportToWkt()) + suffix;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CRSInfo::fromWGS84(std::vector<double>& x, std::vector<double>& y) const
{
  Fmi::CoordinateTransformation from_wgs84("WGS84", id);
  if (!from_wgs84.transform(x, y))
    throw Fmi::Exception(BCP, "Failed to transform coordinates to " + id);
}

void CRSInfo::toWGS84(std::vector<double>& x, std::vector<double>& y) const
{
  Fmi::CoordinateTransformation to_wgs84(id, "WGS84");
  if (!to_wgs84.transform(x, y))
    throw Fmi::Exception(BCP, "Failed to transform coordinates from " + id);
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
