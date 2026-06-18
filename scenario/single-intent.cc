#include "scenario.h"

/*
 * Case 1: homogeneous demand — every vehicle requests remote driving
 * (20 Mbps, <= 5 ms). The cell breaks when numVehicles * 20 Mbps
 * approaches the cell capacity, or earlier for cell-edge vehicles.
 */
std::vector<Intent>
AssignSingleIntent(uint32_t numVehicles)
{
    return std::vector<Intent>(numVehicles, IntentByName("remote-driving"));
}
