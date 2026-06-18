#include "sla-monitor.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sys/stat.h>

namespace
{

// Aggregated statistics for all flows belonging to one intent+direction
struct IntentStats
{
    FlowSla flow;
    uint32_t nFlows = 0;
    uint32_t nViolating = 0;
    uint32_t nViolThroughput = 0; // which SLA component failed (a flow can
    uint32_t nViolDelay = 0;      // count in several of these)
    uint32_t nViolLoss = 0;
    double thrSum = 0.0;
    double thrMin = std::numeric_limits<double>::max();
    double delaySum = 0.0;
    double delayMax = 0.0;
    double lossSum = 0.0;
    double backlogSum = 0.0; // bytes sent but never delivered: queued in the
                             // RAN or discarded - the queueing-pressure proxy
};

bool
FileExists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

} // namespace

bool
AnalyzeAndReport(Ptr<FlowMonitor> monitor,
                 Ptr<Ipv4FlowClassifier> classifier,
                 const std::map<uint16_t, FlowSla>& flowByPort,
                 double flowDurationSec,
                 const ScenarioParams& params)
{
    monitor->CheckForLostPackets();

    std::map<uint16_t, IntentStats> statsByPort;
    for (const auto& [port, flow] : flowByPort)
    {
        statsByPort[port].flow = flow;
    }

    for (const auto& [flowId, flow] : monitor->GetFlowStats())
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        auto it = statsByPort.find(t.destinationPort);
        if (it == statsByPort.end())
        {
            continue; // not an intent flow (e.g. control traffic)
        }
        IntentStats& s = it->second;
        const FlowSla& sla = s.flow;

        double thrMbps = flow.rxBytes * 8.0 / flowDurationSec / 1e6;
        double delayMs = 0.0;
        double loss = 1.0;
        if (flow.rxPackets > 0)
        {
            delayMs = 1000.0 * flow.delaySum.GetSeconds() / flow.rxPackets;
            loss = 1.0 - static_cast<double>(flow.rxPackets) / flow.txPackets;
        }

        bool violThr = (flow.rxPackets == 0) || (thrMbps < 0.95 * sla.slaThroughputMbps);
        bool violDelay = (flow.rxPackets == 0) || (delayMs > sla.slaMaxLatencyMs);
        bool violLoss = (flow.rxPackets == 0) || (loss > sla.slaMaxLossRate);

        s.nFlows++;
        s.thrSum += thrMbps;
        s.thrMin = std::min(s.thrMin, thrMbps);
        s.delaySum += delayMs;
        s.delayMax = std::max(s.delayMax, delayMs);
        s.lossSum += loss;
        s.backlogSum += static_cast<double>(flow.txBytes) - static_cast<double>(flow.rxBytes);
        if (violThr)
        {
            s.nViolThroughput++;
        }
        if (violDelay)
        {
            s.nViolDelay++;
        }
        if (violLoss)
        {
            s.nViolLoss++;
        }
        if (violThr || violDelay || violLoss)
        {
            s.nViolating++;
        }
    }

    /*
     * Per-intent report on stdout + CSV
     */
    bool anyBreach = false;
    std::string breachedIntents;

    bool writeHeader = !FileExists(params.csvPath);
    std::ofstream csv(params.csvPath, std::ios::app);
    if (writeHeader)
    {
        csv << "tech,scenario,numVehicles,speed,intent,nFlows,offeredMbps,avgThrMbps,minThrMbps,"
            << "avgDelayMs,maxDelayMs,avgLoss,avgBacklogKB,violatingFlows,"
            << "violThr,violDelay,violLoss,slaBreached\n";
    }

    std::cout << "\n=== SLA report: tech=" << params.tech << " scenario=" << params.scenario
              << " vehicles=" << params.numVehicles << " ===\n";
    std::cout << std::left << std::setw(24) << "intent" << std::right << std::setw(7) << "flows"
              << std::setw(12) << "avgThr" << std::setw(12) << "minThr" << std::setw(12)
              << "avgDelay" << std::setw(12) << "maxDelay" << std::setw(9) << "loss"
              << std::setw(10) << "violate" << std::setw(9) << "status" << "\n";
    std::cout << std::fixed << std::setprecision(2);

    for (const auto& [port, s] : statsByPort)
    {
        if (s.nFlows == 0)
        {
            continue;
        }
        double avgThr = s.thrSum / s.nFlows;
        double avgDelay = s.delaySum / s.nFlows;
        double avgLoss = s.lossSum / s.nFlows;
        double avgBacklogKB = s.backlogSum / s.nFlows / 1024.0;
        bool breached = s.nViolating > 0;
        if (breached)
        {
            anyBreach = true;
            breachedIntents += (breachedIntents.empty() ? "" : "+") + s.flow.label;
        }

        std::cout << std::left << std::setw(24) << s.flow.label << std::right << std::setw(7)
                  << s.nFlows << std::setw(10) << avgThr << " M" << std::setw(10) << s.thrMin
                  << " M" << std::setw(9) << avgDelay << " ms" << std::setw(9) << s.delayMax
                  << " ms" << std::setw(9) << avgLoss << std::setw(10) << s.nViolating
                  << std::setw(9) << (breached ? "BREACH" : "OK") << "\n";

        csv << params.tech << "," << params.scenario << "," << params.numVehicles << ","
            << params.speed << "," << s.flow.label << "," << s.nFlows << ","
            << s.flow.slaThroughputMbps << "," << avgThr << "," << s.thrMin << "," << avgDelay
            << "," << s.delayMax << "," << avgLoss << "," << avgBacklogKB << "," << s.nViolating
            << "," << s.nViolThroughput << "," << s.nViolDelay << "," << s.nViolLoss << ","
            << (breached ? 1 : 0) << "\n";
    }
    csv.close();

    std::cout << "RESULT tech=" << params.tech << " scenario=" << params.scenario
              << " numVehicles=" << params.numVehicles << " breach=" << (anyBreach ? 1 : 0)
              << " breachedIntents=" << (breachedIntents.empty() ? "none" : breachedIntents)
              << "\n";

    return anyBreach;
}
