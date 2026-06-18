#ifndef FIVEG_CONFIG_H
#define FIVEG_CONFIG_H

#include "ran-config.h"

// 5G NR mid-band macro cell (3.5 GHz, 100 MHz, 30 kHz SCS, 8x8 MIMO).
RanConfig Configure5G(bool enableMimoFeedback = true);

#endif
