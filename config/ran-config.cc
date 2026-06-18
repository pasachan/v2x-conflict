#include "ran-config.h"

#include <cmath>

/*
 * RAN bring-up: spectrum, channel model, antennas, MIMO feedback,
 * scheduler and error model. The numeric parameters come from
 * fiveg-config.cc.
 */
RanConfig
ConfigureRan(const RanParams& p)
{
    RanConfig c;
    c.name = p.name;

    c.epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> beamformingHelper = CreateObject<IdealBeamformingHelper>();

    c.nrHelper = CreateObject<NrHelper>();
    c.nrHelper->SetBeamformingHelper(beamformingHelper);
    c.nrHelper->SetEpcHelper(c.epcHelper);

    /*
     * Spectrum: one operation band -> one component carrier -> one BWP,
     * centred at centralFrequency with the full system bandwidth.
     */
    CcBwpCreator ccBwpCreator;
    CcBwpCreator::SimpleOperationBandConf bandConf(p.centralFrequency, p.bandwidth, 1);
    c.band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);

    /*
     * 3GPP channel. Shadowing disabled and channel update disabled to keep
     * runs reproducible and fast; the SLA breach point is driven by cell
     * load, not by fading realisations.
     */
    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories(p.channelScenario, "Default", "ThreeGpp");
    channelHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    channelHelper->AssignChannelsToBands({c.band});

    c.allBwps = CcBwpCreator::GetAllBwps({c.band});

    /*
     * CQI from CSI-RS (flag 2) instead of the default PDSCH-based CQI.
     * With several UEs per cell the PDSCH-based CQI is measured under
     * whatever beam/precoder the data used, which destabilises link
     * adaptation (~35% HARQ corruption observed). CSI-RS feedback paired
     * with a quasi-omni gNB beam keeps the CSI consistent.
     */
    c.nrHelper->SetAttribute("CsiFeedbackFlags", UintegerValue(2));

    // PHY abstraction / link adaptation
    c.nrHelper->SetDlErrorModel(p.errorModel);
    c.nrHelper->SetUlErrorModel(p.errorModel);
    c.nrHelper->SetGnbDlAmcAttribute("AmcModel", EnumValue(NrAmc::ErrorModel));
    c.nrHelper->SetGnbUlAmcAttribute("AmcModel", EnumValue(NrAmc::ErrorModel));

    c.nrHelper->SetSchedulerTypeId(TypeId::LookupByName(p.scheduler));
    beamformingHelper->SetAttribute("BeamformingMethod",
                                    TypeIdValue(TypeId::LookupByName(p.beamforming)));

    /*
     * MIMO: PMI/RI feedback enables spatial multiplexing up to rankLimit
     * layers. The number of layers is bounded by the antenna *ports*
     * (4 at both ends here); the element count (8x8 / 16x16) adds
     * beamforming gain.
     */
    if (p.enableMimoFeedback)
    {
        NrHelper::MimoPmiParams mimoParams;
        mimoParams.pmSearchMethod = p.pmSearchMethod;
        mimoParams.rankLimit = p.rankLimit;
        mimoParams.subbandSize = 16;
        c.nrHelper->SetupMimoPmi(mimoParams);
    }

    /*
     * Isotropic elements: vehicles drive past the gNB on both sides, so a
     * directional 3GPP element pattern (one sector) would put half the road
     * behind the array. The MIMO array gain itself is kept.
     */
    NrHelper::AntennaParams apGnb;
    apGnb.antennaElem = "ns3::IsotropicAntennaModel";
    apGnb.nAntRows = p.gnbAntennaRows;
    apGnb.nAntCols = p.gnbAntennaCols;
    apGnb.nHorizPorts = 2;
    apGnb.nVertPorts = 2;
    apGnb.isDualPolarized = false;
    apGnb.bearingAngle = 0.0;
    apGnb.polSlantAngle = 0.0;

    /*
     * 4 ports at the UE (2x2) to match the 4 gNB ports -> up to 4 spatial
     * layers. Dual polarization is not used: cross-polarized isotropic
     * elements are degenerate and break the PMI feedback (rank chosen too
     * high, heavy HARQ loss).
     */
    NrHelper::AntennaParams apUe;
    apUe.antennaElem = "ns3::IsotropicAntennaModel";
    apUe.nAntRows = p.ueAntennaRows;
    apUe.nAntCols = p.ueAntennaCols;
    apUe.nHorizPorts = 2;
    apUe.nVertPorts = 2;
    apUe.isDualPolarized = false;
    apUe.bearingAngle = M_PI;
    apUe.polSlantAngle = 0.0;

    c.nrHelper->SetupGnbAntennas(apGnb);
    c.nrHelper->SetGnbAntennaAttribute("DowntiltAngle", DoubleValue(10.0 * M_PI / 180.0));
    c.nrHelper->SetupUeAntennas(apUe);

    c.nrHelper->SetGnbPhyAttribute("Numerology", UintegerValue(p.numerology));
    c.nrHelper->SetGnbPhyAttribute("TxPower", DoubleValue(p.txPowerGnb));
    c.nrHelper->SetUePhyAttribute("TxPower", DoubleValue(23.0));

    return c;
}
