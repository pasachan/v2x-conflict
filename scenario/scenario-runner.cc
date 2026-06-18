#include "../config/fiveg-config.h"
#include "../metrics/sla-monitor.h"
#include "scenario.h"

#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-module.h"

#include <map>

using namespace ns3;

/*
 * Topology:
 *
 *                       gNB (0, 0, 25 m)
 *                        |
 *   ----#----#----#----#----#----#----#----#----  road (y = 10 m)
 *      vehicles evenly spaced over roadLength, driving in +x at `speed`
 *
 * Each vehicle runs a UDP sink; the remote host behind the EPC sends a
 * downlink CBR flow per vehicle according to the vehicle's intent. A
 * dedicated EPS bearer with the intent's 5QI carries each flow. Under QoS
 * protection, remote driving and sensor sharing get GBR bearers and the
 * OFDMA QoS scheduler.
 */
bool
RunScenario(const ScenarioParams& p)
{
    std::vector<Intent> assignment = (p.scenario == "single")
                                         ? AssignSingleIntent(p.numVehicles)
                                         : AssignMultiIntent(p.numVehicles);

    /*
     * RLC buffer of 1 MB per bearer: large enough that congestion shows up
     * as queueing delay (SLA latency breach), small enough that the cell
     * never enters the pathological overload regime where RLC UM sequence
     * numbers wrap and reassembly aborts the simulation.
     */
    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(1000000));

    RanConfig cfg = Configure5G(p.enableMimo);

    /*
     * QoS-protected scheduling. Swap the proportional-fair scheduler for
     * the OFDMA QoS scheduler, which honours per-bearer GBR reservations
     * and 5QI priority. Remote driving and sensor sharing are given GBR
     * bearers below; other intents stay non-GBR and share the leftover
     * capacity. Must be set before InstallGnbDevice.
     */
    if (p.qosProtected)
    {
        cfg.nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerOfdmaQos"));
    }

    std::cout << "Running 5G " << p.scenario << "-intent"
              << (p.qosProtected ? " (QoS-protected)" : "") << " scenario with "
              << p.numVehicles << " vehicles (" << p.numVehicles / (p.roadLength / 1000.0)
              << " vehicles/km)" << std::endl;

    NodeContainer gnbNodes;
    NodeContainer ueNodes;
    gnbNodes.Create(1);
    ueNodes.Create(p.numVehicles);

    // gNB: fixed, 25 m mast
    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> gnbPosition = CreateObject<ListPositionAllocator>();
    gnbPosition->Add(Vector(0.0, 0.0, 25.0));
    gnbMobility.SetPositionAllocator(gnbPosition);
    gnbMobility.Install(gnbNodes);

    // Vehicles: evenly spaced along the road, constant velocity in +x
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    Ptr<ListPositionAllocator> uePosition = CreateObject<ListPositionAllocator>();
    double spacing = p.roadLength / p.numVehicles;
    for (uint32_t i = 0; i < p.numVehicles; ++i)
    {
        double x = -p.roadLength / 2.0 + spacing * (i + 0.5);
        uePosition->Add(Vector(x, 10.0, 1.5));
    }
    ueMobility.SetPositionAllocator(uePosition);
    ueMobility.Install(ueNodes);
    for (uint32_t i = 0; i < p.numVehicles; ++i)
    {
        ueNodes.Get(i)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(
            Vector(p.speed, 0.0, 0.0));
    }

    NetDeviceContainer gnbDevs = cfg.nrHelper->InstallGnbDevice(gnbNodes, cfg.allBwps);
    NetDeviceContainer ueDevs = cfg.nrHelper->InstallUeDevice(ueNodes, cfg.allBwps);

    // Fixed random streams for reproducibility
    int64_t randomStream = 1;
    randomStream += cfg.nrHelper->AssignStreams(gnbDevs, randomStream);
    randomStream += cfg.nrHelper->AssignStreams(ueDevs, randomStream);

    // Internet: remote host behind the core network (zero core delay, so
    // the measured latency is the RAN latency)
    auto [remoteHost, remoteHostIpv4Address] =
        cfg.epcHelper->SetupRemoteHost("100Gb/s", 2500, Seconds(0.0));

    InternetStackHelper internet;
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface = cfg.epcHelper->AssignUeIpv4Address(ueDevs);

    for (uint32_t i = 0; i < p.numVehicles; ++i)
    {
        cfg.nrHelper->AttachToGnb(ueDevs.Get(i), gnbDevs.Get(0));
    }

    /*
     * Per-vehicle downlink traffic according to its intent: UDP sink on the
     * vehicle, CBR source on the remote host, dedicated bearer with the
     * intent's 5QI. Each flow is registered in flowByPort so the SLA
     * monitor can map FlowMonitor results back to the intent.
     */
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;
    std::map<uint16_t, FlowSla> flowByPort;

    for (uint32_t i = 0; i < p.numVehicles; ++i)
    {
        const Intent& intent = assignment[i];
        flowByPort[intent.port] =
            FlowSla{intent.name, intent.rateMbps, intent.slaMaxLatencyMs, intent.slaMaxLossRate};

        UdpServerHelper sink(intent.port);
        serverApps.Add(sink.Install(ueNodes.Get(i)));

        double intervalSec = intent.packetSizeBytes * 8.0 / (intent.rateMbps * 1e6);
        UdpClientHelper client;
        client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        client.SetAttribute("PacketSize", UintegerValue(intent.packetSizeBytes));
        client.SetAttribute("Interval", TimeValue(Seconds(intervalSec)));
        client.SetAttribute("Remote",
                            AddressValue(addressUtils::ConvertToSocketAddress(
                                ueIpIface.GetAddress(i), intent.port)));
        clientApps.Add(client.Install(remoteHost));

        /*
         * Under QoS protection, remote driving and sensor sharing get a GBR
         * bearer reserving their rate (dedicated resources); the QoS
         * scheduler serves these first, by 5QI priority, and lets the other
         * intents share what is left.
         */
        NrEpsBearer bearer(intent.qci);
        if (p.qosProtected && (intent.name == "remote-driving" || intent.name == "sensor-sharing"))
        {
            NrGbrQosInformation gbr;
            uint64_t bps = static_cast<uint64_t>(intent.rateMbps * 1e6);
            gbr.gbrDl = bps;
            gbr.mbrDl = bps;
            bearer = NrEpsBearer(intent.qci, gbr);
        }
        Ptr<NrEpcTft> tft = Create<NrEpcTft>();
        NrEpcTft::PacketFilter filter;
        filter.direction = NrEpcTft::DOWNLINK;
        filter.localPortStart = intent.port;
        filter.localPortEnd = intent.port;
        tft->Add(filter);
        cfg.nrHelper->ActivateDedicatedEpsBearer(ueDevs.Get(i), bearer, tft);
    }

    serverApps.Start(Seconds(p.appStart));
    clientApps.Start(Seconds(p.appStart));
    serverApps.Stop(Seconds(p.simTime));
    clientApps.Stop(Seconds(p.simTime));

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpoints;
    endpoints.Add(remoteHost);
    endpoints.Add(ueNodes);
    Ptr<FlowMonitor> monitor = flowmonHelper.Install(endpoints);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

    Simulator::Stop(Seconds(p.simTime));
    Simulator::Run();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    double flowDuration = p.simTime - p.appStart;
    bool breach = AnalyzeAndReport(monitor, classifier, flowByPort, flowDuration, p);

    Simulator::Destroy();
    return breach;
}
