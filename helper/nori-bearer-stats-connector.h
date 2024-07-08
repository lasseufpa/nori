/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
// Copyright (c) 2015, NYU WIRELESS, Tandon School of Engineering, New York University
//
// SPDX-License-Identifier: GPL-2.0-only

#ifndef NORI_BEARER_STATS_CONNECTOR_H
#define NORI_BEARER_STATS_CONNECTOR_H

#include <ns3/config.h>
#include <ns3/ptr.h>
#include <ns3/simple-ref-count.h>
#include <ns3/traced-callback.h>

#include <ns3/nr-bearer-stats-connector.h>
#include <ns3/nori-bearer-stats-calculator.h>

#include <map>
#include <set>

namespace ns3
{

class NrBearerStatsBase;

/**
 * \ingroup utils
 *
 * This class is very useful when user needs to collect
 * statistics from PDCD and RLC. It automatically connects
 * NrBearerStatsCalculator to appropriate trace sinks.
 * Usually user do not use this class. All he/she needs to
 * to do is to call: LteHelper::EnablePdcpTraces() and/or
 * LteHelper::EnableRlcTraces().
 */

class NoriBearerStatsConnector : public NrBearerStatsConnector
{
  public:
    /// Constructor
    NoriBearerStatsConnector();

     /**
   * Enables trace sinks for E2 reporting of the PDCP layer. 
   * Usually, this function
   * is called by MmWaveHelper::EnableE2PdcpTraces().
   * \param e2PdcpStats statistics calculator for PDCP layer
   */
  void EnableE2PdcpStats (Ptr<NoriBearerStatsCalculator> e2PdcpStats);

  /**
   * Enables trace sinks for E2 reporting of the RLC layer. 
   * Usually, this function
   * is called by MmWaveHelper::EnableE2RlcTraces().
   * \param e2PdcpStats statistics calculator for RLC layer
   */
  void EnableE2RlcStats (Ptr<NoriBearerStatsCalculator> e2RlcStats);



  private:
    /**
     * Creates UE Manager path and stores it in m_ueManagerPathByCellIdRnti
     * \param ueManagerPath
     * \param cellId
     * \param rnti
     */
    void StoreUeManagerPath(std::string ueManagerPath, uint16_t cellId, uint16_t rnti);

    /**
     * Connects Srb0 trace sources at UE and eNB to RLC and PDCP calculators,
     * and Srb1 trace sources at eNB to RLC and PDCP calculators,
     * \param ueRrcPath
     * \param imsi
     * \param cellId
     * \param rnti
     */
    void ConnectSrb0Traces(std::string ueRrcPath, uint64_t imsi, uint16_t cellId, uint16_t rnti);

    /**
     * Connects Srb1 trace sources at UE to RLC and PDCP calculators
     * \param ueRrcPath
     * \param imsi
     * \param cellId
     * \param rnti
     */
    void ConnectSrb1TracesUe(std::string ueRrcPath, uint64_t imsi, uint16_t cellId, uint16_t rnti);

    /**
     * Connects all trace sources at UE to RLC and PDCP calculators.
     * This function can connect traces only once for UE.
     * \param context
     * \param imsi
     * \param cellid
     * \param rnti
     */
    void ConnectTracesUeIfFirstTime(std::string context,
                                    uint64_t imsi,
                                    uint16_t cellid,
                                    uint16_t rnti);

    /**
     * Connects all trace sources at eNB to RLC and PDCP calculators.
     * This function can connect traces only once for eNB.
     * \param context
     * \param imsi
     * \param cellid
     * \param rnti
     */
    void ConnectTracesEnbIfFirstTime(std::string context,
                                     uint64_t imsi,
                                     uint16_t cellid,
                                     uint16_t rnti);

    /**
     * Connects all trace sources at UE to RLC and PDCP calculators.
     * \param context
     * \param imsi
     * \param cellid
     * \param rnti
     */
    void ConnectTracesUe(std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti);

    /**
     * Disconnects all trace sources at UE to RLC and PDCP calculators.
     * Function is not implemented.
     * \param context
     * \param imsi
     * \param cellid
     * \param rnti
     */
    void DisconnectTracesUe(std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti);

    /**
     * Connects all trace sources at eNB to RLC and PDCP calculators
     * \param context
     * \param imsi
     * \param cellid
     * \param rnti
     */
    void ConnectTracesEnb(std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti);

    /**
     * Disconnects all trace sources at eNB to RLC and PDCP calculators.
     * Function is not implemented.
     * \param context
     * \param imsi
     * \param cellid
     * \param rnti
     */
    void DisconnectTracesEnb(std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti);

    Ptr<NrBearerStatsBase> m_rlcStats;  //!< Calculator for RLC Statistics
    Ptr<NrBearerStatsBase> m_pdcpStats; //!< Calculator for PDCP Statistics

    bool m_connected; //!< true if traces are connected to sinks, initially set to false
    std::set<uint64_t>
        m_imsiSeenUe; //!< stores all UEs for which RLC and PDCP traces were connected
    std::set<uint64_t>
        m_imsiSeenEnb; //!< stores all eNBs for which RLC and PDCP traces were connected

    /**
     * Struct used as key in m_ueManagerPathByCellIdRnti map
     */
    struct CellIdRnti
    {
        uint16_t cellId; //!< cellId
        uint16_t rnti;   //!< rnti
    };

    /**
     * Less than operator for CellIdRnti, because it is used as key in map
     */
    friend bool operator<(const CellIdRnti& a, const CellIdRnti& b);

    /**
     * List UE Manager Paths by CellIdRnti
     */
    std::map<CellIdRnti, std::string> m_ueManagerPathByCellIdRnti;

    std::vector< Ptr<NoriBearerStatsCalculator> > m_e2PdcpStatsVector; //!< Calculator for PDCP Statistics
    std::vector< Ptr<NoriBearerStatsCalculator> > m_e2RlcStatsVector; //!< Calculator for PDCP Statistics
};

} // namespace ns3

#endif // NR_BEARER_STATS_CONNECTOR_H
