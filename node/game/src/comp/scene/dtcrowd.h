#pragma once

#include <memory>
#include "DetourCrowd/DetourCrowd.h"
#undef TEXT // Assuming it's from Unreal Engine or similar
using DtCrowdPtr = std::shared_ptr<dtCrowd>;