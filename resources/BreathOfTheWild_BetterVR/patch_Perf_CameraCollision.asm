[BetterVR_PerfCameraCollision_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

; ==================================================================================
; Third-person camera collision.
;
; sub_2E61FC0 runs a Havok worldRayCast plus a SphereCast every frame to pull the camera in
; when terrain gets between it and Link. Its only product is an adjusted camera matrix, and
; updateCameraPositionAndTarget (patch_CTRL_CameraControls) overwrites the position and target
; with the headset pose immediately afterwards, so in first person none of it is ever read.
;
; The function already early-returns two instructions in, so the hook only has to decide and
; the game's own bne does the branching:
;   0x02E61FF4  lbz   r0, 0xC(r1)      flag byte, left in r0
;   0x02E61FF8  cmpwi r0, 0            <- taken over
;   0x02E61FFC  bne   loc_2E622C0      the early return
;
; LR is free here: the function stashed its own return address at 0x02E61FD8 and has since
; called sub_2E545AC, so nothing downstream reads LR before it is set again. r5-r12 are dead
; across that same call, leaving r0 (the flag) and r3 (the value the early return hands back)
; as the only registers worth preserving.

skipCameraCollisionWhenOverridden:
stwu r1, -0x20(r1)
stw r0, 0x18(r1)
mflr r0
stw r0, 0x1C(r1)
stw r3, 0x14(r1)
stw r4, 0x10(r1)

bl import.coreinit.hook_SkipCameraCollision

cmpwi r3, 0
beq keepGameCameraCollisionFlag
; any non-zero value makes the game's bne take the early return
li r0, 1
b restoreCameraCollisionFlag

keepGameCameraCollisionFlag:
lwz r0, 0x18(r1)

restoreCameraCollisionFlag:
lwz r4, 0x10(r1)
lwz r3, 0x14(r1)
lwz r12, 0x1C(r1)
mtlr r12
addi r1, r1, 0x20
; leave CR0 describing r0, which is what the instruction this replaced did
cmpwi r0, 0
blr

0x02E61FF8 = bla skipCameraCollisionWhenOverridden


; ==================================================================================
; The second collision pass.
;
; act::Camera::x_0 starts with sub_2E65180, which runs its own pass through
; sub_2E6373C -> sub_2E635E8 -> a Havok shape cast, guarded by the sub_2E622FC predicate
; ("should camera collision be skipped"). Same argument as above: the matrix it corrects is
; overwritten by the headset pose.
;
; sub_2E622FC has exactly these two call sites, so taking them over cannot reach anything else,
; and when we are not driving the camera the game's own predicate still decides.

cameraCollisionPredicateOverride:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x1C(r1)
stw r3, 0x18(r1)

bl import.coreinit.hook_SkipCameraCollision
cmpwi r3, 0
bne cameraCollisionPredicateSkip

lwz r3, 0x18(r1)
lis r12, 0x02E6
ori r12, r12, 0x22FC
mtctr r12
bctrl
b cameraCollisionPredicateDone

cameraCollisionPredicateSkip:
li r3, 1

cameraCollisionPredicateDone:
lwz r0, 0x1C(r1)
addi r1, r1, 0x20
mtlr r0
blr

0x02E65238 = bla cameraCollisionPredicateOverride
0x02E6539C = bla cameraCollisionPredicateOverride
