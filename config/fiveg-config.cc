#include "fiveg-config.h"

/*
 * 5G NR macro cell, FR1:
 *  - 3.5 GHz carrier (n78)
 *  - 100 MHz bandwidth
 *  - numerology 1 -> 30 kHz subcarrier spacing
 *  - 8x8 antenna array at the gNB (MIMO, up to 4 layers)
 */
RanConfig
Configure5G(bool enableMimoFeedback)
{
    RanParams p;
    p.name = "5G";
    p.centralFrequency = 3.5e9;
    p.bandwidth = 100e6;
    p.numerology = 1;
    p.txPowerGnb = 41.0;
    p.gnbAntennaRows = 8;
    p.gnbAntennaCols = 8;
    p.ueAntennaRows = 2;
    p.ueAntennaCols = 2;
    p.rankLimit = 4;
    p.enableMimoFeedback = enableMimoFeedback;
    p.channelScenario = "UMa";
    p.scheduler = "ns3::NrMacSchedulerOfdmaPF";
    p.beamforming = "ns3::QuasiOmniDirectPathBeamforming";
    p.errorModel = "ns3::NrEesmIrT2";
    p.pmSearchMethod = "ns3::NrPmSearchIdeal";

    return ConfigureRan(p);
}
