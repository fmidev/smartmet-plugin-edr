#pragma once

namespace SmartMet
{
// Engines
namespace Engine
{
namespace Querydata
{
class Engine;
}
namespace Grid
{
class Engine;
}
#ifndef WITHOUT_OBSERVATION
namespace Observation
{
class Engine;
}
#endif
#ifndef WITHOUT_AVI
namespace Avi
{
class Engine;
}
#endif
namespace Geonames
{
class Engine;
}
namespace Gis
{
class Engine;
}
}  // namespace Engine

namespace Plugin
{
namespace EDR
{

struct Engines
{
  Engine::Querydata::Engine* qEngine = nullptr;
  Engine::Geonames::Engine* geoEngine = nullptr;
  Engine::Gis::Engine* gisEngine = nullptr;
  Engine::Grid::Engine* gridEngine = nullptr;
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine* obsEngine = nullptr;
#endif
#ifndef WITHOUT_AVI
  Engine::Avi::Engine* aviEngine = nullptr;
#endif
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
