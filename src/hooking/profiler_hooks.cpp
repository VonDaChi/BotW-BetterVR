#include "pch.h"

#include "cemu_hooks.h"

static bool TryGetProfilerSection(uint32_t sectionId, BetterVRProfiler::Section* outSection) {
    if (sectionId >= (uint32_t)BetterVRProfiler::Section::Count) {
        return false;
    }

    *outSection = (BetterVRProfiler::Section)sectionId;
    return true;
}

void CemuHooks::hook_ProfileSectionBegin(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    BetterVRProfiler::Section section = BetterVRProfiler::Section::RoomscaleResolve;
    if (!TryGetProfilerSection(hCPU->gpr[3], &section)) {
        return;
    }

    BetterVRProfiler::BeginSpan(section);
}

void CemuHooks::hook_ProfileSectionEnd(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    BetterVRProfiler::Section section = BetterVRProfiler::Section::RoomscaleResolve;
    if (!TryGetProfilerSection(hCPU->gpr[3], &section)) {
        return;
    }

    BetterVRProfiler::EndSpan(section);
}
