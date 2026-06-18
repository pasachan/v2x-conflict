#ifndef SCENARIO_H
#define SCENARIO_H

#include "../traffic/intents.h"

#include <string>
#include <vector>

/*
 * Everything that varies between simulation runs. Filled from the command
 * line in main.cc.
 */
struct ScenarioParams
{
    std::string tech = "5g";         // fixed; kept for CSV provenance
    std::string scenario = "single"; // "single" (all remote driving) or "multi"
    uint32_t numVehicles = 10;       // vehicle density on the road segment
    double simTime = 2.0;            // s, total simulated time
    double appStart = 0.5;           // s, traffic starts after attachment settles
    double roadLength = 500.0;       // m, vehicles are spread over this segment
    double speed = 13.89;            // m/s (50 km/h)
    bool enableMimo = true;          // PMI/RI feedback on/off
    bool qosProtected = false;       // QoS-protected scheduling: OFDMA QoS
                                     // scheduler + GBR reservations for
                                     // remote driving and sensor sharing;
                                     // other intents share the leftover.
    std::string csvPath = "results.csv";
};

/*
 * Builds the network for the given parameters, runs it, prints the
 * per-intent SLA report and appends it to the CSV. Returns true if any
 * intent's SLA was breached (the conflict point for this density).
 */
bool RunScenario(const ScenarioParams& params);

// Case 1: every vehicle demands remote driving. (single-intent.cc)
std::vector<Intent> AssignSingleIntent(uint32_t numVehicles);

// Case 2: vehicles demand a round-robin mix of all intents. (multi-intent.cc)
std::vector<Intent> AssignMultiIntent(uint32_t numVehicles);

#endif
