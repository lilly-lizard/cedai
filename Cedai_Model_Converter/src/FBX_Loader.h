#pragma once
#include "AnimatedModel.h"

#define VERSION_NUMBER 1

void writeBinary(const AnimatedModel & model);
void readBinary(AnimatedModel &model);