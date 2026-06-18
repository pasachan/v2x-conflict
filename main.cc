#include "scenario/scenario.h"

#include "ns3/core-module.h"

#include <iostream>

using namespace ns3;

/*
 * V2X intent-conflict study (5G mid-band).
 *
 * Finds the vehicle density at which the cell can no longer honour the SLAs
 * of the requested intents (e.g. 20 Mbps + 5 ms for remote driving).
 *
 * Examples:
 *   ./ns3 run "v2x-conflict-5g --scenario=single --numVehicles=14"
 *   ./ns3 run "v2x-conflict-5g --scenario=multi  --numVehicles=30 --qosProtected=1"
 *
 * For the full multi-seed conflict-density study see
 * scratch/v2x-conflict-5g/validation/run_study.py.
 */
int
main(int argc, char* argv[])
{
    ScenarioParams p;
    uint64_t rngRun = 1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("scenario",
                 "single (all vehicles demand remote driving) or multi (mixed SDV intents)",
                 p.scenario);
    cmd.AddValue("numVehicles", "Number of vehicles on the road segment", p.numVehicles);
    cmd.AddValue("simTime", "Total simulation time in seconds", p.simTime);
    cmd.AddValue("appStart", "Traffic start time in seconds", p.appStart);
    cmd.AddValue("roadLength", "Road segment length in meters", p.roadLength);
    cmd.AddValue("speed", "Vehicle speed in m/s", p.speed);
    cmd.AddValue("enableMimo", "Enable MIMO (PMI/RI) feedback", p.enableMimo);
    cmd.AddValue("qosProtected",
                 "QoS-protected scheduling: OFDMA QoS scheduler + GBR reservation for "
                 "remote driving and sensor sharing",
                 p.qosProtected);
    cmd.AddValue("csvPath", "CSV file to append per-intent results to", p.csvPath);
    cmd.AddValue("rngRun", "RNG run number (repeat runs for confidence intervals)", rngRun);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(p.scenario != "single" && p.scenario != "multi",
                    "--scenario must be single or multi");
    NS_ABORT_MSG_IF(p.appStart >= p.simTime, "--appStart must be smaller than --simTime");

    SeedManager::SetRun(rngRun);

    bool breach = RunScenario(p);

    // Machine-readable line for the study driver
    std::cout << "SLA_BREACH=" << (breach ? 1 : 0) << std::endl;
    return 0;
}
