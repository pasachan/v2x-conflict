#include "intents.h"

/*
 * Downlink SDV intent catalogue. Values follow 3GPP TS 22.186 / TS 23.501
 * / 5GAA service-level requirements:
 *
 *  Intent                 5QI  Latency  Rate
 *  remote-driving         85   5 ms     20 Mbps
 *  cooperative-awareness  79   50 ms    0.5 Mbps
 *  sensor-sharing         83   50 ms    10 Mbps
 *  infotainment           2    150 ms   5 Mbps
 *  hd-map-update          79   50 ms    5 Mbps
 *
 * NOTE on 5QIs in nr v4.1: 85 -> DGBR_ELECTRICITY (standardized 5 ms PDB,
 * the delay-critical GBR class matching remote driving's 5 ms budget),
 * 83 -> DGBR_DISCRETE_AUT_LARGE, 79 -> NGBR_V2X, 2 -> GBR_CONV_VIDEO. All
 * four are routed by the static BWP manager (unlike 5QI 86/DGBR_V2X, which
 * is not, and aborts at runtime).
 */
const std::vector<Intent>&
AllIntents()
{
    static const std::vector<Intent> intents = {
        // name                   rate   pkt   slaLat slaLoss port  5QI
        {"remote-driving",        20.0,  1250, 5.0,   0.01,   5000, NrEpsBearer::DGBR_ELECTRICITY},
        {"cooperative-awareness", 0.5,   800,  50.0,  0.05,   5001, NrEpsBearer::NGBR_V2X},
        {"sensor-sharing",        10.0,  1200, 50.0,  0.01,   5002, NrEpsBearer::DGBR_DISCRETE_AUT_LARGE},
        {"infotainment",          5.0,   1400, 150.0, 0.02,   5003, NrEpsBearer::GBR_CONV_VIDEO},
        {"hd-map-update",         5.0,   1400, 50.0,  0.02,   5004, NrEpsBearer::NGBR_V2X},
    };
    return intents;
}

const Intent&
IntentByName(const std::string& name)
{
    for (const auto& intent : AllIntents())
    {
        if (intent.name == name)
        {
            return intent;
        }
    }
    NS_ABORT_MSG("Unknown intent: " << name);
}
