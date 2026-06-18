#ifndef INTENTS_H
#define INTENTS_H

#include "ns3/nr-module.h"

#include <string>
#include <vector>

using namespace ns3;

/*
 * An "intent" is a service a vehicle requests from the network, together
 * with the SLA it needs. A vehicle with an intent gets one downlink UDP CBR
 * flow (remote host -> vehicle) at rateMbps; the SLA (throughput, latency,
 * loss) is checked per flow after the simulation.
 *
 * Each intent has a unique destination port so flows can be mapped back to
 * their intent in the FlowMonitor results.
 */
struct Intent
{
    std::string name;
    double rateMbps;          // downlink offered rate
    uint32_t packetSizeBytes; // UDP payload size
    double slaMaxLatencyMs;   // SLA: maximum mean one-way delay
    double slaMaxLossRate;    // SLA: maximum packet loss ratio (0..1)
    uint16_t port;            // unique per intent, for flow->intent mapping
    NrEpsBearer::Qci qci;     // 5QI used for the dedicated bearer
};

/*
 * One flow, the unit the SLA monitor evaluates. label is the intent name.
 */
struct FlowSla
{
    std::string label;
    double slaThroughputMbps; // required rate
    double slaMaxLatencyMs;
    double slaMaxLossRate;
};

// The catalogue of SDV intents used in the multi-intent scenario.
const std::vector<Intent>& AllIntents();

// Lookup by name; aborts if the intent does not exist.
const Intent& IntentByName(const std::string& name);

#endif
