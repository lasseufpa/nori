/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * NORI Benchmark Scenario for Performance & Execution Time Evaluation
 *
 * Designed to match the GUARA-ns / RAN Fusion evaluation methodology:
 * - 3GPP UMi channel model (Urban Micro, TR 38.901)
 * - Scalable topology (X gNBs and Y UEs)
 * - Presets for standard evaluation scenarios:
 *     1: 10 UEs, 2 gNBs
 *     2: 1 UE, 50 gNBs
 *     3: 1 UE, 10 gNBs
 *     4: 1 UE, 5 gNBs
 *     5: 10 UEs, 50 gNBs
 * - Periodic 1.0s telemetry & lightweight logging
 * - Wall-clock execution time & speedup factor measurement
 */

#include "ns3/E2-term-helper.h"
#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/grid-scenario-helper.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nori-module.h"
#include "ns3/nr-helper.h"
#include "ns3/nr-module.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/point-to-point-helper.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NoriBenchmarkScenario");

/**
 * @brief Periodic statistics callback executed every 1.0s (or user-defined interval).
 * Lightweight and fast to avoid simulation overhead.
 */
static void
PrintPeriodicStats(Ptr<FlowMonitor> monitor,
                   FlowMonitorHelper* flowmonHelper,
                   double simTime,
                   double interval)
{
    double now = Simulator::Now().GetSeconds();
    if (simTime > 0.0 && now > simTime)
    {
        return;
    }

    monitor->CheckForLostPackets();
    const std::map<FlowId, FlowMonitor::FlowStats>& statsMap = monitor->GetFlowStats();

    uint64_t totalTx = 0;
    uint64_t totalRx = 0;
    uint64_t totalRxBytes = 0;
    double totalDelaySum = 0.0;
    uint32_t activeFlows = 0;

    static uint64_t lastRxBytes = 0;
    static double lastTime = 0.0;

    for (const auto& it : statsMap)
    {
        const FlowMonitor::FlowStats& stats = it.second;
        if (stats.txPackets > 0)
        {
            activeFlows++;
            totalTx += stats.txPackets;
            totalRx += stats.rxPackets;
            totalRxBytes += stats.rxBytes;
            totalDelaySum += stats.delaySum.GetSeconds();
        }
    }

    double deltaT = (lastTime > 0.0) ? (now - lastTime) : interval;
    if (deltaT <= 0.0)
    {
        deltaT = interval;
    }

    uint64_t intervalBytes = (totalRxBytes >= lastRxBytes) ? (totalRxBytes - lastRxBytes) : totalRxBytes;
    double throughputMbps = (intervalBytes * 8.0) / (deltaT * 1e6);
    double avgDelayMs = (totalRx > 0) ? (totalDelaySum / totalRx * 1e3) : 0.0;
    double lossPct = (totalTx > 0) ? ((double)(totalTx - totalRx) * 100.0 / totalTx) : 0.0;

    std::cout << "[t = " << std::fixed << std::setprecision(1) << now << "s] "
              << "Active Flows: " << std::setw(2) << activeFlows
              << " | T-put: " << std::setw(6) << std::fixed << std::setprecision(2) << throughputMbps << " Mbps"
              << " | Delay: " << std::setw(5) << std::fixed << std::setprecision(2) << avgDelayMs << " ms"
              << " | Loss: " << std::setw(4) << std::fixed << std::setprecision(1) << lossPct << " %"
              << std::endl;

    lastRxBytes = totalRxBytes;
    lastTime = now;

    if (simTime <= 0.0 || now + interval <= simTime)
    {
        Simulator::Schedule(Seconds(interval),
                            &PrintPeriodicStats,
                            monitor,
                            flowmonHelper,
                            simTime,
                            interval);
    }
}

int
main(int argc, char* argv[])
{
    // Default parameters
    uint32_t preset = 1;             // 1: 10 UEs, 2 gNBs
    uint32_t gNbNum = 2;             // Number of gNBs
    uint32_t ueNum = 10;             // Number of UEs
    double simTime = 10.0;           // Simulation duration in seconds
    double statsInterval = 1.0;      // 1-second telemetry interval
    double centralFrequency = 3.5e9; // 3.5 GHz (NR Band n78)
    double bandwidth = 100e6;        // 100 MHz bandwidth
    uint16_t numerology = 1;         // 30 kHz Subcarrier Spacing (SCS)
    std::string trafficRate = "10Mb/s";
    uint32_t packetSize = 1024;      // Packet size in bytes
    bool enableRealtime = false;     // false for fast simulated execution time benchmarking
    bool enableE2 = false;           // true if connecting to external RIC
    std::string ipE2TermRic = "10.244.0.246";
    uint16_t ipE2TermRicPort = 36422;
    double gnbDistance = 20.0;       // Inter-gNB distance in meters

    CommandLine cmd(__FILE__);
    cmd.AddValue("preset",
                 "Scenario Preset (1: 2 gNBs/10 UEs, 2: 50 gNBs/1 UE, 3: 10 gNBs/1 UE, 4: 5 gNBs/1 UE, 5: 50 gNBs/10 UEs)",
                 preset);
    cmd.AddValue("gNbNum", "Number of gNB nodes (overrides preset)", gNbNum);
    cmd.AddValue("ueNum", "Number of UE nodes (overrides preset)", ueNum);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("statsInterval", "Periodic stats interval in seconds (default 1.0s)", statsInterval);
    cmd.AddValue("centralFrequency", "Carrier central frequency in Hz", centralFrequency);
    cmd.AddValue("bandwidth", "Carrier bandwidth in Hz", bandwidth);
    cmd.AddValue("numerology", "5G NR numerology (0: 15kHz, 1: 30kHz, 2: 60kHz)", numerology);
    cmd.AddValue("trafficRate", "Traffic rate per UE (e.g. 10Mb/s)", trafficRate);
    cmd.AddValue("packetSize", "UDP Packet size in bytes", packetSize);
    cmd.AddValue("enableRealtime", "Enable RealtimeSimulatorImpl (true for live RIC sync, false for max speed)", enableRealtime);
    cmd.AddValue("enableE2", "Enable E2Term interface installation on gNBs", enableE2);
    cmd.AddValue("ipE2TermRic", "IP address of the Near-RT RIC e2term", ipE2TermRic);
    cmd.AddValue("ipE2TermRicPort", "SCTP Port of the Near-RT RIC e2term", ipE2TermRicPort);
    cmd.AddValue("gnbDistance", "Distance between gNBs in meters", gnbDistance);
    cmd.Parse(argc, argv);

    // Apply preset defaults if specific gNbNum/ueNum were not explicitly modified
    if (preset >= 1 && preset <= 5)
    {
        switch (preset)
        {
        case 1:
            gNbNum = 2;
            ueNum = 10;
            break;
        case 2:
            gNbNum = 50;
            ueNum = 1;
            break;
        case 3:
            gNbNum = 10;
            ueNum = 1;
            break;
        case 4:
            gNbNum = 5;
            ueNum = 1;
            break;
        case 5:
            gNbNum = 50;
            ueNum = 10;
            break;
        }
    }

    // Re-parse command line in case user passed both --preset and specific overrides
    cmd.Parse(argc, argv);

    // Sanitize string inputs from quotes/spaces
    ipE2TermRic.erase(std::remove(ipE2TermRic.begin(), ipE2TermRic.end(), '\"'), ipE2TermRic.end());
    ipE2TermRic.erase(std::remove(ipE2TermRic.begin(), ipE2TermRic.end(), '\''), ipE2TermRic.end());
    ipE2TermRic.erase(std::remove(ipE2TermRic.begin(), ipE2TermRic.end(), ' '), ipE2TermRic.end());

    if (enableRealtime)
    {
        GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    }

    // Fast simulation optimizations
    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(999999999));
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(0)));

    std::cout << "======================================================================\n"
              << " Starting NORI Benchmark Scenario\n"
              << " Preset: " << preset << " | gNBs: " << gNbNum << " | UEs: " << ueNum << "\n"
              << " Channel: 3GPP UMi | SimTime: " << simTime << "s | Interval: " << statsInterval << "s\n"
              << " Mode: " << (enableRealtime ? "Realtime Clock" : "Fast Simulated Time")
              << " | E2 Interface: " << (enableE2 ? "Enabled" : "Disabled") << "\n"
              << "======================================================================" << std::endl;

    // 1. Grid Topology Generation (gNBs and UEs)
    uint32_t cols = std::max<uint32_t>(1, std::ceil(std::sqrt(gNbNum)));
    uint32_t rows = std::max<uint32_t>(1, std::ceil(static_cast<double>(gNbNum) / cols));

    int64_t randomStream = 1;
    GridScenarioHelper gridScenario;
    gridScenario.SetRows(rows);
    gridScenario.SetColumns(cols);
    gridScenario.SetHorizontalBsDistance(gnbDistance);
    gridScenario.SetVerticalBsDistance(gnbDistance);
    gridScenario.SetBsHeight(10.0);
    gridScenario.SetUtHeight(1.5);
    gridScenario.SetSectorization(GridScenarioHelper::SINGLE);
    gridScenario.SetBsNumber(gNbNum);
    gridScenario.SetUtNumber(ueNum);
    gridScenario.SetScenarioHeight(std::max(30.0, rows * gnbDistance));
    gridScenario.SetScenarioLength(std::max(30.0, cols * gnbDistance));
    randomStream += gridScenario.AssignStreams(randomStream);
    gridScenario.CreateScenario();

    NodeContainer gnbs = gridScenario.GetBaseStations();
    NodeContainer ues = gridScenario.GetUserTerminals();

    // 2. Core (EPC) & NR Helpers
    Ptr<NrPointToPointEpcHelper> nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(nrEpcHelper);
    nrEpcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));

    idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                         TypeIdValue(DirectPathBeamforming::GetTypeId()));

    // 3. 3GPP UMi Channel Model Configuration
    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("UMi", "Default", "ThreeGpp");
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    channelHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));

    // Spectrum configuration (1 Component Carrier, 1 BWP)
    CcBwpCreator ccBwpCreator;
    const uint8_t numOfCcs = 1;
    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency, bandwidth, numOfCcs);
    bandConf.m_numBwp = 1;
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);

    channelHelper->AssignChannelsToBands({band});
    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});

    // Antenna configurations
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(1));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(1));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));

    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(2));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<IsotropicAntennaModel>()));

    // 4. Install NetDevices on gNBs and UEs
    NetDeviceContainer gnbNetDev = nrHelper->InstallGnbDevice(gnbs, allBwps);
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ues, allBwps);

    randomStream += nrHelper->AssignStreams(gnbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueNetDev, randomStream);

    for (uint32_t i = 0; i < gnbNetDev.GetN(); ++i)
    {
        nrHelper->GetGnbPhy(gnbNetDev.Get(i), 0)->SetAttribute("Numerology", UintegerValue(numerology));
    }

    // 5. IP Stack & EPC Routing Configuration
    Ptr<Node> pgw = nrEpcHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    InternetStackHelper internet;
    internet.Install(remoteHostContainer);
    internet.Install(ues);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    Ipv4InterfaceContainer ueIpIfaces = nrEpcHelper->AssignUeIpv4Address(ueNetDev);

    for (uint32_t j = 0; j < ues.GetN(); ++j)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ues.Get(j)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(nrEpcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Attach UEs to closest gNBs
    nrHelper->AttachToClosestGnb(ueNetDev, gnbNetDev);

    // Optional E2 Interface Support (All gNBs have their own E2 interface)
    if (enableE2)
    {
        auto e2 = CreateObject<E2TermHelper>();
        e2->SetAttribute("E2TermIp", StringValue(ipE2TermRic));
        e2->SetAttribute("E2Port", UintegerValue(ipE2TermRicPort));
        e2->InstallE2Term(gnbNetDev);
    }

    // 6. Traffic Applications (Downlink UDP from remoteHost to UEs)
    uint16_t basePort = 5000;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < ues.GetN(); ++i)
    {
        uint16_t port = basePort + i;
        Ptr<Node> ueNode = ues.Get(i);
        Ptr<NetDevice> ueDevice = ueNetDev.Get(i);
        Ipv4Address ueAddr = ueIpIfaces.GetAddress(i);

        // Dedicated Bearer for UE traffic
        NrEpsBearer bearer(NrEpsBearer::NGBR_LOW_LAT_EMBB);
        Ptr<NrEpcTft> tft = Create<NrEpcTft>();
        NrEpcTft::PacketFilter dlpf;
        dlpf.localPortStart = port;
        dlpf.localPortEnd = port;
        tft->Add(dlpf);
        nrHelper->ActivateDedicatedEpsBearer(ueDevice, bearer, tft);

        // Sink on UE
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        serverApps.Add(sink.Install(ueNode));

        // Source on RemoteHost
        OnOffHelper clientApp("ns3::UdpSocketFactory", InetSocketAddress(ueAddr, port));
        clientApp.SetAttribute("DataRate", DataRateValue(DataRate(trafficRate)));
        clientApp.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientApp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
        clientApp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
        clientApps.Add(clientApp.Install(remoteHost));
    }

    serverApps.Start(Seconds(0.0));
    clientApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(simTime));
    clientApps.Stop(Seconds(simTime));

    // 7. FlowMonitor & Periodic Telemetry
    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost);
    endpointNodes.Add(ues);

    Ptr<FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

    Simulator::Schedule(Seconds(statsInterval),
                        &PrintPeriodicStats,
                        monitor,
                        &flowmonHelper,
                        simTime,
                        statsInterval);

    // 8. Run Simulation & Measure Wall-Clock Execution Time
    Simulator::Stop(Seconds(simTime));

    auto startWallClock = std::chrono::high_resolution_clock::now();
    Simulator::Run();
    auto endWallClock = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsedSeconds = endWallClock - startWallClock;
    double wallTime = elapsedSeconds.count();
    double speedup = (wallTime > 0.0) ? (simTime / wallTime) : 0.0;

    // 9. Post-Simulation Summary
    monitor->CheckForLostPackets();
    const std::map<FlowId, FlowMonitor::FlowStats>& statsMap = monitor->GetFlowStats();

    uint64_t totalTxPackets = 0;
    uint64_t totalRxPackets = 0;
    uint64_t totalRxBytes = 0;
    double totalDelaySum = 0.0;

    for (const auto& it : statsMap)
    {
        const FlowMonitor::FlowStats& stats = it.second;
        totalTxPackets += stats.txPackets;
        totalRxPackets += stats.rxPackets;
        totalRxBytes += stats.rxBytes;
        totalDelaySum += stats.delaySum.GetSeconds();
    }

    double finalLossPct = (totalTxPackets > 0)
                              ? ((double)(totalTxPackets - totalRxPackets) * 100.0 / totalTxPackets)
                              : 0.0;
    double finalAvgDelayMs = (totalRxPackets > 0) ? (totalDelaySum / totalRxPackets * 1e3) : 0.0;
    double finalThroughputMbps = (simTime > 0.1) ? ((totalRxBytes * 8.0) / (simTime - 0.1) / 1e6) : 0.0;

    std::cout << "\n======================================================================\n"
              << "                 NORI SIMULATION BENCHMARK SUMMARY                    \n"
              << "======================================================================\n"
              << " Scenario Preset        : Preset " << preset << "\n"
              << " Topology               : " << gNbNum << " gNBs, " << ueNum << " UEs\n"
              << " Channel Model          : 3GPP UMi (TR 38.901)\n"
              << " Telemetry Interval     : " << statsInterval << " s\n"
              << " Simulated Time         : " << std::fixed << std::setprecision(2) << simTime << " s\n"
              << " Wall-Clock Exec Time   : " << std::fixed << std::setprecision(4) << wallTime << " s\n"
              << " Execution Speedup      : " << std::fixed << std::setprecision(2) << speedup << "x real-time\n"
              << " Total Tx / Rx Packets  : " << totalTxPackets << " / " << totalRxPackets << "\n"
              << " Packet Loss Ratio      : " << std::fixed << std::setprecision(2) << finalLossPct << " %\n"
              << " Total Throughput       : " << std::fixed << std::setprecision(2) << finalThroughputMbps << " Mbps\n"
              << " Mean E2E Delay         : " << std::fixed << std::setprecision(2) << finalAvgDelayMs << " ms\n"
              << "======================================================================\n"
              << std::endl;

    Simulator::Destroy();
    return 0;
}
