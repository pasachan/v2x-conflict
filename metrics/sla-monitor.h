#ifndef SLA_MONITOR_H
#define SLA_MONITOR_H

#include "../scenario/scenario.h"
#include "../traffic/intents.h"

#include "ns3/flow-monitor-module.h"

#include <map>

using namespace ns3;

/*
 * Walks the FlowMonitor results, maps every flow back to its intent via the
 * destination port, and checks each flow against that flow's SLA:
 *
 *   throughput >= 95% of slaThroughputMbps
 *   mean delay <= slaMaxLatencyMs
 *   loss ratio <= slaMaxLossRate
 *
 * Prints a per-intent report, appends one CSV row per intent to
 * params.csvPath, and returns true if any flow violated its SLA (= the
 * conflict point for that vehicle density).
 */
bool AnalyzeAndReport(Ptr<FlowMonitor> monitor,
                      Ptr<Ipv4FlowClassifier> classifier,
                      const std::map<uint16_t, FlowSla>& flowByPort,
                      double flowDurationSec,
                      const ScenarioParams& params);

#endif
