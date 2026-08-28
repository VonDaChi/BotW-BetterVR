[BetterVR_DisableGraphicalFeatures_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

0x039CFA10 = gsys_ModelSceneFx_drawVolumeMask:
0x039A9D28 = gsys_ModelScene_SyncWithGPUMaybe:
0x039AB3BC = drawProcedural_afterVolumeMask:
0x039AB3D0 = drawProcedural_afterGpuSync:


; God-rays
; Expensive because the game does roughly 172 draw calls to create this effect, and Cemu's CPU overhead is high for draw calls.

0x039AB3B8 = bl (($qualityPreset >= 2) * gsys_ModelSceneFx_drawVolumeMask + ($qualityPreset < 2) * drawProcedural_afterVolumeMask)
0x039AB3CC = bl (($qualityPreset >= 2) * gsys_ModelScene_SyncWithGPUMaybe + ($qualityPreset < 2) * drawProcedural_afterGpuSync)


; Depth shadows (cloud shadows aren't really that costly, since it doesn't require more draw calls)
; Expensive because the game has to draw the cascaded shadow maps for the sun/moon light multiple times. These calls are pretty cheap, but plentiful enough to matter.
; Patches gsys::ModelSceneShadow::isDepthShadowEnabled to return false when the quality preset is 0

stub_skipDepthShadow:
li r9, 0
li r12, $qualityPreset
cmpwi r12, 0
beq depthShadow_ret

lwz r12, 4(r3)
lbz r10, 0x730(r12)
cmpwi r10, 0
beq depthShadow_ret
lbz r0, 0x490(r12)
cmpwi r0, 0
beq depthShadow_ret
li r9, 1

depthShadow_ret:
clrlwi r3, r9, 24
blr

0x039DDFC4 = ba stub_skipDepthShadow
