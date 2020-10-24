#pragma once
//
// file Config.h
// author Maximilien M. Cura
//

#include <Rain/MachineConfig.h>

#define CFG(RAIN_CFG) (defined RAIN_CFG_##RAIN_CFG && RAIN_CFG_##RAIN_CFG)