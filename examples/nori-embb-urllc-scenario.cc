// nori-embb-urllc-com-epc.cc
// Versão com EPC/PGW — tráfego IP fim-a-fim UE <-> remoteHost
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

    // Estrutura para armazenar parâmetros de tráfego por tipo
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
        bandwidth = bwMHz * 1e6; // converter de MHz para Hz
        centralFrequency = configJson["NR"].value("centralFrequency", centralFrequency);
        txPower = configJson["NR"].value("txPower", txPower);
        ueTxPower = configJson["NR"].value("ueTxPower", ueTxPower);

        std::vector<uint32_t> jsonSlices = configJson["slices"]["UesPerSlice"];

        for (uint32_t& item : jsonSlices) {
            NS_LOG_INFO("Número de UEs por slice: " << item);
            uesPerSlice.push_back(item);
        }

        // Ler tipos de tráfego por slice
        if (configJson["slices"].contains("trafficTypes")) {
            trafficTypes = configJson["slices"]["trafficTypes"].get<std::vector<std::string>>();
            for (size_t i = 0; i < trafficTypes.size(); ++i) {
                NS_LOG_INFO("Slice " << i << " tipo de tráfego: " << trafficTypes[i]);
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
        NS_LOG_INFO("Total de UEs atualizado via configuração de Slices: " << ueNum);

    }else {
        NS_LOG_ERROR("Não foi possível abrir o arquivo de configuração: ");
        NS_LOG_INFO("Usando parâmetros padrão.");
    }

    // Vetores auxiliares para mapear cada UE ao slice e tipo de tráfego
    std::vector<int> ueSliceId(ueNum, -1);
    std::vector<std::string> ueSliceTrafficType(ueNum, "");

    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    
    // Parâmetro para habilitar/desabilitar RAN Slicing com RL
    bool enableRanSlicing = true;
    
    CommandLine cmd;
    //cmd.AddValue("ueNum", "Número de UEs", ueNum);
    //cmd.AddValue("simTime", "Tempo de simulação (s)", simTime);
    cmd.AddValue("enableRanSlicing", "Enable RAN Slicing with RL scheduler", enableRanSlicing);
    cmd.AddValue("ipE2TermRic", "Ip address of the E2 termination", ipE2TermRic);
    cmd.Parse(argc, argv);

    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetAttribute("UseIdealRrc", BooleanValue(true));
    nrHelper->SetGnbPhyAttribute("TbDecodeLatency", TimeValue(MicroSeconds(1.0)));
    nrHelper->SetUePhyAttribute("TbDecodeLatency", TimeValue(MicroSeconds(1.0)));
    // Configurar numerologia e potências a partir do config.json
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

    // remoteHost (servidor) será conectado ao PGW via P2P link
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);

    // --- Mobilidade ---
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

    // Logar posições iniciais das gNBs
    NS_LOG_INFO("*** Posições das gNBs ***");
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
    // Reduzir a área de mobilidade para manter UEs mais próximos do gNB
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

    // Logar posições iniciais das UEs
    NS_LOG_INFO("*** Posições iniciais das UEs ***");
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mob = ueNodes.Get(i)->GetObject<MobilityModel>();
        if (mob)
        {
            Vector pos = mob->GetPosition();
            NS_LOG_INFO("UE[" << i << "] pos = (" << pos.x << ", " << pos.y << ", " << pos.z << ")");
        }
    }

    // --- Antenas & canal ---
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

    //  Pilha IP: instalar em remoteHost  e UEs 
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);
    internet.Install(ueNodes);

    NetDeviceContainer gNbDevs = nrHelper->InstallGnbDevice(gNbNodes, allBwps);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, allBwps);

    // habilitar suporte E2 nos gNBs
    auto e2 = CreateObject<E2TermHelper>();
    e2->SetAttribute("E2TermIp", StringValue(ipE2TermRic));
    e2->InstallE2Term(gNbDevs);

    nrHelper->AttachToClosestGnb(ueDevs, gNbDevs);

    // mapeamento de Slices
    if (enableRanSlicing && uesPerSlice.size() > 0)
    {
        NS_LOG_INFO("Configurando mapeamento de slices no scheduler RL...");
        
        // mapeamento de RNTI por slice
        std::vector<std::vector<uint32_t>> sliceUeRntiMap(uesPerSlice.size());
        uint32_t currentUeIdx = 0;
        
        for (size_t sliceId = 0; sliceId < uesPerSlice.size(); ++sliceId) 
        {
            int numUesInSlice = uesPerSlice[sliceId];
            for (int k = 0; k < numUesInSlice; ++k) 
            {
                if (currentUeIdx < ueDevs.GetN()) {
                    // RNTI começa em 1 e é sequencial
                    uint32_t rnti = currentUeIdx + 1;
                    sliceUeRntiMap[sliceId].push_back(rnti);
                    NS_LOG_INFO("Slice " << sliceId << " -> UE RNTI " << rnti);
                    currentUeIdx++;
                }
            }
        }
        
        // Configurar mapeamento em cada gNB (assumindo um BWP por gNB -> índice 0)
        for (uint32_t gNbIdx = 0; gNbIdx < gNbDevs.GetN(); ++gNbIdx)
        {
            Ptr<NrGnbNetDevice> gnbNetDev = gNbDevs.Get(gNbIdx)->GetObject<NrGnbNetDevice>();
            if (!gnbNetDev)
            {
                continue;
            }

            Ptr<NrMacScheduler> scheduler = gnbNetDev->GetScheduler(0);
            Ptr<NrRLMacSchedulerOfdma> rlScheduler = DynamicCast<NrRLMacSchedulerOfdma>(scheduler);

            if (rlScheduler)
            {
                rlScheduler->SetSliceUeMapping(uesPerSlice.size(), sliceUeRntiMap);
                NS_LOG_INFO("Mapeamento de slices configurado no gNB " << gNbIdx);
            }
            else
            {
                NS_LOG_WARN("Scheduler do gNB " << gNbIdx << " não é NrRLMacSchedulerOfdma");
            }
        }
    }
    else
    {
        NS_LOG_INFO("RAN Slicing desabilitado ou sem slices configurados");
    }

    // --- Conecta remoteHost (servidor) ao PGW via link P2P ---
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

    // O epcHelper atribui endereços IPv4 aos UEs (pool interna)
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(ueDevs);
    // Em muitas versões: Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(ueDevs);

    // Mapa IP->índice de UE para classificação posterior dos fluxos
    std::map<Ipv4Address, uint32_t> ueIpToIndex;

    NS_LOG_INFO("*** Endereços atribuídos (EPC) ***");
    NS_LOG_INFO("remoteHost (server): " << remoteHostAddr);
    NS_LOG_INFO("PGW: " << pgwAddr);
    for (uint32_t i = 0; i < ueIpIfaces.GetN(); ++i)
    {
        Ipv4Address addr = ueIpIfaces.GetAddress(i);
        ueIpToIndex[addr] = i;
        NS_LOG_INFO("UE[" << i << "] IP (via EPC): " << addr);
    }

    // --- Rotas: no remoteHost precisamos de rota para rede UE via PGW ---
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4> remoteIpv4 = remoteHostContainer.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> remoteStatic = ipv4RoutingHelper.GetStaticRouting(remoteIpv4);
    // rota para rede UE (o EPC usa rede 7.0.0.0/8 em alguns exemplos, mas epcHelper tem pool - usaremos o pool retornado acima)
    // O epcHelper normalmente usa 7.0.0.0/8 como rede para UEs; se AssignUeIpv4Address retornou endereços, inferimos a rede:
    // Para simplicidade, adicionar rota para o endereço da UE sob máscara /24 via interface 1 (p2p)
    // Ajuste se sua versão do epcHelper gerar outra rede
    //remoteStatic->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    remoteStatic->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), Ipv4Address("1.0.0.1"), 1);

    // Configurar rota default nos UEs para o gateway do EPC (via EPC)
    // Padrão alinhado com exemplos LTE/NR (SetDefaultRoute para GetUeDefaultGatewayAddress)
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4> ueIpv4 = ueNodes.Get(i)->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> ueStatic = ipv4RoutingHelper.GetStaticRouting(ueIpv4);
        ueStatic->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
        NS_LOG_INFO("UE[" << i << "] default route: GW=" << epcHelper->GetUeDefaultGatewayAddress()
                         << " via interface 1");
    }

    // Aplicações: instalar sinks nos UEs e OnOff no remoteHost (downlink)
    uint16_t portBase = 8080;

    // --- Ativar bearers dedicados para tráfego de downlink (remoteHost -> UEs) ---
    // Isso garante que o EPC crie mapeamentos S1-U corretos para as portas UDP usadas
    // nas aplicações OnOff (portas 8080 + índice do UE).
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        Ptr<NetDevice> ueDevice = ueDevs.Get(i);

        // Usar um bearer não-GBR de baixa latência (padrão usado em outros exemplos Nori)
        NrEpsBearer bearer(NrEpsBearer::NGBR_LOW_LAT_EMBB);

        Ptr<NrEpcTft> tft = Create<NrEpcTft>();

        // Filtro de downlink: porta local = porta UDP em que o UE escuta (sink)
        uint16_t uePort = portBase + i;
        NrEpcTft::PacketFilter dlpf;
        dlpf.localPortStart = uePort;
        dlpf.localPortEnd = uePort;
        tft->Add(dlpf);

        nrHelper->ActivateDedicatedEpsBearer(ueDevice, bearer, tft);

        NS_LOG_INFO("Bearer dedicado DL ativado para UE[" << i << "] porta " << uePort);
    }

    // instalar sink em cada UE
    // Sinks precisam iniciar ANTES das apps (em 0s) para estar prontas
    for (uint32_t i = 0; i < ueNum; ++i)
    {
        uint16_t port = portBase + i;
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sink.Install(ueNodes.Get(i));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(simTime));
        NS_LOG_INFO("Sink instalada na porta " << port << " no UE[" << i << "]");
    }

    // (lógica antiga de OnOff por metade dos UEs removida; agora o tráfego é definido por slices e perfis em config.json)

    // LÓGICA DE APLICAÇÃO PARA N SLICES
    uint32_t currentUeIndex = 0;

    for (size_t sliceId = 0; sliceId < uesPerSlice.size(); ++sliceId) 
    {
        int countUes = uesPerSlice[sliceId];
        
        // Obter tipo de tráfego do arquivo de configuração
        // Se não estiver configurado, usar valor padrão
        std::string trafficType = (sliceId < trafficTypes.size()) 
            ? trafficTypes[sliceId] 
            : ((sliceId == 0) ? "eMBB" : "URLLC");

        NS_LOG_INFO("Slice " << sliceId << " configurado com tráfego tipo: " << trafficType);

        for (int k = 0; k < countUes; ++k) 
        {
            // Proteção para não exceder o número de nós criados
            if (currentUeIndex >= ueNodes.GetN()) break;

            uint32_t nodeIdx = currentUeIndex++;
            uint16_t port = portBase + nodeIdx;

            // Verificar se o tipo de tráfego tem perfil configurado
            std::string resolvedTrafficType = trafficType;
            if (trafficProfiles.find(resolvedTrafficType) == trafficProfiles.end()) {
                NS_LOG_WARN("Tipo de tráfego '" << resolvedTrafficType << "' não configurado. Usando padrão eMBB.");
                resolvedTrafficType = "eMBB";
            }

            TrafficProfile profile = trafficProfiles[resolvedTrafficType];

            Ipv4Address ueAddr = ueIpIfaces.GetAddress(nodeIdx);

            // Registrar mapeamento UE -> slice e tipo de tráfego para uso na análise do FlowMonitor
            ueSliceId[nodeIdx] = static_cast<int>(sliceId);
            ueSliceTrafficType[nodeIdx] = resolvedTrafficType;

            // DEBUG: tráfego downlink: remoteHost -> UE
            NS_LOG_INFO("DEBUG DL para UE[" << nodeIdx << "] (" << ueAddr
                        << "): porta " << port << " tipo " << resolvedTrafficType);

            OnOffHelper trafficApp("ns3::UdpSocketFactory", InetSocketAddress(ueAddr, port));
            trafficApp.SetAttribute("DataRate", DataRateValue(DataRate(std::to_string((int)profile.dataRate) + "Mbps")));
            trafficApp.SetAttribute("PacketSize", UintegerValue(profile.packetSize));
            
            std::string onTimeStr = "ns3::ExponentialRandomVariable[Mean=" + std::to_string(profile.onTime) + "]";
            std::string offTimeStr = "ns3::ExponentialRandomVariable[Mean=" + std::to_string(profile.offTime) + "]";
            
            trafficApp.SetAttribute("OnTime", StringValue(onTimeStr));
            trafficApp.SetAttribute("OffTime", StringValue(offTimeStr));
            // Tentar ativar a aplicação assim que possível
            trafficApp.SetAttribute("StartTime", TimeValue(Seconds(0.1)));
            
            ApplicationContainer sourceApps = trafficApp.Install(remoteHostContainer.Get(0));
            
            // Iniciar em 6s para garantir que RRC seja completado
            sourceApps.Start(Seconds(6.0));
            sourceApps.Stop(Seconds(simTime));

            NS_LOG_INFO("UE[" << nodeIdx << "] app instalada e agendada: start=6s, stop=" << simTime << "s");
        }
    }

    // Teste de conectividade: enviar um UDP Echo para verificar
    NS_LOG_INFO("Instalando teste UDP Echo...");
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
    NS_LOG_INFO("Teste Echo: UE[0] enviará 1 pacote para remoteHost:9 em t=6.1s");

    // --- FlowMonitor para estatísticas ---
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Run
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Análise
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());

    double totalThroughputEmbB = 0.0, totalDelayEmbB = 0.0;
    uint32_t embbFlows = 0;
    double totalThroughputUrllc = 0.0, totalDelayUrllc = 0.0;
    uint32_t urllcFlows = 0;
    uint32_t ignoredFlows = 0; // Contador para fluxos de infraestrutura

    Ipv4Address ueNetworkAddress("7.0.0.0");
    Ipv4Mask ueNetworkMask("255.0.0.0");

    std::map<FlowId, FlowMonitor::FlowStats> statsMap = monitor->GetFlowStats();
    
    std::cout << "\n=== DEBUG: Total de fluxos capturados ===" << std::endl;
    std::cout << "Total de fluxos: " << statsMap.size() << std::endl;
    
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
    
    // Diagnóstico: verificar status das aplicações
    std::cout << "\n=== DIAGNÓSTICO ===" << std::endl;
    std::cout << "Se Total Tx = 0: Apps OnOff NÃO enviaram pacotes" << std::endl;
    std::cout << "Se Total Tx > 0 mas Total Rx = 0: Pacotes perdidos na rede" << std::endl;
    std::cout << "Se Total Rx > 0: Fluxos estão passando pela rede" << std::endl;
    
    std::cout << "\n=== DETALHAMENTO DOS FLUXOS ===" << std::endl;

    for (const auto& it : statsMap)
    {
        FlowId flowId = it.first;
        const FlowMonitor::FlowStats& stats = it.second;
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);

        // Verifica se o fluxo está associado a um UE (uplink ou downlink)
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

            // Identificar se é fluxo do teste de Echo (porta 9 em qualquer extremo)
            bool isEchoFlow = (t.sourcePort == echoPort || t.destinationPort == echoPort);

            // Descobrir qual é o IP do UE neste fluxo
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
                // Caso raro: ambos endpoints em rede de UEs (UE-UE)
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
                // Fluxo de teste de conectividade: não entra nas estatísticas eMBB/URLLC
                std::cout << "Flow " << flowId << " (ECHO TEST): UE " << ueAddr
                          << " | T-put: " << std::fixed << std::setprecision(2) << throughput << " Mbps"
                          << " | Delay: " << delay << " ms"
                          << " | Loss: " << lossRatio << " %" << std::endl;
                continue;
            }

            // Atualizar estatísticas agregadas conforme o tipo de tráfego do slice
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
            // Debug: Mostrar que ignoramos um fluxo de infraestrutura (ex: tunel GTP)
            ignoredFlows++;
            // Descomente a linha abaixo se quiser ver os IPs dos túneis ignorados
            std::cout << "  (Ignorado fluxo infra: " << t.sourceAddress << ":" << t.sourcePort 
                      << " -> " << t.destinationAddress << ":" << t.destinationPort 
                      << ") Tx: " << stats.txPackets << " Rx: " << stats.rxPackets << std::endl;
        }
    }

    std::cout << "\n=== RESUMO ===" << std::endl;
    std::cout << "Fluxos de Infraestrutura ignorados (GTP/Backhaul): " << ignoredFlows << std::endl;

    if (embbFlows > 0)
    {
        std::cout << "Média eMBB (" << embbFlows << " fluxos) - Throughput: " 
                  << (totalThroughputEmbB / embbFlows) << " Mbps; Delay: "
                  << (totalDelayEmbB / embbFlows) << " ms" << std::endl;
    }
    else 
    {
        std::cout << "Nenhum fluxo eMBB detectado (verifique se o tempo de inicio da App > tempo de conexão RRC)." << std::endl;
    }

    if (urllcFlows > 0)
    {
        std::cout << "Média URLLC (" << urllcFlows << " fluxos) - Throughput: " 
                  << (totalThroughputUrllc / urllcFlows) << " Mbps; Delay: "
                  << (totalDelayUrllc / urllcFlows) << " ms" << std::endl;
    }
    else
    {
         std::cout << "Nenhum fluxo URLLC detectado." << std::endl;
    }

    Simulator::Destroy();
    return 0;
}
