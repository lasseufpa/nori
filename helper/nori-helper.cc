/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#include "nori-helper.h"
#include "nori-bearer-stats-connector.h"
#include "ns3/nori-lte-enb-rrc.h"

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

#include <sys/time.h>

namespace ns3
{

/* ... */
NS_LOG_COMPONENT_DEFINE("NoriHelper");

NS_OBJECT_ENSURE_REGISTERED(NoriHelper);

NoriHelper::NoriHelper() : m_basicCellId (1), m_cellIdCounter (1), m_startTime (0)
{
    NS_LOG_FUNCTION(this);
    m_startTime = GetStartTime();
    m_basicCellId = m_cellIdCounter;
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
                                          MakeBooleanChecker())
                            .AddAttribute ("E2ModeNr",
                                        "If true, enable reporting over E2 for NR cells.",
                                        BooleanValue (false),
                                        MakeBooleanAccessor (&NoriHelper::m_e2mode_nr),
                                        MakeBooleanChecker ())
                            .AddAttribute ("E2ModeLte",
                                        "If true, enable reporting over E2 for LTE cells.",
                                        BooleanValue (true),
                                        MakeBooleanAccessor (&NoriHelper::m_e2mode_lte),
                                        MakeBooleanChecker ())
                            .AddAttribute ("E2TermIp",
                                        "The IP address of the RIC E2 termination",
                                        StringValue ("10.244.0.240"),
                                        MakeStringAccessor (&NoriHelper::m_e2ip),
                                        MakeStringChecker ())
                            .AddAttribute ("E2Port",
                                        "Port number for E2",
                                        UintegerValue (36422),
                                        MakeUintegerAccessor (&NoriHelper::m_e2port),
                                        MakeUintegerChecker<uint16_t> ())
                            .AddAttribute ("E2LocalPort",
                                        "The first port number for the local bind",
                                        UintegerValue (38470),
                                        MakeUintegerAccessor (&NoriHelper::m_e2localPort),
                                        MakeUintegerChecker<uint16_t> ());
    return tid;
}


Ptr<NetDevice>
NoriHelper::InstallSingleGnbDevice(
    const Ptr<Node>& n,
    const std::vector<std::reference_wrapper<BandwidthPartInfoPtr>> allBwps)
{
    NS_ABORT_MSG_IF(m_cellIdCounter == 65535, "max num gNBs exceeded");

    Ptr<NoriGnbNetDevice> dev = m_gnbNetDeviceFactory.Create<NoriGnbNetDevice>();

    NS_LOG_DEBUG("Creating gNB, cellId = " << m_cellIdCounter);
    uint16_t cellId = m_cellIdCounter++;

    dev->SetCellId(cellId);
    dev->SetNode(n);
    dev->SetStartTime(m_startTime);

    // create component carrier map for this gNB device
    std::map<uint8_t, Ptr<BandwidthPartGnb>> ccMap;

    for (uint32_t bwpId = 0; bwpId < allBwps.size(); ++bwpId)
    {
        NS_LOG_DEBUG("Creating BandwidthPart, id = " << bwpId);
        Ptr<BandwidthPartGnb> cc = CreateObject<BandwidthPartGnb>();
        double bwInKhz = allBwps[bwpId].get()->m_channelBandwidth / 1000.0;
        NS_ABORT_MSG_IF(bwInKhz / 100.0 > 65535.0,
                        "A bandwidth of " << bwInKhz / 100.0 << " kHz cannot be represented");

        cc->SetUlBandwidth(static_cast<uint16_t>(bwInKhz / 100));
        cc->SetDlBandwidth(static_cast<uint16_t>(bwInKhz / 100));
        cc->SetDlEarfcn(0); // Argh... handover not working
        cc->SetUlEarfcn(0); // Argh... handover not working
        cc->SetCellId(m_cellIdCounter++);

        auto phy = CreateGnbPhy(
            n,
            allBwps[bwpId].get(),
            dev,
            std::bind(&NrGnbNetDevice::RouteIngoingCtrlMsgs, dev, std::placeholders::_1, bwpId));
        phy->SetBwpId(bwpId);
        cc->SetPhy(phy);

        auto mac = CreateGnbMac();
        cc->SetMac(mac);
        phy->GetCam()->SetNrGnbMac(mac);

        auto sched = CreateGnbSched();
        cc->SetNrMacScheduler(sched);

        if (bwpId == 0)
        {
            cc->SetAsPrimary(true);
        }
        else
        {
            cc->SetAsPrimary(false);
        }

        ccMap.insert(std::make_pair(bwpId, cc));
    }

    Ptr<LteEnbRrc> rrc = CreateObject<LteEnbRrc>();
    Ptr<LteEnbComponentCarrierManager> ccmEnbManager =
        DynamicCast<LteEnbComponentCarrierManager>(CreateObject<BwpManagerGnb>());
    DynamicCast<BwpManagerGnb>(ccmEnbManager)
        ->SetBwpManagerAlgorithm(m_gnbBwpManagerAlgoFactory.Create<BwpManagerAlgorithm>());

    // Convert Enb carrier map to only PhyConf map
    // we want to make RRC to be generic, to be able to work with any type of carriers, not only
    // strictly LTE carriers
    std::map<uint8_t, Ptr<ComponentCarrierBaseStation>> ccPhyConfMap;
    for (const auto& i : ccMap)
    {
        Ptr<ComponentCarrierBaseStation> c = i.second;
        ccPhyConfMap.insert(std::make_pair(i.first, c));
    }

    // ComponentCarrierManager SAP
    rrc->SetLteCcmRrcSapProvider(ccmEnbManager->GetLteCcmRrcSapProvider());
    ccmEnbManager->SetLteCcmRrcSapUser(rrc->GetLteCcmRrcSapUser());
    // Set number of component carriers. Note: eNB CCM would also set the
    // number of component carriers in eNB RRC

    ccmEnbManager->SetNumberOfComponentCarriers(ccMap.size());
    rrc->ConfigureCarriers(ccPhyConfMap);

    // nr module currently uses only RRC ideal mode
    bool useIdealRrc = true;

    if (useIdealRrc)
    {
        Ptr<NrGnbRrcProtocolIdeal> rrcProtocol = CreateObject<NrGnbRrcProtocolIdeal>();
        rrcProtocol->SetLteEnbRrcSapProvider(rrc->GetLteEnbRrcSapProvider());
        rrc->SetLteEnbRrcSapUser(rrcProtocol->GetLteEnbRrcSapUser());
        rrc->AggregateObject(rrcProtocol);
    }
    else
    {
        Ptr<LteEnbRrcProtocolReal> rrcProtocol = CreateObject<LteEnbRrcProtocolReal>();
        rrcProtocol->SetLteEnbRrcSapProvider(rrc->GetLteEnbRrcSapProvider());
        rrc->SetLteEnbRrcSapUser(rrcProtocol->GetLteEnbRrcSapUser());
        rrc->AggregateObject(rrcProtocol);
    }

    if (m_epcHelper != nullptr)
    {
        EnumValue<LteEnbRrc::LteEpsBearerToRlcMapping_t> epsBearerToRlcMapping;
        rrc->GetAttribute("EpsBearerToRlcMapping", epsBearerToRlcMapping);
        // it does not make sense to use RLC/SM when also using the EPC
        if (epsBearerToRlcMapping.Get() == LteEnbRrc::RLC_SM_ALWAYS)
        {
            rrc->SetAttribute("EpsBearerToRlcMapping", EnumValue(LteEnbRrc::RLC_UM_ALWAYS));
        }
    }

    // This RRC attribute is used to connect each new RLC instance with the MAC layer
    // (for function such as TransmitPdu, ReportBufferStatusReport).
    // Since in this new architecture, the component carrier manager acts a proxy, it
    // will have its own LteMacSapProvider interface, RLC will see it as through original MAC
    // interface LteMacSapProvider, but the function call will go now through
    // LteEnbComponentCarrierManager instance that needs to implement functions of this interface,
    // and its task will be to forward these calls to the specific MAC of some of the instances of
    // component carriers. This decision will depend on the specific implementation of the component
    // carrier manager.
    rrc->SetLteMacSapProvider(ccmEnbManager->GetLteMacSapProvider());
    rrc->SetForwardUpCallback(MakeCallback(&NrGnbNetDevice::Receive, dev));

    for (auto& it : ccMap)
    {
        it.second->GetPhy()->SetEnbCphySapUser(rrc->GetLteEnbCphySapUser(it.first));
        rrc->SetLteEnbCphySapProvider(it.second->GetPhy()->GetEnbCphySapProvider(), it.first);

        rrc->SetLteEnbCmacSapProvider(it.second->GetMac()->GetEnbCmacSapProvider(), it.first);
        it.second->GetMac()->SetEnbCmacSapUser(rrc->GetLteEnbCmacSapUser(it.first));

        // PHY <--> MAC SAP
        it.second->GetPhy()->SetPhySapUser(it.second->GetMac()->GetPhySapUser());
        it.second->GetMac()->SetPhySapProvider(it.second->GetPhy()->GetPhySapProvider());
        // PHY <--> MAC SAP END

        // Scheduler SAP
        it.second->GetMac()->SetNrMacSchedSapProvider(
            it.second->GetScheduler()->GetMacSchedSapProvider());
        it.second->GetMac()->SetNrMacCschedSapProvider(
            it.second->GetScheduler()->GetMacCschedSapProvider());

        it.second->GetScheduler()->SetMacSchedSapUser(it.second->GetMac()->GetNrMacSchedSapUser());
        it.second->GetScheduler()->SetMacCschedSapUser(
            it.second->GetMac()->GetNrMacCschedSapUser());
        // Scheduler SAP END

        it.second->GetMac()->SetLteCcmMacSapUser(ccmEnbManager->GetLteCcmMacSapUser());
        ccmEnbManager->SetCcmMacSapProviders(it.first,
                                             it.second->GetMac()->GetLteCcmMacSapProvider());

        // insert the pointer to the LteMacSapProvider interface of the MAC layer of the specific
        // component carrier
        ccmEnbManager->SetMacSapProvider(it.first, it.second->GetMac()->GetMacSapProvider());
    }

    if(m_e2mode_nr) {
    const uint16_t local_port = m_e2localPort + (uint16_t) cellId;
    const std::string gnb_id{std::to_string (cellId)};
    
    std::string plmnId = "111";

    NS_LOG_INFO ("cell_id " << gnb_id);
    Ptr<E2Termination> e2term =
        CreateObject<E2Termination> (m_e2ip, m_e2port, local_port, gnb_id, plmnId);

    dev->SetAttribute("E2Termination", PointerValue(e2term));

    EnableE2PdcpTraces();
    EnableE2RlcTraces();
    dev->SetAttribute("E2PdcpCalculator", PointerValue(m_e2PdcpStats));
    dev->SetAttribute("E2RlcCalculator", PointerValue(m_e2RlcStats));
    dev->SetAttribute("E2DuCalculator", PointerValue(m_phyStats));
    }

    dev->SetAttribute("LteEnbComponentCarrierManager", PointerValue(ccmEnbManager));
    dev->SetCcMap(ccMap);
    dev->SetAttribute("LteEnbRrc", PointerValue(rrc));
    dev->Initialize();

    n->AddDevice(dev);

    if (m_epcHelper != nullptr)
    {
        NS_LOG_INFO("adding this eNB to the EPC");
        m_epcHelper->AddEnb(n, dev, dev->GetCellIds());
        Ptr<EpcEnbApplication> enbApp = n->GetApplication(0)->GetObject<EpcEnbApplication>();
        NS_ASSERT_MSG(enbApp != nullptr, "cannot retrieve EpcEnbApplication");

        // S1 SAPs
        rrc->SetS1SapProvider(enbApp->GetS1SapProvider());
        enbApp->SetS1SapUser(rrc->GetS1SapUser());

        // X2 SAPs
        Ptr<EpcX2> x2 = n->GetObject<EpcX2>();
        x2->SetEpcX2SapUser(rrc->GetEpcX2SapUser());
        rrc->SetEpcX2SapProvider(x2->GetEpcX2SapProvider());
    }

    return dev;
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

int64_t
NoriHelper::DoAssignStreamsToChannelObjects(Ptr<NrSpectrumPhy> phy, int64_t currentStream)
{
    int64_t initialStream = currentStream;

    Ptr<ThreeGppPropagationLossModel> propagationLossModel =
        DynamicCast<ThreeGppPropagationLossModel>(
            phy->GetSpectrumChannel()->GetPropagationLossModel());
    if (!propagationLossModel)
    {
        currentStream +=
            phy->GetSpectrumChannel()->GetPropagationLossModel()->AssignStreams(currentStream);
        return currentStream - initialStream;
    }

    if (std::find(m_channelObjectsWithAssignedStreams.begin(),
                  m_channelObjectsWithAssignedStreams.end(),
                  propagationLossModel) == m_channelObjectsWithAssignedStreams.end())
    {
        currentStream += propagationLossModel->AssignStreams(currentStream);
        m_channelObjectsWithAssignedStreams.emplace_back(propagationLossModel);
    }

    Ptr<ChannelConditionModel> channelConditionModel =
        propagationLossModel->GetChannelConditionModel();

    if (std::find(m_channelObjectsWithAssignedStreams.begin(),
                  m_channelObjectsWithAssignedStreams.end(),
                  channelConditionModel) == m_channelObjectsWithAssignedStreams.end())
    {
        currentStream += channelConditionModel->AssignStreams(currentStream);
        m_channelObjectsWithAssignedStreams.emplace_back(channelConditionModel);
    }

    Ptr<ThreeGppSpectrumPropagationLossModel> spectrumLossModel =
        DynamicCast<ThreeGppSpectrumPropagationLossModel>(
            phy->GetSpectrumChannel()->GetPhasedArraySpectrumPropagationLossModel());

    if (spectrumLossModel)
    {
        if (std::find(m_channelObjectsWithAssignedStreams.begin(),
                      m_channelObjectsWithAssignedStreams.end(),
                      spectrumLossModel) == m_channelObjectsWithAssignedStreams.end())
        {
            Ptr<ThreeGppChannelModel> channel =
                DynamicCast<ThreeGppChannelModel>(spectrumLossModel->GetChannelModel());
            currentStream += channel->AssignStreams(currentStream);
            m_channelObjectsWithAssignedStreams.emplace_back(spectrumLossModel);
        }
    }

    return currentStream - initialStream;
}

uint64_t
NoriHelper::GetStartTime ()
{
  struct timeval time_now{};
  gettimeofday (&time_now, nullptr);
 
  return (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
}

void
NoriHelper::DoDeActivateDedicatedEpsBearer(Ptr<NetDevice> ueDevice,
                                         Ptr<NetDevice> enbDevice,
                                         uint8_t bearerId)
{
    NS_LOG_FUNCTION(this << ueDevice << bearerId);

    // Extract IMSI and rnti
    uint64_t imsi = ueDevice->GetObject<NrUeNetDevice>()->GetImsi();
    uint16_t rnti = ueDevice->GetObject<NrUeNetDevice>()->GetRrc()->GetRnti();

    Ptr<NoriLteEnbRrc> enbRrc = DynamicCast<NoriLteEnbRrc> (enbDevice->GetObject<NoriGnbNetDevice>()->GetRrc());

    enbRrc->DoSendReleaseDataRadioBearer(imsi, rnti, bearerId);
}

Ptr<NrMacScheduler>
NoriHelper::CreateGnbSched()
{
    NS_LOG_FUNCTION(this);

    auto sched = m_schedFactory.Create<NrMacSchedulerNs3>();
    auto dlAmc = m_gnbDlAmcFactory.Create<NrAmc>();
    auto ulAmc = m_gnbUlAmcFactory.Create<NrAmc>();

    sched->InstallDlAmc(dlAmc);
    sched->InstallUlAmc(ulAmc);

    return sched;
}

Ptr<NrGnbMac>
NoriHelper::CreateGnbMac()
{
    NS_LOG_FUNCTION(this);

    Ptr<NrGnbMac> mac = m_gnbMacFactory.Create<NrGnbMac>();
    return mac;
}

Ptr<NrUeMac>
NoriHelper::CreateUeMac() const
{
    NS_LOG_FUNCTION(this);
    Ptr<NrUeMac> mac = m_ueMacFactory.Create<NrUeMac>();
    return mac;
}

Ptr<NrUePhy>
NoriHelper::CreateUePhy(const Ptr<Node>& n,
                      const std::unique_ptr<BandwidthPartInfo>& bwp,
                      const Ptr<NrUeNetDevice>& dev,
                      const NrSpectrumPhy::NrPhyDlHarqFeedbackCallback& dlHarqCallback,
                      const NrSpectrumPhy::NrPhyRxCtrlEndOkCallback& phyRxCtrlCallback)
{
    NS_LOG_FUNCTION(this);

    Ptr<NrUePhy> phy = m_uePhyFactory.Create<NrUePhy>();

    NS_ASSERT(bwp->m_channel != nullptr);

    phy->InstallCentralFrequency(bwp->m_centralFrequency);

    phy->ScheduleStartEventLoop(n->GetId(), 0, 0, 0);

    // connect CAM and PHY
    Ptr<NrChAccessManager> cam =
        DynamicCast<NrChAccessManager>(m_ueChannelAccessManagerFactory.Create());
    phy->SetCam(cam);
    // set device
    phy->SetDevice(dev);

    Ptr<MobilityModel> mm = n->GetObject<MobilityModel>();
    NS_ASSERT_MSG(
        mm,
        "MobilityModel needs to be set on node before calling NrHelper::InstallUeDevice ()");

    Ptr<NrSpectrumPhy> channelPhy =
        m_ueSpectrumFactory.Create<NrSpectrumPhy>(); // Create NrSpectrumPhy

    if (m_harqEnabled)
    {
        Ptr<NrHarqPhy> harq = Create<NrHarqPhy>(); // Create HARQ instance
        channelPhy->InstallHarqPhyModule(harq);
        channelPhy->SetPhyDlHarqFeedbackCallback(dlHarqCallback);
    }
    channelPhy->SetIsEnb(false);
    channelPhy->SetDevice(dev); // each NrSpectrumPhy should have a pointer to device

    Ptr<UniformPlanarArray> antenna =
        m_ueAntennaFactory.Create<UniformPlanarArray>(); // Create antenna per panel
    channelPhy->SetAntenna(antenna);

    cam->SetNrSpectrumPhy(channelPhy); // connect CAM

    Ptr<LteChunkProcessor> pData = Create<LteChunkProcessor>();
    pData->AddCallback(MakeCallback(&NrSpectrumPhy::UpdateSinrPerceived, channelPhy));
    channelPhy->AddDataSinrChunkProcessor(pData);

    Ptr<NrMimoChunkProcessor> pDataMimo{nullptr};
    if (bwp->m_3gppChannel)
    {
        pDataMimo = Create<NrMimoChunkProcessor>();
        pDataMimo->AddCallback(MakeCallback(&NrSpectrumPhy::UpdateMimoSinrPerceived, channelPhy));
        channelPhy->AddDataMimoChunkProcessor(pDataMimo);
    }
    if (bwp->m_3gppChannel && m_enableMimoFeedback)
    {
        // Report DL CQI, PMI, RI (channel quality, MIMO precoding matrix and rank indicators)
        pDataMimo->AddCallback(MakeCallback(&NrUePhy::GenerateDlCqiReportMimo, phy));
    }
    else
    {
        // SISO CQI feedback
        pData->AddCallback(MakeCallback(&NrUePhy::GenerateDlCqiReport, phy));
    }

    Ptr<LteChunkProcessor> pRs = Create<LteChunkProcessor>();
    pRs->AddCallback(MakeCallback(&NrUePhy::ReportRsReceivedPower, phy));
    channelPhy->AddRsPowerChunkProcessor(pRs);

    Ptr<LteChunkProcessor> pSinr = Create<LteChunkProcessor>();
    pSinr->AddCallback(MakeCallback(&NrSpectrumPhy::ReportDlCtrlSinr, channelPhy));
    channelPhy->AddDlCtrlSinrChunkProcessor(pSinr);

    channelPhy->SetChannel(bwp->m_channel);
    channelPhy->InstallPhy(phy);
    channelPhy->SetMobility(mm);
    channelPhy->SetPhyRxDataEndOkCallback(MakeCallback(&NrUePhy::PhyDataPacketReceived, phy));
    channelPhy->SetPhyRxCtrlEndOkCallback(phyRxCtrlCallback);

    Ptr<BeamManager> beamManager = m_ueBeamManagerFactory.Create<BeamManager>();
    beamManager->Configure(antenna);
    channelPhy->SetBeamManager(beamManager);
    phy->InstallSpectrumPhy(channelPhy);
    return phy;
}

Ptr<NetDevice>
NoriHelper::InstallSingleUeDevice(
    const Ptr<Node>& n,
    const std::vector<std::reference_wrapper<BandwidthPartInfoPtr>> allBwps)
{
    NS_LOG_FUNCTION(this);

    Ptr<NrUeNetDevice> dev = m_ueNetDeviceFactory.Create<NrUeNetDevice>();
    dev->SetNode(n);

    std::map<uint8_t, Ptr<BandwidthPartUe>> ueCcMap;

    // Create, for each ue, its bandwidth parts
    for (uint32_t bwpId = 0; bwpId < allBwps.size(); ++bwpId)
    {
        Ptr<BandwidthPartUe> cc = CreateObject<BandwidthPartUe>();
        double bwInKhz = allBwps[bwpId].get()->m_channelBandwidth / 1000.0;
        NS_ABORT_MSG_IF(bwInKhz / 100.0 > 65535.0,
                        "A bandwidth of " << bwInKhz / 100.0 << " kHz cannot be represented");
        cc->SetUlBandwidth(static_cast<uint16_t>(bwInKhz / 100));
        cc->SetDlBandwidth(static_cast<uint16_t>(bwInKhz / 100));
        cc->SetDlEarfcn(0); // Used for nothing..
        cc->SetUlEarfcn(0); // Used for nothing..

        auto mac = CreateUeMac();
        cc->SetMac(mac);

        auto phy = CreateUePhy(
            n,
            allBwps[bwpId].get(),
            dev,
            MakeCallback(&NrUeNetDevice::EnqueueDlHarqFeedback, dev),
            std::bind(&NrUeNetDevice::RouteIngoingCtrlMsgs, dev, std::placeholders::_1, bwpId));

        phy->SetBwpId(bwpId);
        cc->SetPhy(phy);

        if (bwpId == 0)
        {
            cc->SetAsPrimary(true);
        }
        else
        {
            cc->SetAsPrimary(false);
        }

        ueCcMap.insert(std::make_pair(bwpId, cc));
    }

    Ptr<LteUeComponentCarrierManager> ccmUe =
        DynamicCast<LteUeComponentCarrierManager>(CreateObject<BwpManagerUe>());
    DynamicCast<BwpManagerUe>(ccmUe)->SetBwpManagerAlgorithm(
        m_ueBwpManagerAlgoFactory.Create<BwpManagerAlgorithm>());

    Ptr<LteUeRrc> rrc = CreateObject<LteUeRrc>();
    rrc->m_numberOfComponentCarriers = ueCcMap.size();
    // run InitializeSap to create the proper number of sap provider/users
    rrc->InitializeSap();
    rrc->SetLteMacSapProvider(ccmUe->GetLteMacSapProvider());
    // setting ComponentCarrierManager SAP
    rrc->SetLteCcmRrcSapProvider(ccmUe->GetLteCcmRrcSapProvider());
    ccmUe->SetLteCcmRrcSapUser(rrc->GetLteCcmRrcSapUser());
    ccmUe->SetNumberOfComponentCarriers(ueCcMap.size());

    bool useIdealRrc = true;
    if (useIdealRrc)
    {
        Ptr<nrUeRrcProtocolIdeal> rrcProtocol = CreateObject<nrUeRrcProtocolIdeal>();
        rrcProtocol->SetUeRrc(rrc);
        rrc->AggregateObject(rrcProtocol);
        rrcProtocol->SetLteUeRrcSapProvider(rrc->GetLteUeRrcSapProvider());
        rrc->SetLteUeRrcSapUser(rrcProtocol->GetLteUeRrcSapUser());
    }
    else
    {
        Ptr<LteUeRrcProtocolReal> rrcProtocol = CreateObject<LteUeRrcProtocolReal>();
        rrcProtocol->SetUeRrc(rrc);
        rrc->AggregateObject(rrcProtocol);
        rrcProtocol->SetLteUeRrcSapProvider(rrc->GetLteUeRrcSapProvider());
        rrc->SetLteUeRrcSapUser(rrcProtocol->GetLteUeRrcSapUser());
    }

    if (m_epcHelper != nullptr)
    {
        rrc->SetUseRlcSm(false);
    }
    else
    {
        rrc->SetUseRlcSm(true);
    }
    Ptr<EpcUeNas> nas = CreateObject<EpcUeNas>();

    nas->SetAsSapProvider(rrc->GetAsSapProvider());
    nas->SetDevice(dev);
    nas->SetForwardUpCallback(MakeCallback(&NrUeNetDevice::Receive, dev));

    rrc->SetAsSapUser(nas->GetAsSapUser());

    for (auto& it : ueCcMap)
    {
        rrc->SetLteUeCmacSapProvider(it.second->GetMac()->GetUeCmacSapProvider(), it.first);
        it.second->GetMac()->SetUeCmacSapUser(rrc->GetLteUeCmacSapUser(it.first));

        it.second->GetPhy()->SetUeCphySapUser(rrc->GetLteUeCphySapUser());
        rrc->SetLteUeCphySapProvider(it.second->GetPhy()->GetUeCphySapProvider(), it.first);

        it.second->GetPhy()->SetPhySapUser(it.second->GetMac()->GetPhySapUser());
        it.second->GetMac()->SetPhySapProvider(it.second->GetPhy()->GetPhySapProvider());

        bool ccmTest =
            ccmUe->SetComponentCarrierMacSapProviders(it.first,
                                                      it.second->GetMac()->GetUeMacSapProvider());

        if (!ccmTest)
        {
            NS_FATAL_ERROR("Error in SetComponentCarrierMacSapProviders");
        }
    }

    NS_ABORT_MSG_IF(m_imsiCounter >= 0xFFFFFFFF, "max num UEs exceeded");
    uint64_t imsi = ++m_imsiCounter;

    dev->SetAttribute("Imsi", UintegerValue(imsi));
    dev->SetCcMap(ueCcMap);
    dev->SetAttribute("nrUeRrc", PointerValue(rrc));
    dev->SetAttribute("EpcUeNas", PointerValue(nas));
    dev->SetAttribute("LteUeComponentCarrierManager", PointerValue(ccmUe));

    n->AddDevice(dev);

    if (m_epcHelper != nullptr)
    {
        m_epcHelper->AddUe(dev, dev->GetImsi());
    }

    dev->Initialize();

    return dev;
}

void
NoriHelper::AttachToClosestEnb(NetDeviceContainer ueDevices, NetDeviceContainer enbDevices)
{
    NS_LOG_FUNCTION(this);

    for (NetDeviceContainer::Iterator i = ueDevices.Begin(); i != ueDevices.End(); i++)
    {
        AttachToClosestEnb(*i, enbDevices);
    }
}

void
NoriHelper::AttachToClosestEnb(Ptr<NetDevice> ueDevice, NetDeviceContainer enbDevices)
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT_MSG(enbDevices.GetN() > 0, "empty enb device container");
    Vector uepos = ueDevice->GetNode()->GetObject<MobilityModel>()->GetPosition();
    double minDistance = std::numeric_limits<double>::infinity();
    Ptr<NetDevice> closestEnbDevice;
    for (NetDeviceContainer::Iterator i = enbDevices.Begin(); i != enbDevices.End(); ++i)
    {
        Vector enbpos = (*i)->GetNode()->GetObject<MobilityModel>()->GetPosition();
        double distance = CalculateDistance(uepos, enbpos);
        if (distance < minDistance)
        {
            minDistance = distance;
            closestEnbDevice = *i;
        }
    }
    NS_ASSERT(closestEnbDevice);

    AttachToEnb(ueDevice, closestEnbDevice);
}

} // namespace ns3
