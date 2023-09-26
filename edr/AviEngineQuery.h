// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin AVI engine query processing
 */
// ======================================================================

#pragma once

#include "Plugin.h"
#include <engines/avi/Engine.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

class AviEngineQuery
{
 public:
  explicit AviEngineQuery(const Plugin &thePlugin);

#ifndef WITHOUT_AVI
  void processAviEngineQuery(const State &state,
                             const Query &query,
                             const std::string &producer,
                             TS::OutputData &outputData) const;

  bool isAviProducer(const std::string &producer) const;

#endif

  const Plugin &itsPlugin;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
