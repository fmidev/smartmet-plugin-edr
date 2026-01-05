// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin query processing hub
 */
// ======================================================================

#pragma once

#include "AviEngineQuery.h"
#include "GridEngineQuery.h"
#include "Json.h"
#include "ObsEngineQuery.h"
#include "Plugin.h"
#include "QEngineQuery.h"
#include "ZipWriter.h"
#include <spine/HTTP.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

class QueryProcessingHub
{
 public:
  QueryProcessingHub(const Plugin& thePlugin);

  std::shared_ptr<std::string> processQuery(const State& state,
                                            Spine::Table& table,
                                            Query& masterquery,
                                            const QueryServer::QueryStreamer_sptr& queryStreamer,
                                            std::size_t& product_hash);

  std::size_t hash_value(const State& state,
                         const Spine::HTTP::Request& request,
                         Query masterquery) const;

  const std::string& getZipFileName() const { return zipFileName; }

 private:
  static Json::Value processMetaDataQuery(const State& state, const EDRQuery& edr_query);
  std::shared_ptr<std::string> processMetaDataQuery(const State& state,
                                                    const Query& masterquery,
                                                    Spine::Table& table) const;

  static void setPrecisions(EDRMetaData& emd, const Query& masterquery);
  void processIWXXMAndTACData(const Config& config,
                              const TS::OutputData& outputData,
                              const Query& masterquery,
                              Spine::Table& table);
  static std::string getMsgZipFileName(const std::vector<std::string>& icaoCodes,
                                       const Fmi::LocalDateTime& ldt,
                                       std::vector<std::string>::const_iterator* icaoIterator);
  std::string parseIWXXMAndTACMessages(const TS::TimeSeriesGroupPtr& tsgicao_data,
                                       const TS::TimeSeriesGroupPtr& tsg_data,
                                       const Query& masterquery,
                                       ZipWriter* zipWriter) const;

  QEngineQuery itsQEngineQuery;
  ObsEngineQuery itsObsEngineQuery;
  AviEngineQuery itsAviEngineQuery;
  GridEngineQuery itsGridEngineQuery;

  std::string zipFileName;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
