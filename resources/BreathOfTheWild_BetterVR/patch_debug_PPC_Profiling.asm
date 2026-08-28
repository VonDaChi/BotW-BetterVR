[BetterVR_PPC_Profiling_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

; BetterVRProfiler::Section values. Keep in sync with src/utils/profiler.h.
PPC_SystemPreCalc                    = 12
PPC_SystemStateMachine               = 13
PPC_SystemPostCalc                   = 14
PPC_CalcPlacementMgr                 = 15
PPC_PhysicsPostBgBaseProcMgr         = 16
PPC_ActorUpdateJobs                  = 17
PPC_GraphicsCalc                     = 18
PPC_SystemTaskPreCalc               = 19
PPC_SystemTaskPostCalc              = 20
PPC_SystemTaskDrawTV                = 21
PPC_SystemTaskDrawDRC               = 22
PPC_SystemTaskPostDrawTV            = 23
PPC_SystemTaskPostDrawDRC           = 24
PPC_Layer3DDraw                     = 25
PPC_Layer3DCalcView                 = 26
PPC_Layer3DCalcViewGPU              = 27
PPC_Layer3DDrawBG                   = 28
PPC_Layer3DDrawOpaque               = 29
PPC_Layer3DDrawXlu                  = 30
PPC_Layer3DDrawPostEffects          = 31
PPC_Layer3DDrawFinalImage           = 32
PPC_ActorJob0_1                     = 33
PPC_ActorJob0_2                     = 34
PPC_ActorJob1_1                     = 35
PPC_ActorJob1_2                     = 36
PPC_ActorJob2_1Ragdoll              = 37
PPC_ActorJob2_2                     = 38
PPC_ActorJob4                       = 39

0x02C57E54 = uking__frm__System__preCalc_continue:
0x02C57E84 = uking__frm__System__calcAndRunStateMachine_continue:
0x02C57EE0 = uking__frm__System__postCalc_continue:
0x0340EEE4 = ksys__CalcPlacementMgr_continue:
0x031FCE74 = MCMgr__calcPostBgBaseProcMgr_continue:
0x03414D7C = runActorUpdateStuff_continue:
0x03416598 = gameScene__CalcGraphicsStuff_continue:

0x03A128DC = gsys__SystemTask__preCalc_continue:
0x03A135A8 = gsys__SystemTask__postCalc_continue:
0x03A146E8 = gsys__SystemTask__drawTV_continue:
0x03A1480C = gsys__SystemTask__drawDRC_continue:
0x03A148DC = gsys__SystemTask__postDrawTV_continue:
0x03A14978 = gsys__SystemTask__postDrawDRC_continue:

0x037A5DB8 = real_actor_job0_1_continue:
0x037A7D44 = real_actor_job0_2_continue:
0x037A6ECC = real_actor_job1_1_continue:
0x037A7CC0 = real_actor_job1_2_continue:
0x037A7448 = real_actor_job2_1_ragdoll_related_continue:
0x037A7E34 = real_actor_job2_2_continue:
0x037A7C08 = real_actor_job4_continue:

Profile_uking__frm__System__preCalc:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)
stw r3, 0x08(r1)
stw r4, 0x0C(r1)

li r3, PPC_SystemPreCalc
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lwz r4, 0x0C(r1)
lis r0, ReturnAfter_uking__frm__System__preCalc@h
ori r0, r0, ReturnAfter_uking__frm__System__preCalc@l
stw r0, 0x04(r1)
lis r12, uking__frm__System__preCalc_continue@ha
addi r12, r12, uking__frm__System__preCalc_continue@l
mtctr r12
bctr

ReturnAfter_uking__frm__System__preCalc:
li r3, PPC_SystemPreCalc
bla import.coreinit.hook_ProfileSectionEnd
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_uking__frm__System__calcAndRunStateMachine:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)
stw r3, 0x08(r1)
stw r4, 0x0C(r1)

li r3, PPC_SystemStateMachine
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lwz r4, 0x0C(r1)
lis r0, ReturnAfter_uking__frm__System__calcAndRunStateMachine@h
ori r0, r0, ReturnAfter_uking__frm__System__calcAndRunStateMachine@l
stwu r1, -0x18(r1)
lis r12, uking__frm__System__calcAndRunStateMachine_continue@ha
addi r12, r12, uking__frm__System__calcAndRunStateMachine_continue@l
mtctr r12
bctr

ReturnAfter_uking__frm__System__calcAndRunStateMachine:
stw r3, 0x08(r1)
li r3, PPC_SystemStateMachine
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_uking__frm__System__postCalc:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)
stw r3, 0x08(r1)
stw r4, 0x0C(r1)

li r3, PPC_SystemPostCalc
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lwz r4, 0x0C(r1)
lis r0, ReturnAfter_uking__frm__System__postCalc@h
ori r0, r0, ReturnAfter_uking__frm__System__postCalc@l
stw r0, 0x04(r1)
lis r12, uking__frm__System__postCalc_continue@ha
addi r12, r12, uking__frm__System__postCalc_continue@l
mtctr r12
bctr

ReturnAfter_uking__frm__System__postCalc:
li r3, PPC_SystemPostCalc
bla import.coreinit.hook_ProfileSectionEnd
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_ksys__CalcPlacementMgr:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)

li r3, PPC_CalcPlacementMgr
bla import.coreinit.hook_ProfileSectionBegin

lis r0, ReturnAfter_ksys__CalcPlacementMgr@h
ori r0, r0, ReturnAfter_ksys__CalcPlacementMgr@l
stwu r1, -0x40(r1)
lis r12, ksys__CalcPlacementMgr_continue@ha
addi r12, r12, ksys__CalcPlacementMgr_continue@l
mtctr r12
bctr

ReturnAfter_ksys__CalcPlacementMgr:
li r3, PPC_CalcPlacementMgr
bla import.coreinit.hook_ProfileSectionEnd
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_MCMgr__calcPostBgBaseProcMgr:
stwu r1, -0x20(r1)
mflr r12
stw r12, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_PhysicsPostBgBaseProcMgr
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r12, ReturnAfter_MCMgr__calcPostBgBaseProcMgr@ha
addi r12, r12, ReturnAfter_MCMgr__calcPostBgBaseProcMgr@l
mtlr r12
stwu r1, -0xB0(r1)
stmw r23, 0x8C(r1)
lis r12, MCMgr__calcPostBgBaseProcMgr_continue@ha
addi r12, r12, MCMgr__calcPostBgBaseProcMgr_continue@l
mtctr r12
bctr

ReturnAfter_MCMgr__calcPostBgBaseProcMgr:
stw r3, 0x08(r1)
li r3, PPC_PhysicsPostBgBaseProcMgr
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_runActorUpdateStuff:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)

li r3, PPC_ActorUpdateJobs
bla import.coreinit.hook_ProfileSectionBegin

lis r0, ReturnAfter_runActorUpdateStuff@h
ori r0, r0, ReturnAfter_runActorUpdateStuff@l
stwu r1, -0x10(r1)
lis r12, runActorUpdateStuff_continue@ha
addi r12, r12, runActorUpdateStuff_continue@l
mtctr r12
bctr

ReturnAfter_runActorUpdateStuff:
stw r3, 0x08(r1)
li r3, PPC_ActorUpdateJobs
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_gameScene__CalcGraphicsStuff:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_GraphicsCalc
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r0, ReturnAfter_gameScene__CalcGraphicsStuff@h
ori r0, r0, ReturnAfter_gameScene__CalcGraphicsStuff@l
stwu r1, -0x18(r1)
lis r12, gameScene__CalcGraphicsStuff_continue@ha
addi r12, r12, gameScene__CalcGraphicsStuff_continue@l
mtctr r12
bctr

ReturnAfter_gameScene__CalcGraphicsStuff:
stw r3, 0x08(r1)
li r3, PPC_GraphicsCalc
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_gsys__SystemTask__preCalc:
stwu r1, -0x20(r1)
mflr r12
stw r12, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_SystemTaskPreCalc
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r12, ReturnAfter_gsys__SystemTask__preCalc@ha
addi r12, r12, ReturnAfter_gsys__SystemTask__preCalc@l
mtlr r12
stwu r1, -0x48(r1)
stmw r20, 0x18(r1)
lis r12, gsys__SystemTask__preCalc_continue@ha
addi r12, r12, gsys__SystemTask__preCalc_continue@l
mtctr r12
bctr

ReturnAfter_gsys__SystemTask__preCalc:
stw r3, 0x08(r1)
li r3, PPC_SystemTaskPreCalc
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_gsys__SystemTask__postCalc:
stwu r1, -0x20(r1)
mflr r12
stw r12, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_SystemTaskPostCalc
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r12, ReturnAfter_gsys__SystemTask__postCalc@ha
addi r12, r12, ReturnAfter_gsys__SystemTask__postCalc@l
mtlr r12
stwu r1, -0xA8(r1)
stmw r22, 0x80(r1)
lis r12, gsys__SystemTask__postCalc_continue@ha
addi r12, r12, gsys__SystemTask__postCalc_continue@l
mtctr r12
bctr

ReturnAfter_gsys__SystemTask__postCalc:
stw r3, 0x08(r1)
li r3, PPC_SystemTaskPostCalc
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_gsys__SystemTask__drawTV:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_SystemTaskDrawTV
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r0, ReturnAfter_gsys__SystemTask__drawTV@h
ori r0, r0, ReturnAfter_gsys__SystemTask__drawTV@l
stwu r1, -0x48(r1)
lis r12, gsys__SystemTask__drawTV_continue@ha
addi r12, r12, gsys__SystemTask__drawTV_continue@l
mtctr r12
bctr

ReturnAfter_gsys__SystemTask__drawTV:
stw r3, 0x08(r1)
li r3, PPC_SystemTaskDrawTV
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_gsys__SystemTask__drawDRC:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_SystemTaskDrawDRC
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r0, ReturnAfter_gsys__SystemTask__drawDRC@h
ori r0, r0, ReturnAfter_gsys__SystemTask__drawDRC@l
stwu r1, -0x30(r1)
lis r12, gsys__SystemTask__drawDRC_continue@ha
addi r12, r12, gsys__SystemTask__drawDRC_continue@l
mtctr r12
bctr

ReturnAfter_gsys__SystemTask__drawDRC:
stw r3, 0x08(r1)
li r3, PPC_SystemTaskDrawDRC
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_gsys__SystemTask__postDrawTV:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_SystemTaskPostDrawTV
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r0, ReturnAfter_gsys__SystemTask__postDrawTV@h
ori r0, r0, ReturnAfter_gsys__SystemTask__postDrawTV@l
stwu r1, -0x28(r1)
lis r12, gsys__SystemTask__postDrawTV_continue@ha
addi r12, r12, gsys__SystemTask__postDrawTV_continue@l
mtctr r12
bctr

ReturnAfter_gsys__SystemTask__postDrawTV:
stw r3, 0x08(r1)
li r3, PPC_SystemTaskPostDrawTV
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_gsys__SystemTask__postDrawDRC:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_SystemTaskPostDrawDRC
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r0, ReturnAfter_gsys__SystemTask__postDrawDRC@h
ori r0, r0, ReturnAfter_gsys__SystemTask__postDrawDRC@l
stwu r1, -0x28(r1)
lis r12, gsys__SystemTask__postDrawDRC_continue@ha
addi r12, r12, gsys__SystemTask__postDrawDRC_continue@l
mtctr r12
bctr

ReturnAfter_gsys__SystemTask__postDrawDRC:
stw r3, 0x08(r1)
li r3, PPC_SystemTaskPostDrawDRC
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_Actor__job0_1:
stwu r1, -0x20(r1)
mflr r12
stw r12, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_ActorJob0_1
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r12, ReturnAfter_Actor__job0_1@ha
addi r12, r12, ReturnAfter_Actor__job0_1@l
mtlr r12
stwu r1, -0x98(r1)
stmw r17, 0x5C(r1)
lis r12, real_actor_job0_1_continue@ha
addi r12, r12, real_actor_job0_1_continue@l
mtctr r12
bctr

ReturnAfter_Actor__job0_1:
stw r3, 0x08(r1)
li r3, PPC_ActorJob0_1
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_Actor__job0_2:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_ActorJob0_2
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r0, ReturnAfter_Actor__job0_2@h
ori r0, r0, ReturnAfter_Actor__job0_2@l
stwu r1, -0x10(r1)
lis r12, real_actor_job0_2_continue@ha
addi r12, r12, real_actor_job0_2_continue@l
mtctr r12
bctr

ReturnAfter_Actor__job0_2:
stw r3, 0x08(r1)
li r3, PPC_ActorJob0_2
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_Actor__job1_1:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_ActorJob1_1
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r0, ReturnAfter_Actor__job1_1@h
ori r0, r0, ReturnAfter_Actor__job1_1@l
stwu r1, -0x20(r1)
lis r12, real_actor_job1_1_continue@ha
addi r12, r12, real_actor_job1_1_continue@l
mtctr r12
bctr

ReturnAfter_Actor__job1_1:
stw r3, 0x08(r1)
li r3, PPC_ActorJob1_1
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_Actor__job1_2:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_ActorJob1_2
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r0, ReturnAfter_Actor__job1_2@h
ori r0, r0, ReturnAfter_Actor__job1_2@l
stwu r1, -0x10(r1)
lis r12, real_actor_job1_2_continue@ha
addi r12, r12, real_actor_job1_2_continue@l
mtctr r12
bctr

ReturnAfter_Actor__job1_2:
stw r3, 0x08(r1)
li r3, PPC_ActorJob1_2
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_Actor__job2_1_ragdoll_related:
stwu r1, -0x20(r1)
mflr r12
stw r12, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_ActorJob2_1Ragdoll
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r12, ReturnAfter_Actor__job2_1_ragdoll_related@ha
addi r12, r12, ReturnAfter_Actor__job2_1_ragdoll_related@l
mtlr r12
stwu r1, -0x310(r1)
stmw r23, 0x2DC(r1)
stfd f31, 0x300(r1)
ps_merge10 f31, f31, f31
lis r12, real_actor_job2_1_ragdoll_related_continue@ha
addi r12, r12, real_actor_job2_1_ragdoll_related_continue@l
mtctr r12
bctr

ReturnAfter_Actor__job2_1_ragdoll_related:
stw r3, 0x08(r1)
li r3, PPC_ActorJob2_1Ragdoll
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

Profile_Actor__job2_2:
stwu r1, -0x20(r1)
mflr r11
stw r11, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_ActorJob2_2
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r11, ReturnAfter_Actor__job2_2@ha
addi r11, r11, ReturnAfter_Actor__job2_2@l
mtlr r11
lwz r12, 0x364(r3)
lis r11, real_actor_job2_2_continue@ha
addi r11, r11, real_actor_job2_2_continue@l
mtctr r11
bctr

ReturnAfter_Actor__job2_2:
stw r3, 0x08(r1)
li r3, PPC_ActorJob2_2
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r11, 0x1C(r1)
mtlr r11
addi r1, r1, 0x20
blr

Profile_Actor__job4:
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)
stw r3, 0x08(r1)

li r3, PPC_ActorJob4
bla import.coreinit.hook_ProfileSectionBegin

lwz r3, 0x08(r1)
lis r0, ReturnAfter_Actor__job4@h
ori r0, r0, ReturnAfter_Actor__job4@l
stwu r1, -0x10(r1)
lis r12, real_actor_job4_continue@ha
addi r12, r12, real_actor_job4_continue@l
mtctr r12
bctr

ReturnAfter_Actor__job4:
stw r3, 0x08(r1)
li r3, PPC_ActorJob4
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x08(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
blr

0x02C57E50 = ba Profile_uking__frm__System__preCalc
0x02C57E80 = ba Profile_uking__frm__System__calcAndRunStateMachine
0x02C57EDC = ba Profile_uking__frm__System__postCalc
0x0340EEE0 = ba Profile_ksys__CalcPlacementMgr
0x031FCE6C = ba Profile_MCMgr__calcPostBgBaseProcMgr
0x03414D78 = ba Profile_runActorUpdateStuff
0x03416594 = ba Profile_gameScene__CalcGraphicsStuff

0x03A128D4 = ba Profile_gsys__SystemTask__preCalc
0x03A135A0 = ba Profile_gsys__SystemTask__postCalc
0x03A146E4 = ba Profile_gsys__SystemTask__drawTV
0x03A14808 = ba Profile_gsys__SystemTask__drawDRC
0x03A148D8 = ba Profile_gsys__SystemTask__postDrawTV
0x03A14974 = ba Profile_gsys__SystemTask__postDrawDRC

0x037A5DB0 = ba Profile_Actor__job0_1
0x037A7D40 = ba Profile_Actor__job0_2
0x037A6EC8 = ba Profile_Actor__job1_1
0x037A7CBC = ba Profile_Actor__job1_2
0x037A7438 = ba Profile_Actor__job2_1_ragdoll_related
0x037A7E30 = ba Profile_Actor__job2_2
0x037A7C04 = ba Profile_Actor__job4


; ==================================================================================
; Subdivisions of PPC_SystemStateMachine. These wrap the call sites inside
; GameScene::calcAndRunStateMachine rather than the callee prologues, so no prologue
; needs replicating. Each incoming argument and each return value is spilled around
; the hook calls because the HLE transition may clobber volatiles.
; Note gameScene::CalcGraphicsStuff is called from here too but already has its own
; marker, so the state machine total already contains it.

PPC_PreCalcWorldPre                 = 47
PPC_CalcEntryJob                    = 48
PPC_StateMachineRun                 = 49
PPC_CalcControllerAndUi             = 50
PPC_MCMgrCalc                       = 51
PPC_PhysicsMemSysCalc               = 52
PPC_TeraStuff                       = 53

0x03415600 = target_ksys__PreCalcWorldPre:
0x03415CD0 = target_ksys__calcEntryJob:
0x03625E00 = target_StateMachine__run:
0x03415AF8 = target_ksys__CalcControllerAndUi:
0x031FD360 = target_MCMgr__calc:
0x037FFE74 = target_PhysicsMemSys__calc:
0x03417E5C = target_teraStuff:

Profile_PreCalcWorldPre:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_PreCalcWorldPre
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_ksys__PreCalcWorldPre@ha
addi r12, r12, target_ksys__PreCalcWorldPre@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_PreCalcWorldPre
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

Profile_CalcEntryJob:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_CalcEntryJob
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_ksys__calcEntryJob@ha
addi r12, r12, target_ksys__calcEntryJob@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_CalcEntryJob
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

Profile_StateMachineRun:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_StateMachineRun
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_StateMachine__run@ha
addi r12, r12, target_StateMachine__run@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_StateMachineRun
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

Profile_CalcControllerAndUi:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_CalcControllerAndUi
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_ksys__CalcControllerAndUi@ha
addi r12, r12, target_ksys__CalcControllerAndUi@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_CalcControllerAndUi
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

Profile_MCMgrCalc:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_MCMgrCalc
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_MCMgr__calc@ha
addi r12, r12, target_MCMgr__calc@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_MCMgrCalc
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

Profile_PhysicsMemSysCalc:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_PhysicsMemSysCalc
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_PhysicsMemSys__calc@ha
addi r12, r12, target_PhysicsMemSys__calc@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_PhysicsMemSysCalc
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

Profile_TeraStuff:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_TeraStuff
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_teraStuff@ha
addi r12, r12, target_teraStuff@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_TeraStuff
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

0x02C2D190 = bla Profile_TeraStuff
0x02C2D198 = bla Profile_PreCalcWorldPre
0x02C2D1E0 = bla Profile_PhysicsMemSysCalc
0x02C2D204 = bla Profile_MCMgrCalc
0x02C2D30C = bla Profile_CalcEntryJob
0x02C2D47C = bla Profile_StateMachineRun
0x02C2D484 = bla Profile_CalcControllerAndUi
0x02C2D48C = bla Profile_StateMachineRun

; ---------------------------------------------------------------------------
; Subdivisions of ksys::calcEntryJob @ 0x03415CD0.
;
; These split the two blocking WorkerMgr::sync waits from the work calcEntryJob
; does inline, separating time spent in the Havok jobs from time spent in the
; passes on the main thread.
; ---------------------------------------------------------------------------

PPC_EntryControllerUi      = 54
PPC_EntryWorkerSync1       = 55
PPC_EntryWorkerSync2       = 56
PPC_EntryTerrainAuto15     = 57
PPC_EntryGrassCut          = 58
PPC_EntryInvoker3          = 59
PPC_EntryInvoker4          = 60
PPC_EntryAttention         = 61

0x03415AF8 = target_EntryControllerUi:
0x030CBB84 = target_EntryWorkerSync1:
0x030CBB84 = target_EntryWorkerSync2:
0x036F3768 = target_EntryTerrainAuto15:
0x0340F03C = target_EntryGrassCut:
0x031FD370 = target_EntryInvoker3:
0x031FD394 = target_EntryInvoker4:
0x03415C6C = target_EntryAttention:

; ksys::CalcControllerAndUi, called inline between run and sync
Profile_EntryControllerUi:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryControllerUi
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryControllerUi@ha
addi r12, r12, target_EntryControllerUi@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryControllerUi
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; sead::WorkerMgr::sync, the first block on the Havok workers
Profile_EntryWorkerSync1:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryWorkerSync1
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryWorkerSync1@ha
addi r12, r12, target_EntryWorkerSync1@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryWorkerSync1
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; sead::WorkerMgr::sync, the second block
Profile_EntryWorkerSync2:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryWorkerSync2
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryWorkerSync2@ha
addi r12, r12, target_EntryWorkerSync2@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryWorkerSync2
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; Terrain::__auto15, four args so r4-r6 must survive the hook
Profile_EntryTerrainAuto15:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryTerrainAuto15
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryTerrainAuto15@ha
addi r12, r12, target_EntryTerrainAuto15@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryTerrainAuto15
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; GrassCutJob::runInner
Profile_EntryGrassCut:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryGrassCut
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryGrassCut@ha
addi r12, r12, target_EntryGrassCut@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryGrassCut
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; MCMgr::requestInvoker3
Profile_EntryInvoker3:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryInvoker3
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryInvoker3@ha
addi r12, r12, target_EntryInvoker3@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryInvoker3
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; MCMgr::requestInvoker4OrSound
Profile_EntryInvoker4:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryInvoker4
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryInvoker4@ha
addi r12, r12, target_EntryInvoker4@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryInvoker4
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; ksys::CalcAttentionAndVibration, the targeting and highlight pass
Profile_EntryAttention:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryAttention
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryAttention@ha
addi r12, r12, target_EntryAttention@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryAttention
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

0x034160F0 = bla Profile_EntryControllerUi
0x034160F8 = bla Profile_EntryWorkerSync1
0x03416248 = bla Profile_EntryWorkerSync2
0x03416480 = bla Profile_EntryTerrainAuto15
0x03416528 = bla Profile_EntryGrassCut
0x03416530 = bla Profile_EntryInvoker3
0x03416538 = bla Profile_EntryInvoker4
0x0341656C = bla Profile_EntryAttention

PPC_EntryBaseProcPre       = 62
PPC_EntryWorkerRun1        = 63
PPC_EntryGoIdle            = 64
PPC_EntryPushExtraJobs     = 65
PPC_EntryWorkerRun2        = 66

0x03790990 = target_EntryBaseProcPre:
0x030CB940 = target_EntryWorkerRun1:
0x03790804 = target_EntryGoIdle:
0x030CB8F0 = target_EntryPushExtraJobs:
0x030CB940 = target_EntryWorkerRun2:

; sub_3790990(BaseProcMgr), before the queues are pushed
Profile_EntryBaseProcPre:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryBaseProcPre
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryBaseProcPre@ha
addi r12, r12, target_EntryBaseProcPre@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryBaseProcPre
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; sead::WorkerMgr::run, first. sync is ~0 ms so the calling thread very likely runs the jobs here
Profile_EntryWorkerRun1:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryWorkerRun1
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryWorkerRun1@ha
addi r12, r12, target_EntryWorkerRun1@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryWorkerRun1
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; ksys::act::BaseProcMgr::goIdle
Profile_EntryGoIdle:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryGoIdle
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryGoIdle@ha
addi r12, r12, target_EntryGoIdle@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryGoIdle
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; ksys::act::BaseProcMgr::pushExtraJobsEx
Profile_EntryPushExtraJobs:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryPushExtraJobs
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryPushExtraJobs@ha
addi r12, r12, target_EntryPushExtraJobs@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryPushExtraJobs
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; sead::WorkerMgr::run, second
Profile_EntryWorkerRun2:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryWorkerRun2
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryWorkerRun2@ha
addi r12, r12, target_EntryWorkerRun2@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryWorkerRun2
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

0x03415FDC = bla Profile_EntryBaseProcPre
0x034160EC = bla Profile_EntryWorkerRun1
0x03416100 = bla Profile_EntryGoIdle
0x034161F8 = bla Profile_EntryPushExtraJobs
0x03416204 = bla Profile_EntryWorkerRun2

PPC_EntryEffectCalc        = 67
PPC_EntryXLinkCalc         = 68

0x03783F74 = target_EntryEffectCalc:
0x0383B398 = target_EntryXLinkCalc:

; Effect::calc
Profile_EntryEffectCalc:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryEffectCalc
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryEffectCalc@ha
addi r12, r12, target_EntryEffectCalc@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryEffectCalc
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; XLink::calc, the effect and sound linkage system
Profile_EntryXLinkCalc:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryXLinkCalc
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryXLinkCalc@ha
addi r12, r12, target_EntryXLinkCalc@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryXLinkCalc
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

0x03415D18 = bla Profile_EntryEffectCalc
0x03415D2C = bla Profile_EntryXLinkCalc

; The MessageDispatcher call at 0x03415D44 is a bctrl, so the trampoline must leave CTR
; alone and simply branch through it.

PPC_EntryMessageDispatch    = 69

Profile_EntryMessageDispatch:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryMessageDispatch
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryMessageDispatch
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

0x03415D44 = bla Profile_EntryMessageDispatch

PPC_EntryTeraCheck         = 70
PPC_EntryIsSpecialJob      = 71
PPC_EntryWorldMgrCheck     = 72
PPC_EntryGraphicsCalc      = 73

0x03414668 = target_EntryTeraCheck:
0x0378F190 = target_EntryIsSpecialJob:
0x0367920C = target_EntryWorldMgrCheck:
0x03409478 = target_EntryGraphicsCalc:

; checkTeraSystemInstanceStatus, the first call and the gate on the whole body
Profile_EntryTeraCheck:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryTeraCheck
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryTeraCheck@ha
addi r12, r12, target_EntryTeraCheck@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryTeraCheck
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; BaseProcMgr::isSpecialJobType
Profile_EntryIsSpecialJob:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryIsSpecialJob
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryIsSpecialJob@ha
addi r12, r12, target_EntryIsSpecialJob@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryIsSpecialJob
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; sub_367920C(WorldMgr::sInstance)
Profile_EntryWorldMgrCheck:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryWorldMgrCheck
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryWorldMgrCheck@ha
addi r12, r12, target_EntryWorldMgrCheck@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryWorldMgrCheck
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

; sub_3409478(Graphics::sInstance)
Profile_EntryGraphicsCalc:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x08(r1)
li r3, PPC_EntryGraphicsCalc
bla import.coreinit.hook_ProfileSectionBegin
lwz r3, 0x08(r1)
lis r12, target_EntryGraphicsCalc@ha
addi r12, r12, target_EntryGraphicsCalc@l
mtctr r12
bctrl
stw r3, 0x0C(r1)
li r3, PPC_EntryGraphicsCalc
bla import.coreinit.hook_ProfileSectionEnd
lwz r3, 0x0C(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

0x03415CFC = bla Profile_EntryTeraCheck
0x03416498 = bla Profile_EntryIsSpecialJob
0x034164AC = bla Profile_EntryWorldMgrCheck
0x03416500 = bla Profile_EntryGraphicsCalc
