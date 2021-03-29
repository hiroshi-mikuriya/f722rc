#pragma once

#include "common.h"
#include "fx_base.h"

void tunerDisp();

void tunerProcess(float (&xL)[fx::BLOCK_SIZE], float (&xR)[fx::BLOCK_SIZE]);
