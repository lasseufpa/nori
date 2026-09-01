/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2026 LASSE/UFPA - Universidade Federal do Pará
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Andrey Adailso <andreyadailsom@gmail.com>
 *
 * E2SM-RC v3.01 Dynamic Slicing Demo Scenario:
 * 1 gNB and 2 UEs in separate network slices:
 *   - Slice 0: eMBB  (SST = 1, e.g., default quota: 70% max PRB)
 *   - Slice 1: URLLC (SST = 2, e.g., default quota: 30% max PRB)
 *
 * The gNB runs NrRLMacSchedulerOfdma and registers both E2SM-KPM (ID 200)
 * and E2SM-RC (ID 300) with the Near-RT RIC. When RICcontrolRequest directives
 * arrive from the RIC xApp, PRB quotas are updated in real-time.
 */

#include "ns3/E2-term-helper.h"
#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nori-module.h"
#include "ns3/nori-slicing-helper.h"
#include "ns3/nr-helper.h"
#include "ns3/nr-module.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/nr-rl-mac-scheduler-ofdma.h"
#include "ns3/point-to-point-helper.h"

#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NoriRcSlicingDemo");

// Helper to print periodic per-UE / per-Slice flow statistics
void
PrintPeriodicFlowStats(Ptr<FlowMonitor> monitor,
                       FlowMonitorHelper* flowmonHelper,
                       const std::map<Ipv4Address, uint32_t>& ueIpToIndex,
                       Ipv4Address ueNetworkAddress,
                       Ipv4Mask ueNetworkMask,
                       double simTime,
                       double interval)
{
    double now = Simulator::Now().GetSeconds();
    if (now > simTime)
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
    std::vector<double> ueFirstTx(ueCount, 0.0);
    std::vector<double> ueLastRx(ueCount, 0.0);
    std::vector<bool> ueHasFirstTx(ueCount, false);

    for (const auto& it : statsMap)
    {
        FlowId flowId = it.first;
        const FlowMonitor::FlowStats& stats = it.second;
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);

        bool ueAsDest = ueNetworkMask.IsMatch(t.destinationAddress, ueNetworkAddress);
        if (!ueAsDest)
        {
            continue;
        }

        auto itIdx = ueIpToIndex.find(t.destinationAddress);
        if (itIdx == ueIpToIndex.end())
        {
            continue;
        }

        uint32_t idx = itIdx->second;
        if (idx >= ueCount)
        {
            continue;
        }

        ueTxPackets[idx] += stats.txPackets;
        ueRxPackets[idx] += stats.rxPackets;
        ueRxBytes[idx] += stats.rxBytes;
        ueDelaySum[idx] += stats.delaySum.GetSeconds();

        double firstTx = stats.timeFirstTxPacket.GetSeconds();
        double lastRx = stats.timeLastRxPacket.GetSeconds();

        if (!ueHasFirstTx[idx] || firstTx < ueFirstTx[idx])
        {
            ueFirstTx[idx] = firstTx;
            ueHasFirstTx[idx] = true;
        }
        if (stats.rxPackets > 0 && lastRx > ueLastRx[idx])
        {
            ueLastRx[idx] = lastRx;
        }
    }

    std::cout << "\n========================================================" << std::endl;
    std::cout << "[t = " << std::fixed << std::setprecision(1) << now
              << "s] Per-Slice Performance Monitoring:" << std::endl;
    std::cout << "========================================================" << std::endl;

    for (uint32_t i = 0; i < ueCount; ++i)
    {
        double throughput = 0.0;
        double delayMs = 0.0;
        double lossRatio = 0.0;
        std::string sliceName = (i == 0) ? "Slice 0 (eMBB, SST=1)" : "Slice 1 (URLLC, SST=2)";

        if (ueTxPackets[i] > 0)
        {
            if (ueRxPackets[i] > 0 && ueHasFirstTx[i])
            {
                double duration = ueLastRx[i] - ueFirstTx[i];
                if (duration <= 0.0)
                {
                    duration = 1e-9;
                }
                throughput = (ueRxBytes[i] * 8.0) / duration / 1e6; // Mbps
                delayMs = (ueDelaySum[i] / ueRxPackets[i]) * 1e3;    // ms
            }
            lossRatio = (double)(ueTxPackets[i] - ueRxPackets[i]) * 100.0 / ueTxPackets[i];
        }

        std::cout << "  " << sliceName << " -> UE[" << i << "]: "
                  << "Throughput = " << std::fixed << std::setprecision(2) << throughput << " Mbps, "
                  << "Delay = " << delayMs << " ms, "
                  << "Loss = " << lossRatio << " %" << std::endl;
    }
    std::cout << "========================================================\n" << std::endl;

    if (now + interval <= simTime)
    {
        Simulator::Schedule(Seconds(interval),
                            &PrintPeriodicFlowStats,
                            monitor,
                            flowmonHelper,
                            ueIpToIndex,
                            ueNetworkAddress,
                            ueNetworkMask,
                            simTime,
                            interval);
    }
}

int
main(int argc, char* argv[])
{
    LogComponentEnable("NoriRcSlicingDemo", LOG_LEVEL_INFO);
    LogComponentEnable("E2Interface", LOG_LEVEL_INFO);
    LogComponentEnable("E2Termination", LOG_LEVEL_INFO);
    LogComponentEnable("RicControlMessage", LOG_LEVEL_INFO);
    LogComponentEnable("RicControlFunctionDescription", LOG_LEVEL_INFO);

    // Scenario parameters
    uint16_t gNbNum = 1;
    uint16_t ueNum = 2; // UE 0: eMBB, UE 1: URLLC
    double simTime = 20.0;
    double centralFrequency = 3.5e9; // 3.5 GHz
    double bandwidth = 100e6;        // 100 MHz
    uint16_t numerology = 1;         // 30 kHz SCS

    std::string ipE2TermRic = "10.244.0.246";
    uint16_t ipE2TermRicPort = 36422;
    bool enableRealtime = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time in seconds", simTime);
    cmd.AddValue("centralFrequency", "Central carrier frequency in Hz", centralFrequency);
    cmd.AddValue("bandwidth", "Bandwidth in Hz", bandwidth);
    cmd.AddValue("numerology", "5G NR Numerology (0:15kHz, 1:30kHz, 2:60kHz)", numerology);
    cmd.AddValue("ipE2TermRic", "IP address of the Near-RT RIC e2term", ipE2TermRic);
    cmd.AddValue("ipE2TermRicPort", "SCTP Port of the Near-RT RIC e2term", ipE2TermRicPort);
    cmd.AddValue("enableRealtime", "Enable realtime simulation clock for RIC SCTP sync", enableRealtime);
    cmd.Parse(argc, argv);

    if (enableRealtime)
    {
        GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    }

    NS_LOG_INFO("Configuring 5G NR Topology: 1 gNB, " << ueNum << " UEs (eMBB + URLLC slices)");

    // 1. Create Nodes
    NodeContainer gNbNodes;
    gNbNodes.Create(gNbNum);

    NodeContainer ueNodes;
    ueNodes.Create(ueNum);

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // 2. Mobility Configuration
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // gNB at origin
    Ptr<ListPositionAllocator> gNbPositionAlloc = CreateObject<ListPositionAllocator>();
    gNbPositionAlloc->Add(Vector(0.0, 0.0, 10.0));
    mobility.SetPositionAllocator(gNbPositionAlloc);
    mobility.Install(gNbNodes);

    // UEs at distances 20m (eMBB) and 40m (URLLC)
    Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
    uePositionAlloc->Add(Vector(20.0, 0.0, 1.5)); // UE 0
    uePositionAlloc->Add(Vector(40.0, 0.0, 1.5)); // UE 1
    mobility.SetPositionAllocator(uePositionAlloc);
    mobility.Install(ueNodes);

    // 3. NR and EPC Helpers
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(epcHelper);

    // Set RL Mac Scheduler (with Slicing support)
    nrHelper->SetGnbMacAttribute("SchedulerTypeId", TypeIdValue(NrRLMacSchedulerOfdma::GetTypeId()));

    // 4. Configure Spectrum / Bandwidth Part
    CcBwpCreator ccBwpCreator;
    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency, bandwidth, 1, numerology);
    CcBwpCreator::SimpleOperationBandConfVector bandsConfVector = {bandConf};
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandsConfVector);

    nrHelper->InitializeOperationBand(&band);

    // Bandwidth Part Manager
    nrHelper->SetGnbBwpManagerAlgorithmAttribute("NGBwpManagerAlgorithm",
                                                 TypeIdValue(TypeId::LookupByName("ns3::NrGnbBwpManagerAlgorithm")));
    nrHelper->SetUeBwpManagerAlgorithmAttribute("NUBwpManagerAlgorithm",
                                                TypeIdValue(TypeId::LookupByName("ns3::NrUeBwpManagerAlgorithm")));

    // 5. Install NetDevices
    NetDeviceContainer gNbDevs = nrHelper->InstallGnbDevice(gNbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // 6. Setup Internet Stack & EPC Network
    InternetStackHelper internet;
    internet.Install(ueNodes);

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

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to gNB
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        nrHelper->AttachToGnb(ueDevs.Get(i), gNbDevs.Get(0));
    }

    // 7. Configure Slicing (SST=1 for eMBB, SST=2 for URLLC)
    std::vector<int> uesPerSlice = {1, 1};        // 1 UE in eMBB, 1 UE in URLLC
    std::vector<uint8_t> sstPerSlice = {1, 2};    // SST 1 (eMBB), SST 2 (URLLC)

    NoriSlicingHelper::ScheduleSliceMapping(Seconds(0.1),
                                            true,
                                            uesPerSlice,
                                            sstPerSlice,
                                            gNbDevs,
                                            ueDevs);

    // 8. Install E2 Termination and Register KPM v3 + RC v3
    Ptr<E2TermHelper> e2Helper = CreateObject<E2TermHelper>();
    e2Helper->SetAttribute("E2TermIp", StringValue(ipE2TermRic));
    e2Helper->SetAttribute("E2Port", UintegerValue(ipE2TermRicPort));
    e2Helper->InstallE2Term(gNbDevs.Get(0));

    // 9. Install Traffic Applications
    // RemoteHost sends downlink UDP traffic to each UE
    uint16_t dlPort = 1234;
    std::map<Ipv4Address, uint32_t> ueIpToIndex;

    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        Ipv4Address ueIp = ueIpIfaces.GetAddress(i);
        ueIpToIndex[ueIp] = i;

        // Packet Sink on UE
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), dlPort));
        ApplicationContainer sinkApps = sinkHelper.Install(ueNodes.Get(i));
        sinkApps.Start(Seconds(0.5));
        sinkApps.Stop(Seconds(simTime));

        // Downlink Traffic Source on RemoteHost
        OnOffHelper clientHelper("ns3::UdpSocketFactory", InetSocketAddress(ueIp, dlPort));
        clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

        if (i == 0)
        {
            // eMBB: High throughput stream (20 Mbps)
            clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("20Mb/s")));
            clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
            clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
        }
        else
        {
            // URLLC: Lower rate, frequent packets (5 Mbps)
            clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mb/s")));
            clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
            clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
        }

        ApplicationContainer clientApps = clientHelper.Install(remoteHost);
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simTime));
    }

    // 10. FlowMonitor for Per-Slice Performance Tracking
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Ipv4Address ueNetworkAddress("7.0.0.0");
    Ipv4Mask ueNetworkMask("255.0.0.0");

    Simulator::Schedule(Seconds(2.0),
                        &PrintPeriodicFlowStats,
                        monitor,
                        &flowmonHelper,
                        ueIpToIndex,
                        ueNetworkAddress,
                        ueNetworkMask,
                        simTime,
                        2.0);

    NS_LOG_INFO("Starting Simulation for " << simTime << " seconds...");
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    Simulator::Destroy();
    NS_LOG_INFO("Simulation finished successfully.");
    return 0;
}
