#include "scenario.h"

/*
 * Case 2: heterogeneous demand. Vehicles request different SDV intents,
 * assigned round robin over the intent catalogue so every density step
 * adds a predictable, reproducible mix (vehicle 0 = remote-driving,
 * vehicle 1 = cooperative-awareness, ...).
 */
std::vector<Intent>
AssignMultiIntent(uint32_t numVehicles)
{
    const std::vector<Intent>& catalogue = AllIntents();
    std::vector<Intent> assignment;
    assignment.reserve(numVehicles);
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        assignment.push_back(catalogue[i % catalogue.size()]);
    }
    return assignment;
}
