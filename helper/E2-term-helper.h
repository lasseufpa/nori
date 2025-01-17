/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <ns3/E2-interface.h>
#include <ns3/lte-enb-net-device.h>
#include <ns3/net-device-container.h>
#include <ns3/net-device.h>
#include <ns3/nr-bearer-stats-calculator.h>
#include <ns3/nr-bearer-stats-connector.h>
#include <ns3/nr-gnb-net-device.h>
#include <ns3/object-factory.h>
#include <ns3/oran-interface.h>

namespace ns3
{
class NoriE2Report;

class E2TermHelper : public Object
{
  public:
    /**
     * @brief E2 term creation, using the eNB/gNB net device. It creates an instance of the E2 term;
     * Also, installs it on the gNB or eNB node, with attributes.
     */

    E2TermHelper();

    /**
     * @brief ~NoriHelper
     */
    ~E2TermHelper() override;

    /**
     * @brief GetTypeId
     * @return the type id of the object
     */
    static TypeId GetTypeId();

    /**
     * @brief Install E2 term on the net device.
     * @param NetDevice the net device of the node where the E2 term will be installed.
     *
     */
    void InstallE2Term(Ptr<NetDevice> NetDevice);

    /**
     * @brief Install E2 term on the
     * @param NetDevice the net device of the node where the E2 term will be installed.
     *
     */
    void InstallE2Term(NetDeviceContainer& NetDevices);

    /**
     * @brief Enable SINR traces
     * @param e2Messages A pointer to the E2 interface
     */
    void EnableSinrTraces(Ptr<E2Interface> e2Messages);

    /**
     * @brief Force E2 Logging for the E2 termination, this methods calls E2Termination::Start
     * @param NetDevice the E2 gNB/eNB net device will have a log.
     *
     */
    void ForceE2Log(Ptr<NetDevice> NetDevice);

    void SetStandardLog(bool standardLog);

  private:
    /***
     * @brief Enable E2 RLC traces and report for LTE and NR cells.
     */
    void EnableE2RlcTraces();

    /***
     * @brief Enable E2 PDCP traces and report for LTE and NR cells.
     */
    void EnableE2PdcpTraces();

    /**
     * @brief Connect PDU reports to the E2 termination
     * @param NetDevice the net device that provides the PDU information.
     * @param e2Messages the E2 interface that will receive the PDU information.
     */
    void ConnectPDUReports(Ptr<NetDevice> NetDevice, Ptr<E2Interface> e2Messages);

    /**
     * @brief Connect PHY traces to the E2
     *
     */
    void ConnectPhyTraces();

    Ptr<NrBearerStatsCalculator> m_e2RlcStats;     // !< E2 RLC stats
    Ptr<NrBearerStatsCalculator> m_e2PdcpStats;    // !< E2 PDCP stats
    Ptr<NrBearerStatsCalculator> m_e2LteRlcStats;  // !< E2 LTE RLC stats
    Ptr<NrBearerStatsCalculator> m_e2LtePdcpStats; // !< E2 LTE PDCP stats

    NrBearerStatsConnector m_e2StatsConnector; // !< E2 PDCP connector

    // E2 termination attributes
    std::string m_e2ip;     // !< E2 termination IP
    uint16_t m_e2port;      // !< E2 termination port
    uint16_t m_e2localPort; // !< E2 termination local port

    // E2 messages attributes
    bool m_e2ForceLog;  // !< E2 force log
    bool m_standardLog; // !< E2 standard message log

    Ptr<E2Termination> m_e2Term;    // !< E2 termination
    Ptr<E2Interface> m_e2Interface; // !< E2 interface
    Ptr<NoriE2Report> m_e2Report;   // !< E2 report
};

} // namespace ns3
