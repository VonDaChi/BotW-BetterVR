[BetterVR_STUB_StereoRenderingDebugging_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

useStubHooks:
.int 0

useStubHooks_preventClearCalls:
.int 0

const_cameraHeightChange:
.float 0.5
.float -0.5
; shouldn't use this, but we might upgrade to a bigger frame counter
.float 0.5
.float -0.5

; r3 = layer->renderCamera
; r11 = currentEyeSide
; r12 = modifiedCopy_seadLookAtCamera
stub_hook_GetRenderCamera:
cmpwi r11, 0
beqlr ; don't modify right eye


lis r12, currentFrameCounter@ha
lwz r12, currentFrameCounter@l(r12)
cmpwi r12, 0
; camera.pos.y += 0.5 or -0.5 depending on the frame
lis r11, const_cameraHeightChange@ha
addi r11, r11, const_cameraHeightChange@l
beq .+0x08
addi r12, r12, 1
lfs f12, 0(r11)


; camera->pos.y += 2.0; 
lfs f12, 0x34+0x04(r3)
fadds f12, f12, f13
stfs f12, 0x34+0x04(r3)
; camera->at.y += 2.0;
lfs f12, 0x40+0x04(r3)
fadds f12, f12, f13
stfs f12, 0x40+0x04(r3)
; camera->mtx.pos_y += 2.0;
lfs f12, 0x1C(r3)
fadds f12, f12, f13
stfs f12, 0x1C(r3)

blr


example_PerspectiveProjectionMatrix:
.byte 1,1,0,0
; matrix
.float 0.9300732,-107374176,-107374176,-107374176
.float -107374176,0.86867154,-107374176,-107374176
.float 0.06992682,-0.0352425,-1.000008,-1
.float -107374176,-107374176,-0.2000008,0
; deviceMatrix
.float 0.9300732,-107374176,-107374176,-107374176
.float -107374176,0.86867154,-107374176,-107374176
.float 0.06992682,-0.0352425,-1.000008,-1
.float -107374176,-107374176,-0.2000008,0
; devicePosture
.int 0
; deviceZScale
.float 1
; deviceZOffset
.float 0
; lookAt vtable
.int 0x1027B54C
; near, far and fov
.float 0.1
.float 25000
.float 1.7104228
.float 0.7547096
.float 0.65605897
.float 1.1503685
.float 0.95918363
.float 0.034906596
.float -0.017453283

stub_hook_GetRenderProjection:
lis r3, example_PerspectiveProjectionMatrix@ha
addi r3, r3, example_PerspectiveProjectionMatrix@l
blr
