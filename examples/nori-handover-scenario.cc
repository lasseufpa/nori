// Copyright (c) 2026 LASSE/UFPA
//
// SPDX-License-Identifier: GPL-2.0-only

/**
 * @file nori-handover-scenario.cc
 * @brief Three gNBs in a line and two UEs moving back and forth, with X2 handover.
 *
 * Topology (top view, gNBs 10 m high, UEs 1.5 m high):
 *
 *     gNB1 (cell 1)        gNB2 (cell 2)        gNB3 (cell 3)
 *     x = 0                x = d                x = 2d
 *     UE 0: x = 0 -> 2d -> 0 -> ...   (y = +10 m)
 *     UE 1: x = 2d -> 0 -> 2d -> ...  (y = -10 m)
 *
 * The UEs cross the cell borders in opposite directions, so each one hands over
 * 1 -> 2 -> 3 -> 2 -> 1 ... (A3 event on RSRP, X2 handover). Every gNB has its
 * own E2 termination (E2 node gnb_<plmn>_0000000<cellId>), so a KPM xApp sees the
 * UE (identified by its IMSI in the UE-level report) move from one E2 node to another.
 *
 * Ground truth: each completed handover is printed as
 *   [HO] t=<s> IMSI=<imsi> -> cell <cellId> (RNTI <rnti>)
 *
 * Example:
 *   ./ns3 run "nori-handover-scenario --simTime=80 --ipE2TermRic=10.10.20.142 --ipE2TermRicPort=32222"
 */

#include "ns3/E2-term-helper.h"
#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nori-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-helper.h"

#include <iomanip>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NoriHandoverScenario");

namespace
{

void
NotifyHandoverEndOk(std::string /*context*/, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    std::cout << "[HO] t=" << std::fixed << std::setprecision(3)
              << Simulator::Now().GetSeconds() << "s IMSI=" << imsi << " -> cell " << cellId
              << " (RNTI " << rnti << ")" << std::endl;
}

void
NotifyConnectionEstablished(std::string /*context*/,
                            uint64_t imsi,
                            uint16_t cellId,
                            uint16_t rnti)
{
    std::cout << "[CONN] t=" << std::fixed << std::setprecision(3)
              << Simulator::Now().GetSeconds() << "s IMSI=" << imsi << " connected to cell "
              << cellId << " (RNTI " << rnti << ")" << std::endl;
}

/**
 * Install a back-and-forth path between xStart and xEnd at constant speed,
 * long enough to cover the simulation.
 */
void
InstallShuttleMobility(Ptr<Node> ue,
                       double xStart,
                       double xEnd,
                       double y,
                       double speed,
                       double duration)
{
    auto waypoints = CreateObject<WaypointMobilityModel>();
    const double legTime = std::abs(xEnd - xStart) / speed;
    double t = 0.0;
    bool forward = true;
    waypoints->AddWaypoint(Waypoint(Seconds(0), Vector(xStart, y, 1.5)));
    while (t < duration)
    {
        t += legTime;
        waypoints->AddWaypoint(Waypoint(Seconds(t), Vector(forward ? xEnd : xStart, y, 1.5)));
        forward = !forward;
    }
    ue->AggregateObject(waypoints);
}

} // namespace

int
main(int argc, char* argv[])
{
    double simTime = 80.0;           // s (simulated)
    double gnbDistance = 200.0;      // m between neighbor gNBs
    double ueSpeed = 20.0;           // m/s
    double centralFrequency = 3.5e9; // Hz
    double bandwidth = 20e6;         // Hz
    uint16_t numerology = 1;
    double gnbTxPower = 30.0;        // dBm
    double hysteresis = 3.0;         // dB (A3)
    uint32_t timeToTriggerMs = 256;
    std::string trafficRate = "5Mb/s";
    std::string ipE2TermRic = "10.244.0.30";
    uint16_t ipE2TermRicPort = 36422;
    bool enableE2 = true;
    double e2StartStagger = 1.0; // s between the E2 Setup of consecutive gNBs
    bool enableRealtime = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Simulated time in seconds", simTime);
    cmd.AddValue("gnbDistance", "Distance between neighbor gNBs (m)", gnbDistance);
    cmd.AddValue("ueSpeed", "UE speed (m/s)", ueSpeed);
    cmd.AddValue("trafficRate", "Downlink UDP rate per UE", trafficRate);
    cmd.AddValue("hysteresis", "A3 hysteresis (dB)", hysteresis);
    cmd.AddValue("timeToTrigger", "A3 time to trigger (ms)", timeToTriggerMs);
    cmd.AddValue("ipE2TermRic", "IP address of the Near-RT RIC e2term", ipE2TermRic);
    cmd.AddValue("ipE2TermRicPort", "SCTP port of the Near-RT RIC e2term", ipE2TermRicPort);
    cmd.AddValue("enableE2", "Install the E2 termination on the gNBs", enableE2);
    cmd.AddValue("e2StartStagger",
                 "Delay (s) between the E2 Setup of consecutive gNBs (0: all together)",
                 e2StartStagger);
    cmd.AddValue("enableRealtime", "Use the realtime simulator (needed with a real RIC)",
                 enableRealtime);
    cmd.Parse(argc, argv);

    if (enableRealtime)
    {
        GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    }
    Config::SetDefault("ns3::NrGnbPhy::TxPower", DoubleValue(gnbTxPower));

    const uint16_t gnbNum = 3;
    const uint16_t ueNum = 2;

    NodeContainer gnbNodes;
    gnbNodes.Create(gnbNum);
    NodeContainer ueNodes;
    ueNodes.Create(ueNum);

    // gNBs in a line
    Ptr<ListPositionAllocator> gnbPositions = CreateObject<ListPositionAllocator>();
    for (uint16_t i = 0; i < gnbNum; ++i)
    {
        gnbPositions->Add(Vector(i * gnbDistance, 0.0, 10.0));
    }
    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMobility.SetPositionAllocator(gnbPositions);
    gnbMobility.Install(gnbNodes);

    // UEs shuttling along the line in opposite directions
    const double xMax = (gnbNum - 1) * gnbDistance;
    InstallShuttleMobility(ueNodes.Get(0), 0.0, xMax, 10.0, ueSpeed, simTime + 1.0);
    InstallShuttleMobility(ueNodes.Get(1), xMax, 0.0, -10.0, ueSpeed, simTime + 1.0);

    // NR, EPC and handover
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> beamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetEpcHelper(epcHelper);
    nrHelper->SetBeamformingHelper(beamformingHelper);
    nrHelper->SetAttribute("UseIdealRrc", BooleanValue(true));
    beamformingHelper->SetAttribute("BeamformingMethod",
                                    TypeIdValue(DirectPathBeamforming::GetTypeId()));

    nrHelper->SetHandoverAlgorithmType("ns3::NrA3RsrpHandoverAlgorithm");
    nrHelper->SetHandoverAlgorithmAttribute("Hysteresis", DoubleValue(hysteresis));
    nrHelper->SetHandoverAlgorithmAttribute("TimeToTrigger",
                                            TimeValue(MilliSeconds(timeToTriggerMs)));

    // Single-element antennas keep the simulation light
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(1));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(1));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(1));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(1));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<IsotropicAntennaModel>()));
    nrHelper->SetGnbPhyAttribute("Numerology", UintegerValue(numerology));

    // Deterministic channel: UMi LOS without shadowing, so handovers follow the geometry
    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("UMi", "LOS", "ThreeGpp");
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));

    CcBwpCreator ccBwpCreator;
    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency, bandwidth, 1);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);
    channelHelper->AssignChannelsToBands({band});
    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});

    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes, allBwps);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, allBwps);

    // Internet: remote host -> PGW -> UEs
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);
    internet.Install(ueNodes);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    ipv4h.Assign(internetDevices);

    Ipv4StaticRoutingHelper routingHelper;
    routingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>())
        ->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(ueDevs);
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        routingHelper.GetStaticRouting(ueNodes.Get(i)->GetObject<Ipv4>())
            ->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // UE 0 starts next to gNB1 and UE 1 next to gNB3
    nrHelper->AttachToGnb(ueDevs.Get(0), gnbDevs.Get(0));
    nrHelper->AttachToGnb(ueDevs.Get(1), gnbDevs.Get(gnbNum - 1));

    // X2 between all gNBs (required for handover)
    nrHelper->AddX2Interface(gnbNodes);

    Config::Connect("/NodeList/*/DeviceList/*/NrUeRrc/HandoverEndOk",
                    MakeCallback(&NotifyHandoverEndOk));
    Config::Connect("/NodeList/*/DeviceList/*/NrUeRrc/ConnectionEstablished",
                    MakeCallback(&NotifyConnectionEstablished));

    // One E2 termination per gNB (the helper must live until the end of the run)
    Ptr<E2TermHelper> e2Helper;
    if (enableE2)
    {
        e2Helper = CreateObject<E2TermHelper>();
        e2Helper->SetAttribute("E2TermIp", StringValue(ipE2TermRic));
        e2Helper->SetAttribute("E2Port", UintegerValue(ipE2TermRicPort));
        e2Helper->SetAttribute("StartStagger", TimeValue(Seconds(e2StartStagger)));
        e2Helper->InstallE2Term(gnbDevs);
    }

    // Downlink UDP to each UE
    const uint16_t dlPort = 1234;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        PacketSinkHelper sink("ns3::UdpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), dlPort));
        sink.Install(ueNodes.Get(i)).Start(Seconds(0.5));

        OnOffHelper client("ns3::UdpSocketFactory",
                           InetSocketAddress(ueIpIfaces.GetAddress(i), dlPort));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        client.SetAttribute("DataRate", DataRateValue(DataRate(trafficRate)));
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
        client.Install(remoteHost).Start(Seconds(1.0));
    }

    std::cout << "Handover scenario: " << gnbNum << " gNBs every " << gnbDistance << " m, "
              << ueNum << " UEs at " << ueSpeed << " m/s, simTime " << simTime << " s"
              << std::endl;

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    std::cout << "Simulation finished successfully." << std::endl;
    return 0;
}
