#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NoriHandOverExample");

int main(int argc, char *argv[])
{
    uint16_t gNbNum = 2;
    uint16_t ueNumPerCell = 1;
    double simTime = 3.0;
    double interSiteDistance = 500.0;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Configuração do log
    LogLevel logLevel = (LogLevel)(LOG_PREFIX_TIME | LOG_PREFIX_NODE | LOG_LEVEL_INFO);
    LogComponentEnable("NrHandOverExample", logLevel);

    // Criando nós
    NodeContainer gNbNodes;
    gNbNodes.Create(gNbNum);
    NodeContainer ueNodes;
    ueNodes.Create(ueNumPerCell);

    // Configurando a mobilidade
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));     // Posição da gNB1
    positionAlloc->Add(Vector(interSiteDistance, 0.0, 0.0)); // Posição da gNB2
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gNbNodes);

    // Configurando mobilidade da UE
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(ueNodes);
    ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20.0, 0.0, 0.0));

    // Instalando o NR
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetNrMacSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerOfdmTdma"));

    // Instalando dispositivos
    NetDeviceContainer gNbNetDev = nrHelper->InstallGnbDevice(gNbNodes);
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueNodes);

    // Configurando IP
    InternetStackHelper internet;
    internet.Install(ueNodes);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("7.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueNetDev);

    // Ativando as aplicações
    uint16_t dlPort = 1234;
    uint16_t ulPort = 2000;
    ApplicationContainer clientApps, serverApps;

    UdpClientHelper dlClient(ueIpIface.GetAddress(0), dlPort);
    dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));

    UdpServerHelper dlServer(dlPort);
    serverApps.Add(dlServer.Install(ueNodes.Get(0)));
    clientApps.Add(dlClient.Install(gNbNodes.Get(0)));

    UdpClientHelper ulClient(gNbNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), ulPort);
    ulClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    ulClient.SetAttribute("MaxPackets", UintegerValue(1000000));

    UdpServerHelper ulServer(ulPort);
    serverApps.Add(ulServer.Install(gNbNodes.Get(0)));
    clientApps.Add(ulClient.Install(ueNodes.Get(0)));

    serverApps.Start(Seconds(0.01));
    clientApps.Start(Seconds(0.01));

    // Ativando rastreamento
    nrHelper->EnableTraces();

    // Executando simulação
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
