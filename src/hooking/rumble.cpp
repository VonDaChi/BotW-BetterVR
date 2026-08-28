#include "rumble.h"
#include "cemu_hooks.h"
#include "instance.h"


void CemuHooks::hook_XRRumble_VPADControlMotor(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    uint32_t channel = hCPU->gpr[3];
    uint32_t patternPtr = hCPU->gpr[4];
    uint8_t length = hCPU->gpr[5];

    uint8_t* pattern = (uint8_t*)(s_memoryBaseAddress + patternPtr);

    VRManager::instance().XR->GetRumbleManager()->controlMotor(pattern, length);
}

void CemuHooks::hook_XRRumble_VPADStopMotor(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    uint32_t channel = hCPU->gpr[3];

    VRManager::instance().XR->GetRumbleManager()->stopMotor();
}