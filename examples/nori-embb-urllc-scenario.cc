// NR eMBB/URLLC scenario with EPC/PGW (end-to-end IP UE <-> remoteHost)
#include "ns3/E2-term-helper.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/nr-rl-mac-scheduler-ofdma.h"
#include "ns3/nr-ue-net-device.h"
#include "ns3/nr-ue-mac.h"
#include "ns3/nori-slicing-helper.h"
#include <nlohmann/json.hpp>

#include <iomanip>
#include <iostream>
#include <map>

#include <vector>
#include <numeric>


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("nori-embb-urllc-scenario");

int main(int argc, char* argv[])
{   
    LogComponentEnable("nori-embb-urllc-scenario", LOG_LEVEL_INFO);
    LogComponentEnable("E2Interface", LOG_LEVEL_INFO);
    LogComponentEnable("E2Termination", LOG_LEVEL_INFO);
    //LogComponentEnable("NrRLMacSchedulerOfdma", LOG_LEVEL_INFO);

    uint16_t gNbNum = 1;
    uint16_t ueNum = 2;
    double simTime = 10.0;
    double interSiteDistance = 10.0;
    double centralFrequency = 28e9;
    double bandwidth = 100e6;

    uint16_t numerology = 0;
    double txPower = 0.0;
    double ueTxPower = 0.0;

    std::string ipE2TermRic = "10.244.0.246";

    std::vector<int> uesPerSlice;
    std::vector<std::string> trafficTypes;

    // Per-traffic-type traffic parameters
    struct TrafficProfile {
        double dataRate;
        uint16_t packetSize;
        double onTime;
        double offTime;
    };
    std::map<std::string, TrafficProfile> trafficProfiles;

    std::ifstream configFile("/home/openran-br/ns-3-dev/contrib/nori/examples/config.json");
    if (configFile.is_open()) {
        nlohmann::json configJson;
        configFile >> configJson;
        configFile.close();

        gNbNum = configJson["topology"].value("numGNb", gNbNum);
        interSiteDistance = configJson["topology"].value("distance", interSiteDistance);

        simTime = configJson["simulation"].value("duration", simTime);

        numerology = configJson["NR"].value("numerology", numerology);
        double bwMHz = configJson["NR"]["bandwidthMHz"].get<double>();
        bandwidth = bwMHz * 1e6;
        centralFrequency = configJson["NR"].value("centralFrequency", centralFrequency);
        txPower = configJson["NR"].value("txPower", txPower);
        ueTxPower = configJson["NR"].value("ueTxPower", ueTxPower);

        std::vector<uint32_t> jsonSlices = configJson["slices"]["UesPerSlice"];

        for (uint32_t& item : jsonSlices) {
            NS_LOG_INFO("Number of UEs per slice: " << item);
            uesPerSlice.push_back(item);
        }

        if (configJson["slices"].contains("trafficTypes")) {
            trafficTypes = configJson["slices"]["trafficTypes"].get<std::vector<std::string>>();
            for (size_t i = 0; i < trafficTypes.size(); ++i) {
                NS_LOG_INFO("Slice " << i << " traffic type: " << trafficTypes[i]);
            }
        }

        // Ler parâmetros de tráfego para cada tipo
        if (configJson["traffic"].contains("eMBB")) {
            auto embbConfig = configJson["traffic"]["eMBB"];
            TrafficProfile embbProfile;
            embbProfile.dataRate = embbConfig.value("bitrateMbps", 0.0);
            embbProfile.packetSize = embbConfig.value("packetSize", static_cast<uint16_t>(0));
            embbProfile.onTime = embbConfig.value("onTimeMean", 1.0);
            embbProfile.offTime = embbConfig.value("offTimeMean", 0.01);
            trafficProfiles["eMBB"] = embbProfile;
        }

        if (configJson["traffic"].contains("URLLC")) {
            auto urllcConfig = configJson["traffic"]["URLLC"];
            TrafficProfile urllcProfile;
            urllcProfile.dataRate = urllcConfig.value("bitrateMbps", 0.0);
            urllcProfile.packetSize = urllcConfig.value("packetSize", static_cast<uint16_t>(0));
            urllcProfile.onTime = urllcConfig.value("onTimeMean", 0.5);
            urllcProfile.offTime = urllcConfig.value("offTimeMean", 0.01);
            trafficProfiles["URLLC"] = urllcProfile;
        }

        ueNum = std::accumulate(uesPerSlice.begin(), uesPerSlice.end(), 0);
        NS_LOG_INFO("Total number of UEs (from slice configuration): " << ueNum);

    }else {
        NS_LOG_ERROR("Could not open configuration file; using default parameters.");
    }

    // Map each UE to its slice and traffic type (for post-processing)
    std::vector<int> ueSliceId(ueNum, -1);
    std::vector<std::string> ueSliceTrafficType(ueNum, "");

    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));

    // Enable/disable RAN slicing with RL scheduler
    bool enableRanSlicing = true;
    
    CommandLine cmd;
    cmd.AddValue("enableRanSlicing", "Enable RAN Slicing with RL scheduler", enableRanSlicing);
    cmd.AddValue("ipE2TermRic", "Ip address of the E2 termination", ipE2TermRic);
    cmd.Parse(argc, argv);

    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetAttribute("UseIdealRrc", BooleanValue(true));
    nrHelper->SetGnbPhyAttribute("TbDecodeLatency", TimeValue(MicroSeconds(1.0)));
    nrHelper->SetUePhyAttribute("TbDecodeLatency", TimeValue(MicroSeconds(1.0)));
    nrHelper->SetGnbPhyAttribute("Numerology", UintegerValue(numerology));
    nrHelper->SetGnbPhyAttribute("TxPower", DoubleValue(txPower));
    nrHelper->SetUePhyAttribute("TxPower", DoubleValue(ueTxPower));
    //nrHelper->SetGnbPhyAttribute("DciProcessingDelay", TimeValue(MicroSeconds(1.0)));
    //nrHelper->SetUePhyAttribute("DciProcessingDelay", TimeValue(MicroSeconds(1.0)));
    
    // Configurar scheduler: RL com slicing ou RoundRobin padrão
    std::string schedulerType = enableRanSlicing ? "ns3::NrRLMacSchedulerOfdma" : "ns3::NrMacSchedulerOfdmaRR";
    nrHelper->SetSchedulerTypeId(TypeId::LookupByName(schedulerType));
    NS_LOG_INFO("Scheduler selecionado: " << schedulerType);
    
    // EPC helper
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    NodeContainer gNbNodes;
    gNbNodes.Create(gNbNum);

    NodeContainer ueNodes;
    ueNodes.Create(ueNum);

    // Remote host (server) connected to PGW via P2P link
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);

    // Mobility
    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<GridPositionAllocator> gnbPositionAlloc = CreateObject<GridPositionAllocator>();
    gnbPositionAlloc->SetAttribute("MinX", DoubleValue(0.0));
    gnbPositionAlloc->SetAttribute("MinY", DoubleValue(0.0));
    gnbPositionAlloc->SetAttribute("DeltaX", DoubleValue(interSiteDistance));
    gnbPositionAlloc->SetAttribute("DeltaY", DoubleValue(interSiteDistance));
    gnbPositionAlloc->SetAttribute("GridWidth", UintegerValue(3));
    gnbPositionAlloc->SetAttribute("LayoutType", StringValue("RowFirst"));
    gnbMobility.SetPositionAllocator(gnbPositionAlloc);
    gnbMobility.Install(gNbNodes);

    // Log initial gNB positions
    NS_LOG_INFO("*** gNB initial positions ***");
    for (uint32_t i = 0; i < gNbNodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mob = gNbNodes.Get(i)->GetObject<MobilityModel>();
        if (mob)
        {
            Vector pos = mob->GetPosition();
            NS_LOG_INFO("gNB[" << i << "] pos = (" << pos.x << ", " << pos.y << ", " << pos.z << ")");
        }
    }

    MobilityHelper ueMobility;
    Ptr<RandomRectanglePositionAllocator> positionAlloc = CreateObject<RandomRectanglePositionAllocator>();
    positionAlloc->SetAttribute("X", StringValue("ns3::UniformRandomVariable[Min=-20|Max=20]"));
    positionAlloc->SetAttribute("Y", StringValue("ns3::UniformRandomVariable[Min=-20|Max=20]"));
    ueMobility.SetPositionAllocator(positionAlloc);
    ueMobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                "Speed",
                                StringValue("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                                "Pause",
                                StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
                                "PositionAllocator",
                                PointerValue(positionAlloc));
    ueMobility.Install(ueNodes);

    // Log initial UE positions
    NS_LOG_INFO("*** UE initial positions ***");
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mob = ueNodes.Get(i)->GetObject<MobilityModel>();
        if (mob)
        {
            Vector pos = mob->GetPosition();
            NS_LOG_INFO("UE[" << i << "] pos = (" << pos.x << ", " << pos.y << ", " << pos.z << ")");
        }
    }

    // Antennas and channel
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(1));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(1));
    nrHelper->SetUeAntennaAttribute("AntennaElement", PointerValue(CreateObject<IsotropicAntennaModel>()));

    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("AntennaElement", PointerValue(CreateObject<IsotropicAntennaModel>()));

    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator ccBwpCreator;
    OperationBandInfo band;
    const uint8_t numOfCcs = 1;
    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency, bandwidth, numOfCcs);
    bandConf.m_numBwp = 1;
    band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);
    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("UMi", "Default", "ThreeGpp");
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    channelHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    channelHelper->AssignChannelsToBands({band});
    allBwps = CcBwpCreator::GetAllBwps({band});

    // Install IP stack on remoteHost and UEs
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);
    internet.Install(ueNodes);

    NetDeviceContainer gNbDevs = nrHelper->InstallGnbDevice(gNbNodes, allBwps);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, allBwps);

    // Enable E2 support on gNBs
    auto e2 = CreateObject<E2TermHelper>();
    e2->SetAttribute("E2TermIp", StringValue(ipE2TermRic));
    e2->InstallE2Term(gNbDevs);

    nrHelper->AttachToClosestGnb(ueDevs, gNbDevs);

    // Schedule slice mapping after RRC connection has been established
    NoriSlicingHelper::ScheduleSliceMapping(Seconds(1.0),
                                            enableRanSlicing,
                                            uesPerSlice,
                                            gNbDevs,
                                            ueDevs);

    // Connect remoteHost to PGW via P2P link
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2ph.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHostContainer.Get(0));

    // Endereçamento do link PGW <-> remoteHost
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.255.255.252");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1); // endereço do servidor
    Ipv4Address pgwAddr = internetIpIfaces.GetAddress(0);

    // epcHelper assigns IPv4 addresses to UEs
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(ueDevs);
    // Em muitas versões: Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(ueDevs);

    // Map IP -> UE index for FlowMonitor classification
    std::map<Ipv4Address, uint32_t> ueIpToIndex;

    NS_LOG_INFO("*** EPC-assigned addresses ***");
    NS_LOG_INFO("remoteHost (server): " << remoteHostAddr);
    NS_LOG_INFO("PGW: " << pgwAddr);
    for (uint32_t i = 0; i < ueIpIfaces.GetN(); ++i)
    {
        Ipv4Address addr = ueIpIfaces.GetAddress(i);
        ueIpToIndex[addr] = i;
        NS_LOG_INFO("UE[" << i << "] IP (via EPC): " << addr);
    }

    // Static route on remoteHost towards UE network via PGW
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4> remoteIpv4 = remoteHostContainer.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> remoteStatic = ipv4RoutingHelper.GetStaticRouting(remoteIpv4);
    remoteStatic->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), Ipv4Address("1.0.0.1"), 1);

    // Default route on UEs towards EPC gateway
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4> ueIpv4 = ueNodes.Get(i)->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> ueStatic = ipv4RoutingHelper.GetStaticRouting(ueIpv4);
        ueStatic->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
        NS_LOG_INFO("UE[" << i << "] default route: GW=" << epcHelper->GetUeDefaultGatewayAddress()
                         << " via interface 1");
    }

    // Applications: UDP sinks on UEs and OnOff sources on remoteHost (downlink)
    uint16_t portBase = 8080;

    // Enable dedicated downlink bearers (remoteHost -> UEs)
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        Ptr<NetDevice> ueDevice = ueDevs.Get(i);

        // Use a non-GBR low-latency bearer (as in other Nori examples)
        NrEpsBearer bearer(NrEpsBearer::NGBR_LOW_LAT_EMBB);

        Ptr<NrEpcTft> tft = Create<NrEpcTft>();

        // Downlink filter: localPort = UDP port where the UE sink listens
        uint16_t uePort = portBase + i;
        NrEpcTft::PacketFilter dlpf;
        dlpf.localPortStart = uePort;
        dlpf.localPortEnd = uePort;
        tft->Add(dlpf);

        nrHelper->ActivateDedicatedEpsBearer(ueDevice, bearer, tft);

        NS_LOG_INFO("Dedicated DL bearer activated for UE[" << i << "] port " << uePort);
    }

    // Install one UDP sink per UE; sinks start before traffic sources
    for (uint32_t i = 0; i < ueNum; ++i)
    {
        uint16_t port = portBase + i;
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sink.Install(ueNodes.Get(i));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(simTime));
        NS_LOG_INFO("Sink installed on port " << port << " at UE[" << i << "]");
    }

    // Application logic per slice
    uint32_t currentUeIndex = 0;

    for (size_t sliceId = 0; sliceId < uesPerSlice.size(); ++sliceId) 
    {
        int countUes = uesPerSlice[sliceId];
        
        // Traffic type per slice from configuration (fallback to default types)
        std::string trafficType = (sliceId < trafficTypes.size()) 
            ? trafficTypes[sliceId] 
            : ((sliceId == 0) ? "eMBB" : "URLLC");

        NS_LOG_INFO("Slice " << sliceId << " configured with traffic type: " << trafficType);

        for (int k = 0; k < countUes; ++k) 
        {
            if (currentUeIndex >= ueNodes.GetN()) break;

            uint32_t nodeIdx = currentUeIndex++;
            uint16_t port = portBase + nodeIdx;

            // Fallback to eMBB profile if traffic type is not configured
            std::string resolvedTrafficType = trafficType;
            if (trafficProfiles.find(resolvedTrafficType) == trafficProfiles.end()) {
                NS_LOG_WARN("Traffic type '" << resolvedTrafficType << "' not configured. Falling back to eMBB.");
                resolvedTrafficType = "eMBB";
            }

            TrafficProfile profile = trafficProfiles[resolvedTrafficType];

            Ipv4Address ueAddr = ueIpIfaces.GetAddress(nodeIdx);

            // Store UE -> slice and traffic type for FlowMonitor analysis
            ueSliceId[nodeIdx] = static_cast<int>(sliceId);
            ueSliceTrafficType[nodeIdx] = resolvedTrafficType;

            // Debug: downlink traffic remoteHost -> UE
            NS_LOG_INFO("DEBUG DL for UE[" << nodeIdx << "] (" << ueAddr
                        << "): port " << port << " type " << resolvedTrafficType);

            OnOffHelper trafficApp("ns3::UdpSocketFactory", InetSocketAddress(ueAddr, port));
            trafficApp.SetAttribute("DataRate", DataRateValue(DataRate(std::to_string((int)profile.dataRate) + "Mbps")));
            trafficApp.SetAttribute("PacketSize", UintegerValue(profile.packetSize));
            
            std::string onTimeStr = "ns3::ExponentialRandomVariable[Mean=" + std::to_string(profile.onTime) + "]";
            std::string offTimeStr = "ns3::ExponentialRandomVariable[Mean=" + std::to_string(profile.offTime) + "]";
            
            trafficApp.SetAttribute("OnTime", StringValue(onTimeStr));
            trafficApp.SetAttribute("OffTime", StringValue(offTimeStr));
            trafficApp.SetAttribute("StartTime", TimeValue(Seconds(0.1)));
            
            ApplicationContainer sourceApps = trafficApp.Install(remoteHostContainer.Get(0));
            
            sourceApps.Start(Seconds(2.0));
            sourceApps.Stop(Seconds(simTime));

            NS_LOG_INFO("UE[" << nodeIdx << "] app installed and scheduled: start=2s, stop=" << simTime << "s");
        }
    }

    // Simple UDP echo connectivity test
    NS_LOG_INFO("Installing UDP Echo test...");
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(remoteHostContainer.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));
    
    UdpEchoClientHelper echoClient(remoteHostAddr, echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(6.1));
    clientApps.Stop(Seconds(7.0));
    NS_LOG_INFO("Echo test: UE[0] will send 1 packet to remoteHost:9 at t=6.1s");

    // FlowMonitor statistics
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Run
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Post-simulation analysis
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());

    double totalThroughputEmbB = 0.0, totalDelayEmbB = 0.0;
    uint32_t embbFlows = 0;
    double totalThroughputUrllc = 0.0, totalDelayUrllc = 0.0;
    uint32_t urllcFlows = 0;
    uint32_t ignoredFlows = 0; // Infrastructure flows (GTP/backhaul)

    Ipv4Address ueNetworkAddress("7.0.0.0");
    Ipv4Mask ueNetworkMask("255.0.0.0");

    std::map<FlowId, FlowMonitor::FlowStats> statsMap = monitor->GetFlowStats();
    
    std::cout << "\n=== DEBUG: Total number of captured flows ===" << std::endl;
    std::cout << "Total flows: " << statsMap.size() << std::endl;
    
    uint64_t totalTxPackets = 0, totalRxPackets = 0;
    for (const auto& it : statsMap)
    {
        FlowId flowId = it.first;
        const FlowMonitor::FlowStats& stats = it.second;
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        std::cout << "  Flow " << flowId << ": " << t.sourceAddress << ":" << t.sourcePort 
                  << " -> " << t.destinationAddress << ":" << t.destinationPort 
                  << " (Tx: " << stats.txPackets << ", Rx: " << stats.rxPackets << ")" << std::endl;
        totalTxPackets += stats.txPackets;
        totalRxPackets += stats.rxPackets;
    }
    std::cout << "Total Tx: " << totalTxPackets << " | Total Rx: " << totalRxPackets << std::endl;
    
    std::cout << "\n=== DIAGNOSTICS ===" << std::endl;
    std::cout << "If Total Tx = 0: OnOff apps did NOT send any packets" << std::endl;
    std::cout << "If Total Tx > 0 but Total Rx = 0: All packets were lost in the network" << std::endl;
    std::cout << "If Total Rx > 0: Flows are successfully traversing the network" << std::endl;
    
    std::cout << "\n=== FLOW DETAILS ===" << std::endl;

    for (const auto& it : statsMap)
    {
        FlowId flowId = it.first;
        const FlowMonitor::FlowStats& stats = it.second;
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);

        // Check whether the flow is associated with a UE (uplink or downlink)
        bool ueAsSource = ueNetworkMask.IsMatch(t.sourceAddress, ueNetworkAddress);
        bool ueAsDest = ueNetworkMask.IsMatch(t.destinationAddress, ueNetworkAddress);

        if (ueAsSource || ueAsDest)
        {
            double throughput = 0.0;
            double delay = 0.0;
            double lossRatio = 100.0;

            if (stats.rxPackets > 0)
            {
                double txDuration = stats.timeLastRxPacket.GetSeconds() - stats.timeFirstTxPacket.GetSeconds();
                if (txDuration <= 0.0) txDuration = 1e-9;
                
                throughput = (stats.rxBytes * 8.0) / txDuration / 1e6; // Mbps
                delay = (stats.delaySum.GetSeconds() / stats.rxPackets) * 1e3; // ms
                lossRatio = (stats.txPackets > 0) ? ((double)(stats.txPackets - stats.rxPackets) / stats.txPackets) * 100.0 : 0.0;
            }

            // Identify echo-test flow (port 9 at either end)
            bool isEchoFlow = (t.sourcePort == echoPort || t.destinationPort == echoPort);

            // Find the UE IP in this flow
            Ipv4Address ueAddr;
            if (ueAsSource && !ueAsDest)
            {
                ueAddr = t.sourceAddress;
            }
            else if (ueAsDest && !ueAsSource)
            {
                ueAddr = t.destinationAddress;
            }
            else
            {
                // Rare case: both endpoints in UE network (UE-UE)
                ueAddr = t.sourceAddress;
            }

            int ueIndex = -1;
            auto itIdx = ueIpToIndex.find(ueAddr);
            if (itIdx != ueIpToIndex.end())
            {
                ueIndex = static_cast<int>(itIdx->second);
            }

            int sliceId = -1;
            std::string trafficType = "UNKNOWN";
            if (ueIndex >= 0 && ueIndex < static_cast<int>(ueSliceId.size()))
            {
                sliceId = ueSliceId[ueIndex];
            }
            if (ueIndex >= 0 && ueIndex < static_cast<int>(ueSliceTrafficType.size()) &&
                !ueSliceTrafficType[ueIndex].empty())
            {
                trafficType = ueSliceTrafficType[ueIndex];
            }

            if (isEchoFlow)
            {
                // Connectivity test flow: exclude from eMBB/URLLC statistics
                std::cout << "Flow " << flowId << " (ECHO TEST): UE " << ueAddr
                          << " | T-put: " << std::fixed << std::setprecision(2) << throughput << " Mbps"
                          << " | Delay: " << delay << " ms"
                          << " | Loss: " << lossRatio << " %" << std::endl;
                continue;
            }

            // Update aggregate statistics according to slice traffic type
            if (trafficType == "eMBB" || trafficType == "EMBB" || trafficType == "embb")
            {
                totalThroughputEmbB += throughput;
                totalDelayEmbB += delay;
                embbFlows++;
            }
            else if (trafficType == "URLLC" || trafficType == "urllc")
            {
                totalThroughputUrllc += throughput;
                totalDelayUrllc += delay;
                urllcFlows++;
            }

            std::cout << "Flow " << flowId << " (" << trafficType
                      << ", slice " << ((sliceId >= 0) ? std::to_string(sliceId) : std::string("N/A"))
                      << "): " << t.sourceAddress << ":" << t.sourcePort
                      << " -> " << t.destinationAddress << ":" << t.destinationPort
                      << " | T-put: " << std::fixed << std::setprecision(2) << throughput << " Mbps" 
                      << " | Delay: " << delay << " ms" 
                      << " | Loss: " << lossRatio << " %" << std::endl;
        }
        else
        {
            // Infrastructure flow (e.g., GTP/backhaul), ignored in UE statistics
            ignoredFlows++;
            std::cout << "  (Ignored infrastructure flow: " << t.sourceAddress << ":" << t.sourcePort 
                      << " -> " << t.destinationAddress << ":" << t.destinationPort 
                      << ") Tx: " << stats.txPackets << " Rx: " << stats.rxPackets << std::endl;
        }
    }

    std::cout << "\n=== SUMMARY ===" << std::endl;
    std::cout << "Ignored infrastructure flows (GTP/Backhaul): " << ignoredFlows << std::endl;

    if (embbFlows > 0)
    {
        std::cout << "Average eMBB (" << embbFlows << " flows) - Throughput: " 
              << (totalThroughputEmbB / embbFlows) << " Mbps; Delay: "
              << (totalDelayEmbB / embbFlows) << " ms" << std::endl;
    }
    else 
    {
        std::cout << "No eMBB flow detected (check if app start time > RRC connection time)." << std::endl;
    }

    if (urllcFlows > 0)
    {
        std::cout << "Average URLLC (" << urllcFlows << " flows) - Throughput: " 
              << (totalThroughputUrllc / urllcFlows) << " Mbps; Delay: "
              << (totalDelayUrllc / urllcFlows) << " ms" << std::endl;
    }
    else
    {
         std::cout << "No URLLC flow detected." << std::endl;
    }

    Simulator::Destroy();
    return 0;
}
