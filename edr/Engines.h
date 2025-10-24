#pragma once

#include <memory>

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
  std::shared_ptr<Engine::Querydata::Engine> qEngine = nullptr;
  std::shared_ptr<Engine::Geonames::Engine> geoEngine = nullptr;
  std::shared_ptr<Engine::Gis::Engine> gisEngine = nullptr;
  std::shared_ptr<Engine::Grid::Engine> gridEngine = nullptr;
#ifndef WITHOUT_OBSERVATION
  std::shared_ptr<Engine::Observation::Engine> obsEngine = nullptr;
#endif
#ifndef WITHOUT_AVI
  std::shared_ptr<Engine::Avi::Engine> aviEngine = nullptr;
#endif
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
