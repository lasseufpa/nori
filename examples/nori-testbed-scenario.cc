/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2026 LASSE/UFPA - Universidade Federal do Pará
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * NORI Testbed Equivalent Scenario (Hardcoded in C++):
 *
 * Replicates the physical/emulated testbed setup (OCUDU gNB + RU + UEs):
 * - 1 gNodeB (mono-cell indoor, Band n78 at 3350.01 MHz / ARFCN 623334)
 * - Bandwidth: 100 MHz, Numerology 1 (SCS 30 kHz) -> 273 Physical PRBs
 * - TDD Slot Pattern: "DL|DL|DL|DL|DL|DL|DL|S|UL|UL|" (10 slots: 7 DL, 1 Special, 2 UL)
 * - Radio & Antenna: RU TxPower = 20.0 dBm, UE TxPower = 23.0 dBm, 4T4R (2x2 antenna array)
 * - PDSCH MCS Table: 64-QAM (ns3::NrEesmIrT1, 3GPP Table 1)
 * - 3 UEs at bench distances (~25 m), each dedicated to a slice:
 *     1. UE 0 (RNTI 1) -> SST 1: Video Streaming (~30 Mbps UDP continuous, 1400 B)
 *     2. UE 1 (RNTI 2) -> SST 2: Social Network (~10 Mbps bursty, on=0.5s, off=0.5s, 1200 B)
 *     3. UE 2 (RNTI 3) -> SST 3: Cloud Gaming (~18 Mbps interactive stable, 1100 B)
 * - RL MAC Scheduler (NrRLMacSchedulerOfdma) with dynamic PRB quota slicing support
 * - E2 interface support (KPM v3.00, RC v3.01, CCC) with 1.0s telemetry periodicity
 * - Ex: ./ns3 run nori-testbed-scenario -- --simTime=0 --enableE2=True --ipE2TermRic= 
 */

#include "ns3/E2-term-helper.h"
#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nori-module.h"
#include "ns3/nori-slicing-helper.h"
#include "ns3/nr-amc.h"
#include "ns3/nr-helper.h"
#include "ns3/nr-module.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/nr-rl-mac-scheduler-ofdma.h"
#include "ns3/point-to-point-helper.h"

#include <csignal>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NoriTestbedScenario");

namespace
{
constexpr uint16_t kDefaultGnbNum = 1;
constexpr uint16_t kDefaultUeNum = 3;
constexpr double kCentralFrequencyHz = 3.35001e9; // 3350.01 MHz (Band n78, dl_arfcn = 623334)
constexpr double kBandwidthHz = 100e6;            // 100 MHz
constexpr uint16_t kNumerology = 1;               // SCS 30 kHz (subcarrier spacing = 1)
constexpr double kRbOverhead = 0.017;             // 3GPP overhead for 273 PRBs in 100 MHz @ 30 kHz
constexpr uint32_t kExpectedPrbs = 273;           // 273 physical PRBs (100 MHz, mu=1)
constexpr double kGnbTxPowerDbm = 20.0;           // RU TX power = 20 dBm
constexpr double kUeTxPowerDbm = 23.0;            // UE TX power = 23 dBm
constexpr double kE2PeriodicitySec = 1.0;         // du_report_period = 1000 ms (1.0s)

// 4T4R antennas (nof_antennas_dl: 4, nof_antennas_ul: 4 -> 2x2 planar array)
constexpr uint16_t kGnbAntRows = 2;
constexpr uint16_t kGnbAntCols = 2;
constexpr uint16_t kUeAntRows = 2;
constexpr uint16_t kUeAntCols = 2;

// TDD Slot Pattern (dl_ul_tx_period: 10, nof_dl_slots: 7, 1 Special, nof_ul_slots: 2)
const std::string kTddPattern = "DL|DL|DL|DL|DL|DL|DL|S|UL|UL|";

// MCS Table (mcs_table: qam64 -> 3GPP Table 1 via NrEesmIrT1)
const std::string kErrorModelType = "ns3::NrEesmIrT1";

// Simulation control timing
constexpr double kRrcSettlingTimeSec = 0.1;
constexpr double kSinkStartTimeSec = 0.4;
constexpr double kTrafficStartTimeSec = 1.0;

// Indoor bench geometry
constexpr double kGnbHeightMeters = 10.0;
constexpr double kUeDistanceMeters = 25.0;
constexpr double kUeHeightMeters = 1.5;

void
SignalHandler(int signum)
{
    std::cout << "\n[SINAL " << signum
              << "] Interrupção recebida. Encerrando simulação//..." << std::endl;
    Simulator::Stop(Seconds(0.0));
}
} // namespace

/**
 * \brief Reentrant state context for per-slice flow metrics tracking.
 */
struct SliceFlowMetricsContext : public SimpleRefCount<SliceFlowMetricsContext>
{
    std::vector<uint64_t> lastRxBytes;
    std::vector<uint64_t> lastTxPackets;
    std::vector<uint64_t> lastRxPackets;
    double lastTime{0.0};

    explicit SliceFlowMetricsContext(size_t ueCount)
        : lastRxBytes(ueCount, 0),
          lastTxPackets(ueCount, 0),
          lastRxPackets(ueCount, 0)
    {
    }
};

/**
 * \brief Periodically prints throughput, delay, and packet loss statistics per network slice.
 */
void
PrintPeriodicFlowStats(Ptr<FlowMonitor> monitor,
                       FlowMonitorHelper* flowmonHelper,
                       Ptr<SliceFlowMetricsContext> ctx,
                       const std::map<Ipv4Address, uint32_t>& ueIpToIndex,
                       Ipv4Address ueNetworkAddress,
                       Ipv4Mask ueNetworkMask,
                       double simTime,
                       double interval)
{
    double now = Simulator::Now().GetSeconds();
    if (simTime > 0.0 && now > simTime)
    {
        return;
    }

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper->GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> statsMap = monitor->GetFlowStats();

    uint32_t ueCount = ueIpToIndex.size();
    std::vector<uint64_t> ueTxPackets(ueCount, 0);
    std::vector<uint64_t> ueRxPackets(ueCount, 0);
    std::vector<uint64_t> ueRxBytes(ueCount, 0);
    std::vector<double> ueDelaySum(ueCount, 0.0);

    for (const auto& it : statsMap)
    {
        FlowId flowId = it.first;
        const FlowMonitor::FlowStats& stats = it.second;
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);

        if (!ueNetworkMask.IsMatch(t.destinationAddress, ueNetworkAddress))
        {
            continue;
        }

        auto itIdx = ueIpToIndex.find(t.destinationAddress);
        if (itIdx == ueIpToIndex.end() || itIdx->second >= ueCount)
        {
            continue;
        }

        uint32_t idx = itIdx->second;
        ueTxPackets[idx] += stats.txPackets;
        ueRxPackets[idx] += stats.rxPackets;
        ueRxBytes[idx] += stats.rxBytes;
        ueDelaySum[idx] += stats.delaySum.GetSeconds();
    }

    double deltaT = (ctx->lastTime > 0.0) ? (now - ctx->lastTime) : interval;
    if (deltaT <= 0.0)
    {
        deltaT = interval;
    }

    std::cout << "\n========================================================" << std::endl;
    std::cout << "[t = " << std::fixed << std::setprecision(1) << now
              << "s] Testbed Slicing - Per-Slice Performance Monitoring:" << std::endl;
    std::cout << "========================================================" << std::endl;

    const std::vector<std::string> sliceNames = {
        "Slice 1 (Video Streaming, SST=1)",
        "Slice 2 (Social Network,  SST=2)",
        "Slice 3 (Cloud Gaming,    SST=3)"
    };

    for (uint32_t i = 0; i < ueCount; ++i)
    {
        double intervalThroughput = 0.0;
        double avgThroughput = 0.0;
        double delayMs = 0.0;
        double lossRatio = 0.0;
        std::string sliceName =
            (i < sliceNames.size()) ? sliceNames[i] : ("Slice " + std::to_string(i));

        uint64_t diffRxBytes =
            (ueRxBytes[i] >= ctx->lastRxBytes[i]) ? (ueRxBytes[i] - ctx->lastRxBytes[i]) : ueRxBytes[i];
        intervalThroughput = (diffRxBytes * 8.0) / deltaT / 1e6; // Interval throughput in Mbps

        if (ueRxPackets[i] > 0)
        {
            avgThroughput = (ueRxBytes[i] * 8.0) / now / 1e6;
            delayMs = (ueDelaySum[i] / ueRxPackets[i]) * 1e3;
        }

        uint64_t diffTx = (ueTxPackets[i] >= ctx->lastTxPackets[i])
                              ? (ueTxPackets[i] - ctx->lastTxPackets[i])
                              : ueTxPackets[i];
        uint64_t diffRx = (ueRxPackets[i] >= ctx->lastRxPackets[i])
                              ? (ueRxPackets[i] - ctx->lastRxPackets[i])
                              : ueRxPackets[i];
        if (diffTx > 0 && diffTx > diffRx)
        {
            lossRatio = (double)(diffTx - diffRx) * 100.0 / diffTx;
        }

        std::cout << "  " << sliceName << " -> UE[" << i << "]: "
                  << "Throughput = " << std::fixed << std::setprecision(2) << intervalThroughput
                  << " Mbps (Avg: " << avgThroughput << " Mbps), "
                  << "Delay = " << delayMs << " ms, "
                  << "Loss = " << lossRatio << " %" << std::endl;
    }
    std::cout << "========================================================\n" << std::endl;

    ctx->lastRxBytes = ueRxBytes;
    ctx->lastTxPackets = ueTxPackets;
    ctx->lastRxPackets = ueRxPackets;
    ctx->lastTime = now;

    if (simTime <= 0.0 || now + interval <= simTime)
    {
        Simulator::Schedule(Seconds(interval),
                            &PrintPeriodicFlowStats,
                            monitor,
                            flowmonHelper,
                            ctx,
                            ueIpToIndex,
                            ueNetworkAddress,
                            ueNetworkMask,
                            simTime,
                            interval);
    }
}

/**
 * \brief Validates spectrum configuration, TDD pattern, and physical PRBs on the gNB after initialization.
 */
void
ValidateSpectrumConfiguration(Ptr<NetDevice> gnbDev,
                              double centralFrequency,
                              double bandwidth,
                              uint16_t numerology)
{
    Ptr<NrGnbNetDevice> nrGnb = DynamicCast<NrGnbNetDevice>(gnbDev);
    if (!nrGnb)
    {
        return;
    }
    Ptr<NrGnbPhy> phy = nrGnb->GetPhy(0);
    uint32_t numPrb = phy ? phy->GetRbNum() : 0;
    std::string pattern = phy ? phy->GetPattern() : "";
    double txPower = phy ? phy->GetTxPower() : 0.0;

    std::cout << "\n========================================================" << std::endl;
    std::cout << "[t = " << std::fixed << std::setprecision(1) << Simulator::Now().GetSeconds()
              << "s] Validação de Espectro e Rádio na gNodeB (OCUDU + RU):" << std::endl;
    std::cout << "  - Frequência Central: " << std::setprecision(5) << (centralFrequency / 1e9)
              << " GHz (Banda n78, ARFCN 623334)" << std::endl;
    std::cout << "  - Largura de Banda:   " << std::setprecision(1) << (bandwidth / 1e6) << " MHz"
              << std::endl;
    std::cout << "  - Numerologia (SCS):  " << numerology << " (30 kHz)" << std::endl;
    std::cout << "  - Padrão TDD:         " << pattern << std::endl;
    std::cout << "  - Potência TX (RU):   " << txPower << " dBm" << std::endl;
    std::cout << "  - Antenas (MIMO):     " << (kGnbAntRows * kGnbAntCols) << "T"
              << (kGnbAntRows * kGnbAntCols) << "R (gNB " << kGnbAntRows << "x" << kGnbAntCols
              << ", UE " << kUeAntRows << "x" << kUeAntCols << ")" << std::endl;
    std::cout << "  - Tabela MCS PDSCH:   64-QAM (" << kErrorModelType << ")" << std::endl;
    std::cout << "  - PRBs Físicos Ativos: " << numPrb << " PRBs (Esperado: " << kExpectedPrbs
              << " PRBs)" << std::endl;
    if (numPrb == kExpectedPrbs)
    {
        std::cout << "  - Status: SUCESSO! " << kExpectedPrbs
                  << " PRBs físicos confirmados na gNodeB!" << std::endl;
    }
    else
    {
        std::cout << "  - Status: ATENÇÃO! PRBs obtidos = " << numPrb << " (esperado "
                  << kExpectedPrbs << ")" << std::endl;
    }
    std::cout << "========================================================\n" << std::endl;
}

int
main(int argc, char* argv[])
{
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    // Enable relevant log components
    LogComponentEnable("NoriTestbedScenario", LOG_LEVEL_INFO);
    LogComponentEnable("E2Interface", LOG_LEVEL_INFO);
    LogComponentEnable("E2Termination", LOG_LEVEL_INFO);
    LogComponentEnable("NoriSlicingHelper", LOG_LEVEL_INFO);

    // =========================================================================
    // Real Testbed Parameters (OCUDU values.yaml + RU running-config)
    // =========================================================================
    uint16_t gNbNum = kDefaultGnbNum;
    uint16_t ueNum = kDefaultUeNum;
    double centralFrequency = kCentralFrequencyHz; // 3.35001 GHz (Band n78)
    double bandwidth = kBandwidthHz;               // 100 MHz (273 PRBs)
    uint16_t numerology = kNumerology;             // 1 (SCS 30 kHz)
    double txPower = kGnbTxPowerDbm;               // 20.0 dBm (RU TX power)
    double ueTxPower = kUeTxPowerDbm;             // 23.0 dBm
    double e2Periodicity = kE2PeriodicitySec;      // 1.0s (1000 ms)
    double statsInterval = 1.0;                    // FlowMonitor reporting interval

    // =========================================================================
    // Command Line Flags (Simulation and Near-RT RIC Connection)
    // =========================================================================
    double simTime = 10.0;                     // Duration in seconds (0 = continuous)
    bool enableRealtime = false;               // Synchronize with wall-clock time
    bool enableE2 = false;                     // Enable E2 interface
    std::string ipE2TermRic = "10.233.21.173"; // E2Term IP (values.yaml: 10.233.21.173 or NodePort 10.10.20.144)
    uint16_t ipE2TermRicPort = 36421;          // E2Term SCTP port (values.yaml: 36421 or NodePort 32222)

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Duração da simulação em segundos (0 para contínuo)", simTime);
    cmd.AddValue("enableRealtime", "Sincronizar com relógio de parede", enableRealtime);
    cmd.AddValue("enableE2", "Habilitar interface E2", enableE2);
    cmd.AddValue("ipE2TermRic", "IP do Near-RT RIC E2Term", ipE2TermRic);
    cmd.AddValue("ipE2TermRicPort", "Porta SCTP do RIC E2Term", ipE2TermRicPort);
    cmd.AddValue("statsInterval", "Intervalo das estatísticas locais em segundos", statsInterval);
    cmd.Parse(argc, argv);

    if (enableRealtime)
    {
        GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    }

    NS_LOG_INFO("Configurando Cenário Testbed Equivalente no NORI (Especificações Reais OCUDU/RU):");
    NS_LOG_INFO("  - gNodeB: 1 (Banda n78 @ " << (centralFrequency / 1e9)
                << " GHz, BW = " << (bandwidth / 1e6) << " MHz, SCS = 30 kHz, TxPower = "
                << txPower << " dBm, TDD = " << kTddPattern << ")");
    NS_LOG_INFO("  - Antenas: 4T4R (gNB " << kGnbAntRows << "x" << kGnbAntCols << ", UE "
                << kUeAntRows << "x" << kUeAntCols << "), MCS Table = 64-QAM (" << kErrorModelType
                << ")");
    NS_LOG_INFO("  - UEs: " << ueNum << " (Distância ~" << kUeDistanceMeters
                << "m, TxPower = " << ueTxPower << " dBm)");
    NS_LOG_INFO("  - Interface E2: " << (enableE2 ? "Habilitada" : "Desabilitada")
                << " (RIC: " << ipE2TermRic << ":" << ipE2TermRicPort
                << ", Periodicidade: " << e2Periodicity << "s)");

    // =========================================================================
    // Node Creation and Mobility Topology (Indoor Bench ~25m)
    // =========================================================================
    NodeContainer gNbNodes;
    gNbNodes.Create(gNbNum);

    NodeContainer ueNodes;
    ueNodes.Create(ueNum);

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // gNB positioned at the origin
    Ptr<ListPositionAllocator> gNbPositionAlloc = CreateObject<ListPositionAllocator>();
    gNbPositionAlloc->Add(Vector(0.0, 0.0, kGnbHeightMeters));
    mobility.SetPositionAllocator(gNbPositionAlloc);
    mobility.Install(gNbNodes);

    // 3 UEs placed at bench distances (~25 m)
    Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
    uePositionAlloc->Add(Vector(kUeDistanceMeters, 0.0, kUeHeightMeters));  // UE 0: Video Streaming
    uePositionAlloc->Add(Vector(0.0, kUeDistanceMeters, kUeHeightMeters));  // UE 1: Social Network
    uePositionAlloc->Add(Vector(-kUeDistanceMeters, 0.0, kUeHeightMeters)); // UE 2: Cloud Gaming
    mobility.SetPositionAllocator(uePositionAlloc);
    mobility.Install(ueNodes);

    // =========================================================================
    // NR, EPC, Channel, 64-QAM MCS, and 4T4R Antenna Helpers
    // =========================================================================
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();

    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(epcHelper);
    channelHelper->ConfigureFactories("UMi", "LOS");

    // MAC Scheduler with Network Slicing support (NrRLMacSchedulerOfdma)
    nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrRLMacSchedulerOfdma"));

    // PDSCH/PUSCH MCS table capped at 64-QAM (mcs_table: qam64 -> 3GPP Table 1)
    nrHelper->SetDlErrorModel(kErrorModelType);
    nrHelper->SetUlErrorModel(kErrorModelType);
    nrHelper->SetGnbDlAmcAttribute("AmcModel", EnumValue(NrAmc::ErrorModel));
    nrHelper->SetGnbUlAmcAttribute("AmcModel", EnumValue(NrAmc::ErrorModel));

    // PHY configuration (Numerology, TxPower, TDD Pattern, and RbOverhead for 273 PRBs)
    nrHelper->SetGnbPhyAttribute("Numerology", UintegerValue(numerology));
    nrHelper->SetGnbPhyAttribute("TxPower", DoubleValue(txPower));
    nrHelper->SetGnbPhyAttribute("Pattern", StringValue(kTddPattern));
    nrHelper->SetGnbPhyAttribute("RbOverhead", DoubleValue(kRbOverhead));
    nrHelper->SetUePhyAttribute("TxPower", DoubleValue(ueTxPower));

    idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                         TypeIdValue(DirectPathBeamforming::GetTypeId()));

    // 4T4R Antennas (nof_antennas_dl: 4, nof_antennas_ul: 4 -> 2x2 array)
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(kUeAntRows));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(kUeAntCols));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));

    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(kGnbAntRows));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(kGnbAntCols));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<IsotropicAntennaModel>()));

    // =========================================================================
    // Spectrum / BWP Configuration (100 MHz, SCS 30 kHz -> 273 PRBs)
    // =========================================================================
    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1;

    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency,
                                                   bandwidth,
                                                   numCcPerBand);

    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);
    channelHelper->AssignChannelsToBands({band});
    allBwps = CcBwpCreator::GetAllBwps({band});

    // Install NetDevices (PHY attributes configured on nrHelper are automatically applied)
    NetDeviceContainer gNbDevs = nrHelper->InstallGnbDevice(gNbNodes, allBwps);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, allBwps);

    // Schedule validation of the 273 physical PRBs and radio parameters
    Simulator::Schedule(Seconds(kRrcSettlingTimeSec),
                        &ValidateSpectrumConfiguration,
                        gNbDevs.Get(0),
                        centralFrequency,
                        bandwidth,
                        numerology);

    // =========================================================================
    // Internet Stack, Routing, and EPC
    // =========================================================================
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(remoteHostContainer);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.001)));

    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Assign IPv4 addresses to UEs
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set default route on UEs toward the EPC gateway
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4> ueIpv4 = ueNodes.Get(i)->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> ueStatic = ipv4RoutingHelper.GetStaticRouting(ueIpv4);
        ueStatic->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Attach UEs to the gNB (also synchronizes RbOverhead between gNB and UEs)
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        nrHelper->AttachToGnb(ueDevs.Get(i), gNbDevs.Get(0));
    }

    // =========================================================================
    // 3-Slice Mapping (SST 1, 2, 3) in Radio Scheduler and KPM
    // =========================================================================
    std::vector<int> uesPerSlice = {1, 1, 1};     // 1 UE per slice
    std::vector<uint8_t> sstPerSlice = {1, 2, 3}; // SST 1 (Streaming), SST 2 (Social), SST 3 (Gaming)

    NoriSlicingHelper::ScheduleSliceMapping(Seconds(kRrcSettlingTimeSec),
                                            true,
                                            uesPerSlice,
                                            sstPerSlice,
                                            gNbDevs,
                                            ueDevs);

    // =========================================================================
    // E2 Interface Installation (O-RAN E2SM-KPM / RC / CCC)
    // =========================================================================
    // e2Helper must remain in scope for the entire simulation lifetime
    Ptr<E2TermHelper> e2Helper;
    if (enableE2)
    {
        NS_LOG_INFO("Inicializando Interface E2 para o Near-RT RIC ("
                    << ipE2TermRic << ":" << ipE2TermRicPort
                    << ") com periodicidade de telemetria " << e2Periodicity << "s...");
        Config::SetDefault("ns3::E2Interface::E2Periodicity", DoubleValue(e2Periodicity));
        e2Helper = CreateObject<E2TermHelper>();
        e2Helper->SetAttribute("E2TermIp", StringValue(ipE2TermRic));
        e2Helper->SetAttribute("E2Port", UintegerValue(ipE2TermRicPort));
        e2Helper->InstallE2Term(gNbDevs.Get(0));
    }

    // =========================================================================
    // Fixed Downlink Traffic Profiles (UFA35-WP1 / GreenRAN Testbed)
    // =========================================================================
    // 1. UE 0: Video Streaming (high continuous downlink rate)
    //    - DataRate: 30 Mbps, PacketSize: 1400 B, OnTime: 1.0s, OffTime: 0.0s
    // 2. UE 1: Social Network (intermittent bursty web browsing)
    //    - DataRate: 10 Mbps, PacketSize: 1200 B, OnTime: 0.5s, OffTime: 0.5s
    // 3. UE 2: Cloud Gaming (Moonlight/Sunshine Xonotic, low-latency constraint)
    //    - DataRate: 18 Mbps, PacketSize: 1100 B, OnTime: 1.0s, OffTime: 0.01s

    struct TrafficProfile
    {
        uint16_t port;
        std::string dataRate;
        uint32_t packetSize;
        double onTime;
        double offTime;
        std::string name;
    };

    const std::vector<TrafficProfile> profiles = {
        {1234, "30Mbps", 1400, 1.0, 0.0, "Slice 1: Video Streaming (SST 1)"},
        {1235, "10Mbps", 1200, 0.5, 0.5, "Slice 2: Social Network (SST 2)"},
        {1236, "18Mbps", 1100, 1.0, 0.01, "Slice 3: Cloud Gaming (SST 3)"}
    };

    std::map<Ipv4Address, uint32_t> ueIpToIndex;

    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        Ipv4Address ueIp = ueIpIfaces.GetAddress(i);
        ueIpToIndex[ueIp] = i;
        const auto& prof = profiles[i];

        // UE receiver (PacketSink)
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), prof.port));
        ApplicationContainer sinkApps = sinkHelper.Install(ueNodes.Get(i));
        sinkApps.Start(Seconds(kSinkStartTimeSec));
        if (simTime > 0.0)
        {
            sinkApps.Stop(Seconds(simTime));
        }

        // Downlink sender on RemoteHost (OnOffApplication)
        OnOffHelper clientHelper("ns3::UdpSocketFactory", InetSocketAddress(ueIp, prof.port));
        clientHelper.SetAttribute("PacketSize", UintegerValue(prof.packetSize));
        clientHelper.SetAttribute("DataRate", DataRateValue(DataRate(prof.dataRate)));
        clientHelper.SetAttribute("OnTime",
                                  StringValue("ns3::ConstantRandomVariable[Constant=" +
                                              std::to_string(prof.onTime) + "]"));
        clientHelper.SetAttribute("OffTime",
                                  StringValue("ns3::ConstantRandomVariable[Constant=" +
                                              std::to_string(prof.offTime) + "]"));

        ApplicationContainer clientApps = clientHelper.Install(remoteHost);
        clientApps.Start(Seconds(kTrafficStartTimeSec));
        if (simTime > 0.0)
        {
            clientApps.Stop(Seconds(simTime));
        }

        NS_LOG_INFO("Configurado " << prof.name << " -> UE[" << i << "] (" << ueIp
                    << ":" << prof.port << ") - Taxa: " << prof.dataRate
                    << ", Pacote: " << prof.packetSize << " B, On: " << prof.onTime
                    << "s, Off: " << prof.offTime << "s");
    }

    // =========================================================================
    // FlowMonitor for Local Performance Collection and Validation
    // =========================================================================
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Ipv4Address ueNetworkAddress("7.0.0.0");
    Ipv4Mask ueNetworkMask("255.0.0.0");
    Ptr<SliceFlowMetricsContext> metricsCtx = Create<SliceFlowMetricsContext>(ueNum);

    Simulator::Schedule(Seconds(statsInterval + kTrafficStartTimeSec),
                        &PrintPeriodicFlowStats,
                        monitor,
                        &flowmonHelper,
                        metricsCtx,
                        ueIpToIndex,
                        ueNetworkAddress,
                        ueNetworkMask,
                        simTime,
                        statsInterval);

    // =========================================================================
    // Simulation Execution
    // =========================================================================
    if (simTime > 0.0)
    {
        NS_LOG_INFO("Iniciando Simulação por " << simTime << " segundos...");
        Simulator::Stop(Seconds(simTime));
    }
    else
    {
        NS_LOG_INFO("Iniciando Simulação Contínua (executando até Ctrl+C)...");
    }

    Simulator::Run();

    Simulator::Destroy();
    NS_LOG_INFO("Simulação concluída com sucesso.");
    return 0;
}
