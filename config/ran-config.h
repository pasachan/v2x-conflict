#ifndef RAN_CONFIG_H
#define RAN_CONFIG_H

#include "ns3/core-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

/*
 * Parameters that describe the radio access network. Configure5G() fills
 * this in and hands it to ConfigureRan().
 */
struct RanParams
{
    std::string name;            // used in logs and CSV
    double centralFrequency;     // Hz
    double bandwidth;            // Hz
    uint16_t numerology;         // 0=15kHz, 1=30kHz, 2=60kHz, 3=120kHz SCS
    double txPowerGnb;           // dBm
    uint32_t gnbAntennaRows;     // antenna elements at the gNB (rows x cols)
    uint32_t gnbAntennaCols;
    uint32_t ueAntennaRows;      // antenna elements at the vehicle
    uint32_t ueAntennaCols;
    uint8_t rankLimit;           // max number of MIMO layers
    bool enableMimoFeedback;     // PMI/RI feedback (spatial multiplexing)
    std::string channelScenario; // 3GPP scenario: "UMa", "UMi", "RMa", ...
    std::string scheduler;
    std::string beamforming;
    std::string errorModel;
    std::string pmSearchMethod;
};

/*
 * The live helpers produced from RanParams.
 *
 * NOTE: this struct is move-only. `band` owns the bandwidth parts that
 * `allBwps` points into, so the band must live as long as the config does.
 */
struct RanConfig
{
    std::string name;
    Ptr<NrHelper> nrHelper;
    Ptr<NrPointToPointEpcHelper> epcHelper;
    OperationBandInfo band;
    BandwidthPartInfoPtrVector allBwps;
};

RanConfig ConfigureRan(const RanParams& params);

#endif
