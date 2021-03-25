#pragma once

#include "common.h"

void fxInit();

void fxDeinit();

void fxSetParamStr(uint8_t paramIndex);

void fxProcess(float xL[], float xR[]);
