[BetterVR_RND_RemoveFrameWaits_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

; r3 = ticks to sleep, 0 to leave the frame alone
throttleFrameLoop:
stwu r1, -0x10(r1)
mflr r0
stw r0, 0x14(r1)

bl import.coreinit.hook_GetFrameThrottleTicks
cmpwi r3, 0
beq exit_throttleFrameLoop

mr r4, r3
li r3, 0
bl import.coreinit.OSSleepTicks

exit_throttleFrameLoop:
lwz r0, 0x14(r1)
addi r1, r1, 0x10
mtlr r0
blr

; Let OpenXR own frame pacing. FPS++'s cutscene FPS limiter is left alone, it stops a few cutscenes
; crashing above 30/60 FPS.
0x0309D018 = blr ; sead::GameFrameworkCafe::waitForNextFrame_
0x031FACC4 = blr ; sead::GameFrameworkCafe::waitForVsync

; vtable slot 0x104 resolves to either of the two above depending on the framework subclass
0x031FA9C4 = bla throttleFrameLoop ; bctrl into waitForVsync/waitForNextFrame_ in sead::GameFramework::procFrame_

; GX2DrawDone stays intact, Cemu uses it as a command-stream synchronization marker.
0x031FAB00 = nop ; GX2SetGPUFence in sead::GameFrameworkCafe::presentAndDrawDone
