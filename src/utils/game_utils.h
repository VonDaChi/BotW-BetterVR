#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hooking/cemu_hooks.h"

class GameUtils {
public:
    static std::string GetString(uint32_t stringPtr) {
        if (stringPtr == 0) {
            return {};
        }

        return std::string(reinterpret_cast<char*>(CemuHooks::s_memoryBaseAddress + stringPtr));
    }

    static std::string GetActionName(uint32_t actionPtr) {
        if (actionPtr == 0) {
            return {};
        }

        constexpr uint32_t kActionDefinitionIdxOffset = 0x08;
        constexpr uint32_t kActionRootIdxOffset = 0x0A;
        constexpr uint32_t kActionVtblOffset = 0x0C;
        constexpr uint32_t kActorAiProgramHolderOffset = 0x39C;
        constexpr uint32_t kAiProgramPtrOffset = 0x74;
        constexpr uint32_t kAiProgramActionsOffset = 0x24C;
        constexpr uint32_t kAiProgramAIsOffset = 0x254;
        constexpr uint32_t kActionEntrySize = 0x68;
        constexpr uint32_t kActionEntryNameOffset = 0x28;
        constexpr uint32_t kExpectedGetCurrentName = 0x030EADDC;
        constexpr uint32_t kRootNameDemo = 0x1028838C;
        constexpr uint32_t kRootNameRoot = 0x10288208;

        uint32_t actionVtbl = CemuHooks::getMemory<BEType<uint32_t>>(actionPtr + kActionVtblOffset).getLE();
        if (actionVtbl == 0) {
            return {};
        }

        uint32_t getCurrentNameFn = CemuHooks::getMemory<BEType<uint32_t>>(actionVtbl + 0xAC).getLE();
        if (getCurrentNameFn != kExpectedGetCurrentName) {
            return {};
        }

        int16_t definitionIdx = CemuHooks::getMemory<BEType<int16_t>>(actionPtr + kActionDefinitionIdxOffset).getLE();
        if (definitionIdx < 0) {
            int8_t rootIdx = CemuHooks::getMemory<BEType<int8_t>>(actionPtr + kActionRootIdxOffset).getLE();
            if (rootIdx < 0) {
                return {};
            }

            uint32_t rootNamePtr = rootIdx == 0 ? kRootNameDemo : kRootNameRoot;
            return GetString(rootNamePtr);
        }

        uint32_t actorPtr = CemuHooks::getMemory<BEType<uint32_t>>(actionPtr).getLE();
        if (actorPtr == 0) {
            return {};
        }

        uint32_t aiProgramHolderPtr = CemuHooks::getMemory<BEType<uint32_t>>(actorPtr + kActorAiProgramHolderOffset).getLE();
        if (aiProgramHolderPtr == 0) {
            return {};
        }

        uint32_t aiProgramPtr = CemuHooks::getMemory<BEType<uint32_t>>(aiProgramHolderPtr + kAiProgramPtrOffset).getLE();
        if (aiProgramPtr == 0) {
            return {};
        }

        auto getNameFromProgramTable = [&](uint32_t tableOffset, uint16_t entryIndex) -> std::string {
            uint32_t actionsOrAIsPtr = aiProgramPtr + tableOffset;
            uint32_t count = CemuHooks::getMemory<BEType<uint32_t>>(actionsOrAIsPtr).getLE();
            uint32_t entriesPtr = CemuHooks::getMemory<BEType<uint32_t>>(actionsOrAIsPtr + 0x04).getLE();
            if (entriesPtr == 0 || entryIndex >= count) {
                return {};
            }

            uint32_t entryPtr = entriesPtr + static_cast<uint32_t>(entryIndex) * kActionEntrySize;
            uint32_t namePtr = CemuHooks::getMemory<BEType<uint32_t>>(entryPtr + kActionEntryNameOffset).getLE();
            return ReadLikelyName(namePtr);
        };

        uint16_t entryIndex = static_cast<uint16_t>(definitionIdx);

        if (std::string name = getNameFromProgramTable(kAiProgramActionsOffset, entryIndex); !name.empty()) {
            return name;
        }

        return getNameFromProgramTable(kAiProgramAIsOffset, entryIndex);
    }

    struct AnimationSlot {
        uint32_t groupIndex;
        uint32_t slotIndex;
        uint32_t slotPtr;
        std::string name;
    };

    static std::vector<AnimationSlot> GetAnimationSlots(uint32_t asListPtr) {
        std::vector<AnimationSlot> animations;
        if (asListPtr == 0) {
            return animations;
        }

        sead::Buffer groups = CemuHooks::getMemory<sead::Buffer>(asListPtr + offsetof(ASListPartial, slotGroups));
        const uint32_t groupsPtr = groups.data.getLE();
        const int32_t groupCount = groups.size.getLE();
        if (groupsPtr == 0) {
            return animations;
        }

        for (int32_t groupIdx = 0; groupIdx < groupCount; groupIdx++) {
            uint32_t groupPtr = groupsPtr + static_cast<uint32_t>(groupIdx) * sizeof(ASListSlotGroup);
            sead::Buffer slots = CemuHooks::getMemory<sead::Buffer>(groupPtr + offsetof(ASListSlotGroup, slots));
            const uint32_t slotsPtr = slots.data.getLE();
            const int32_t slotCount = slots.size.getLE();
            if (slotsPtr == 0) {
                continue;
            }

            for (int32_t slotIdx = 0; slotIdx < slotCount; slotIdx++) {
                uint32_t slotPtr = slotsPtr + static_cast<uint32_t>(slotIdx) * sizeof(ASListSlot);
                ASListSlot slot = CemuHooks::getMemory<ASListSlot>(slotPtr);
                if (slot.assignedCount.getLE() == 0 || slot.definitionPtr.getLE() == 0) {
                    continue;
                }

                sead::SafeString name = CemuHooks::getMemory<sead::SafeString>(slot.definitionPtr.getLE() + offsetof(ASListDefinitionPartial, name));
                std::string animationName = ReadLikelyName(name.c_str.getLE());
                if (animationName.empty()) {
                    continue;
                }

                animations.emplace_back(static_cast<uint32_t>(groupIdx), static_cast<uint32_t>(slotIdx), slotPtr, std::move(animationName));
            }
        }

        return animations;
    }

    static bool TryReadPlayerBase(PlayerBase& player) {
        if (CemuHooks::s_playerAddress == 0)
            return false;

        CemuHooks::readMemory(CemuHooks::s_playerAddress, &player);
        return true;
    }


    static uint32_t ResolveActorPhysicsController(const ActorWiiU& actor) {
        uint32_t actorPhysics = actor.actorPhysicsPtr.getLE();
        if (actorPhysics == 0)
            return 0;

        return CemuHooks::getMemory<BEType<uint32_t>>(actorPhysics + 0x50).getLE();
    }

    static bool TryGetPlayerCharacterController(uint32_t& controller) {
        PlayerBase player;
        return TryReadPlayerBase(player) && (controller = ResolveActorPhysicsController(player)) != 0;
    }

    static bool IsTrackedPlayerBody(uint32_t rigidBodyAddr) {
        if (rigidBodyAddr == 0)
            return false;

        PlayerBase player;
        if (!TryReadPlayerBase(player))
            return false;

        if (rigidBodyAddr == player.physicsTgtBodyPtr.getLE())
            return true;

        uint32_t mainBody = player.physicsMainBodyPtr.getLE();
        uint32_t controller = ResolveActorPhysicsController(player);
        if (controller != 0) {
            uint32_t controllerBody = CemuHooks::getMemory<BEType<uint32_t>>(controller + 0x04).getLE();
            if (controllerBody != 0) {
                mainBody = controllerBody;
            }
        }

        return rigidBodyAddr == mainBody;
    }

private:
    static std::string ReadLikelyName(uint32_t namePtr) {
        if (namePtr == 0) {
            return {};
        }

        std::string name;
        name.reserve(64);

        for (uint32_t i = 0; i < 64; i++) {
            char c = *reinterpret_cast<char*>(CemuHooks::s_memoryBaseAddress + namePtr + i);
            if (c == '\0') {
                break;
            }

            bool validChar = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
            if (!validChar) {
                return {};
            }

            name.push_back(c);
        }

        return name;
    }
};
