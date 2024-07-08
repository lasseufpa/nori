/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
// Copyright (c) 2015, NYU WIRELESS, Tandon School of Engineering, New York University
//
// SPDX-License-Identifier: GPL-2.0-only

#include "nori-bearer-stats-connector.h"

#include "nori-bearer-stats-calculator.h"

#include <ns3/log.h>
#include <ns3/lte-enb-net-device.h>
#include <ns3/lte-enb-rrc.h>
#include <ns3/lte-ue-net-device.h>
#include <ns3/lte-ue-rrc.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NoriBearerStatsConnector");

NoriBearerStatsConnector::NoriBearerStatsConnector()
{
  NS_LOG_FUNCTION(this);
}


void
NoriBearerStatsConnector::EnableE2PdcpStats (Ptr<NoriBearerStatsCalculator> e2PdcpStats)
{
  if(m_e2PdcpStatsVector.empty()) {
    Simulator::Schedule(Seconds(0), &NoriBearerStatsConnector::EnsureConnected, this);
  }

  m_e2PdcpStatsVector.push_back(e2PdcpStats);
}

void
NoriBearerStatsConnector::EnableE2RlcStats (Ptr<NoriBearerStatsCalculator> e2RlcStats)
{
  if(m_e2RlcStatsVector.empty()) {
    Simulator::Schedule(Seconds(0), &NoriBearerStatsConnector::EnsureConnected, this);
  }

  m_e2RlcStatsVector.push_back(e2RlcStats);
}


} // namespace ns3
