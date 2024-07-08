/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#include "nori-helper.h"
#include "nori-bearer-stats-connector.h"

#include "nori-bearer-stats-calculator.h"
#include "ns3/nr-mac-rx-trace.h"
#include "ns3/nr-phy-rx-trace.h"

#include <ns3/bandwidth-part-gnb.h>
#include <ns3/bandwidth-part-ue.h>
#include <ns3/beam-manager.h>
#include <ns3/buildings-channel-condition-model.h>
#include <ns3/bwp-manager-algorithm.h>
#include <ns3/bwp-manager-gnb.h>
#include <ns3/bwp-manager-ue.h>
#include <ns3/epc-enb-application.h>
#include <ns3/epc-helper.h>
#include <ns3/epc-ue-nas.h>
#include <ns3/epc-x2.h>
#include <ns3/lte-chunk-processor.h>
#include <ns3/lte-rrc-protocol-ideal.h>
#include <ns3/lte-rrc-protocol-real.h>
#include <ns3/lte-ue-rrc.h>
#include <ns3/multi-model-spectrum-channel.h>
#include <ns3/names.h>
#include <ns3/nr-ch-access-manager.h>
#include <ns3/nr-gnb-mac.h>
#include <ns3/nori-gnb-net-device.h>
#include <ns3/nr-gnb-phy.h>
#include <ns3/nr-mac-scheduler-tdma-rr.h>
#include <ns3/nr-pm-search-full.h>
#include <ns3/nr-rrc-protocol-ideal.h>
#include <ns3/nr-ue-mac.h>
#include <ns3/nr-ue-net-device.h>
#include <ns3/nr-ue-phy.h>
#include <ns3/three-gpp-channel-model.h>
#include <ns3/three-gpp-propagation-loss-model.h>
#include <ns3/three-gpp-spectrum-propagation-loss-model.h>
#include <ns3/three-gpp-v2v-channel-condition-model.h>
#include <ns3/three-gpp-v2v-propagation-loss-model.h>
#include <ns3/uniform-planar-array.h>

#include <algorithm>

namespace ns3
{

/* ... */
NS_LOG_COMPONENT_DEFINE("NoriHelper");

NS_OBJECT_ENSURE_REGISTERED(NoriHelper);

NoriHelper::NoriHelper()
{
    NS_LOG_FUNCTION(this);
}

NoriHelper::~NoriHelper()
{
    NS_LOG_FUNCTION(this);
}

TypeId
NoriHelper::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NoriHelper")
                            .SetParent<Object>()
                            .AddConstructor<NoriHelper>()
                            .AddAttribute("EnableMimoFeedback",
                                          "Generate CQI feedback with RI and PMI for MIMO support",
                                          BooleanValue(false),
                                          MakeBooleanAccessor(&NoriHelper::m_enableMimoFeedback),
                                          MakeBooleanChecker())
                            .AddAttribute("PmSearchMethod",
                                          "Type of the precoding matrix search method.",
                                          TypeIdValue(NrPmSearchFull::GetTypeId()),
                                          MakeTypeIdAccessor(&NoriHelper::SetPmSearchTypeId),
                                          MakeTypeIdChecker())
                            .AddAttribute("HarqEnabled",
                                          "Enable Hybrid ARQ",
                                          BooleanValue(true),
                                          MakeBooleanAccessor(&NoriHelper::m_harqEnabled),
                                          MakeBooleanChecker());
    return tid;
}

void
NoriHelper::EnableE2PdcpTraces (void)
{
  if (m_e2PdcpStats == nullptr && (m_e2mode_nr || m_e2mode_lte))
  {
    m_e2PdcpStats = CreateObject<NoriBearerStatsCalculator> ("E2PDCP");
    m_e2PdcpStats->SetAttribute("DlPdcpOutputFilename", StringValue("DlE2PdcpStats.txt"));
    m_e2PdcpStats->SetAttribute("UlPdcpOutputFilename", StringValue("UlE2PdcpStats.txt"));
    m_e2PdcpStats->SetAttribute("EpochDuration", TimeValue(Seconds(1)));
    m_radioBearerStatsConnector.EnableE2PdcpStats(m_e2PdcpStats);

    m_e2PdcpStatsLte = CreateObject<NoriBearerStatsCalculator> ("E2PDCPLTE");
    m_e2PdcpStatsLte->SetAttribute("DlPdcpOutputFilename", StringValue("DlE2PdcpStatsLte.txt"));
    m_e2PdcpStatsLte->SetAttribute("UlPdcpOutputFilename", StringValue("UlE2PdcpStatsLte.txt"));
    m_e2PdcpStatsLte->SetAttribute("EpochDuration", TimeValue(Seconds(1)));
    m_radioBearerStatsConnector.EnableE2PdcpStats (m_e2PdcpStatsLte);
  }
  else
  {
    NS_LOG_INFO("E2 PDCP stats already created");
  }
}

Ptr<NoriBearerStatsCalculator>
NoriHelper::GetE2PdcpStats (void)
{
  // TODO fix this
  return m_e2PdcpStats;
}

void
NoriHelper::EnableE2RlcTraces (void)
{
  if (m_e2RlcStats == nullptr && (m_e2mode_nr || m_e2mode_lte))
  {
    m_e2RlcStats = CreateObject<NoriBearerStatsCalculator> ("E2RLC");
    m_e2RlcStats->SetAttribute("DlPdcpOutputFilename", StringValue("DlE2RlcStats.txt"));
    m_e2RlcStats->SetAttribute("UlPdcpOutputFilename", StringValue("UlE2RlcStats.txt"));
    m_e2RlcStats->SetAttribute("EpochDuration", TimeValue(Seconds(1)));
    m_radioBearerStatsConnector.EnableE2RlcStats (m_e2RlcStats);

    m_e2RlcStatsLte = CreateObject<NoriBearerStatsCalculator> ("E2RLCLTE");
    m_e2RlcStatsLte->SetAttribute("DlPdcpOutputFilename", StringValue("DlE2RlcStatsLte.txt"));
    m_e2RlcStatsLte->SetAttribute("UlPdcpOutputFilename", StringValue("UlE2RlcStatsLte.txt"));
    m_e2RlcStatsLte->SetAttribute("EpochDuration", TimeValue(Seconds(1)));
    m_radioBearerStatsConnector.EnableE2RlcStats (m_e2RlcStatsLte);
  }
  else
  {
    NS_LOG_INFO("E2 RLC stats already created");
  }
}

Ptr<NoriBearerStatsCalculator>
NoriHelper::GetE2RlcStats (void)
{
  return m_e2RlcStats;
}

void
NoriHelper::SetBasicCellId (uint16_t basicCellId)
{
  m_cellIdCounter = basicCellId;
  m_basicCellId = basicCellId;
}

uint16_t
NoriHelper::GetBasicCellId () const
{
  return m_basicCellId;
}

Ptr<NrGnbPhy>
NoriHelper::CreateGnbPhy(const Ptr<Node>& n,
                       const std::unique_ptr<BandwidthPartInfo>& bwp,
                       const Ptr<NoriGnbNetDevice>& dev,
                       const NrSpectrumPhy::NrPhyRxCtrlEndOkCallback& phyEndCtrlCallback)
{
    NS_LOG_FUNCTION(this);

    Ptr<NrGnbPhy> phy = m_gnbPhyFactory.Create<NrGnbPhy>();

    DoubleValue frequency;
    phy->InstallCentralFrequency(bwp->m_centralFrequency);

    phy->ScheduleStartEventLoop(n->GetId(), 0, 0, 0);

    // PHY <--> CAM
    Ptr<NrChAccessManager> cam =
        DynamicCast<NrChAccessManager>(m_gnbChannelAccessManagerFactory.Create());
    phy->SetCam(cam);
    phy->SetDevice(dev);

    Ptr<MobilityModel> mm = n->GetObject<MobilityModel>();
    NS_ASSERT_MSG(
        mm,
        "MobilityModel needs to be set on node before calling NrHelper::InstallEnbDevice ()");

    Ptr<NrSpectrumPhy> channelPhy = m_gnbSpectrumFactory.Create<NrSpectrumPhy>();
    Ptr<UniformPlanarArray> antenna = m_gnbAntennaFactory.Create<UniformPlanarArray>();
    channelPhy->SetAntenna(antenna);
    cam->SetNrSpectrumPhy(channelPhy);

    channelPhy->InstallHarqPhyModule(
        Create<NrHarqPhy>()); // there should be one HARQ instance per NrSpectrumPhy
    channelPhy->SetIsEnb(true);
    channelPhy->SetDevice(dev); // each NrSpectrumPhy should have a pointer to device
    channelPhy->SetChannel(
        bwp->m_channel); // each NrSpectrumPhy needs to have a pointer to the SpectrumChannel
    // object of the corresponding spectrum part
    channelPhy->InstallPhy(phy); // each NrSpectrumPhy should have a pointer to its NrPhy

    Ptr<LteChunkProcessor> pData =
        Create<LteChunkProcessor>(); // create pData chunk processor per NrSpectrumPhy
    Ptr<LteChunkProcessor> pSrs =
        Create<LteChunkProcessor>(); // create pSrs per processor per NrSpectrumPhy
    if (!m_snrTest)
    {
        // TODO: rename to GeneratePuschCqiReport, replace when enabling uplink MIMO
        pData->AddCallback(MakeCallback(&NrGnbPhy::GenerateDataCqiReport,
                                        phy)); // connect DATA chunk processor that will
        // call GenerateDataCqiReport function
        pData->AddCallback(MakeCallback(&NrSpectrumPhy::UpdateSinrPerceived,
                                        channelPhy)); // connect DATA chunk processor that will
        // call UpdateSinrPerceived function
        pSrs->AddCallback(MakeCallback(&NrSpectrumPhy::UpdateSrsSinrPerceived,
                                       channelPhy)); // connect SRS chunk processor that will
                                                     // call UpdateSrsSinrPerceived function
        if (bwp->m_3gppChannel)
        {
            auto pDataMimo = Create<NrMimoChunkProcessor>();
            pDataMimo->AddCallback(
                MakeCallback(&NrSpectrumPhy::UpdateMimoSinrPerceived, channelPhy));
            channelPhy->AddDataMimoChunkProcessor(pDataMimo);
        }
    }
    channelPhy->AddDataSinrChunkProcessor(pData); // set DATA chunk processor to NrSpectrumPhy
    channelPhy->AddSrsSinrChunkProcessor(pSrs);   // set SRS chunk processor to NrSpectrumPhy
    channelPhy->SetMobility(mm);                  // set mobility model to this NrSpectrumPhy
    channelPhy->SetPhyRxDataEndOkCallback(
        MakeCallback(&NrGnbPhy::PhyDataPacketReceived, phy));  // connect PhyRxDataEndOk callback
    channelPhy->SetPhyRxCtrlEndOkCallback(phyEndCtrlCallback); // connect PhyRxCtrlEndOk
                                                               // callback
    channelPhy->SetPhyUlHarqFeedbackCallback(
        MakeCallback(&NrGnbPhy::ReportUlHarqFeedback, phy)); // PhyUlHarqFeedback callback

    Ptr<BeamManager> beamManager = m_gnbBeamManagerFactory.Create<BeamManager>();
    beamManager->Configure(antenna);
    channelPhy->SetBeamManager(beamManager);
    phy->InstallSpectrumPhy(channelPhy); // finally let know phy that there is this spectrum phy

    return phy;
}


} // namespace ns3
