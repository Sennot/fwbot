#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include <Geode/modify/CCNode.hpp>
#include <Geode/modify/GameObject.hpp>

#include "bot/bot.hpp"
#include "bot/updater.hpp"

static const std::unordered_set<int> OTHER_DECO_IDS = {
    902,  943,  944,  945,  946,  947,  948,  949,  950,  951,
    980,  981,  982,  983,  984,  985,  986,  987,  988,  1023,
    1024, 1025, 1026, 1027, 1028, 1029, 1030, 1031, 1032, 1063,
    1064, 1065, 1066, 1067, 1068, 1069, 1070, 1071};

struct SLGameObject : Modify<SLGameObject, GameObject> {
    bool isNoHitboxObject() {
        auto rect = this->getObjectRect();

        return rect.getMinX() == rect.getMaxX() &&
               rect.getMinY() == rect.getMaxY();
    }

    void addGlow(gd::string p0) {
        GameObject::addGlow(std::move(p0));

        if (LevelEditorLayer::get()) return;

        if (Bot::get()->updater().m_layoutMode->inner()) {
            if (m_objectType == GameObjectType::Decoration || m_isNoTouch ||
                (m_objectID >= 506 && m_objectID <= 640) ||
                OTHER_DECO_IDS.contains(m_objectID)) {
                m_isHide = true;
            } else {
                m_isHide = false;
                m_isDontFade = true;
                m_isDontEnter = true;

                if (m_isPassable) {
                    m_opacityMod = 0.5;
                    m_opacityMod2 = 0.5;
                }
            }
        }
    }
};

// struct SLCCNode : Modify<SLCCNode, cocos2d::CCNode> {
//     void visit() override {
//         if (auto object = typeinfo_cast<GameObject*>(this)) {
//             if (
//                 Bot::get()->updater().m_layoutMode->inner()
//             ) {
//                 return;
//             }
//         }
//
//         CCNode::visit();
//     }
// };
//
// $execute {
//     Bot::get()->updater().m_layoutMode->handle([](bool& layout) {
//         if (auto pl = PlayLayer::get(); pl) {
//             geode::log::info("Revisiting {} objects",
//             pl->m_objects->count()); for (int i = 0; i <
//             pl->m_objects->count(); i++) {
//                 static_cast<GameObject*>(pl->m_objects->objectAtIndex(i))
//                     ->visit();
//             }
//         }
//     });
// }
//
// using GameObject_visit_T = void(*)(GameObject*);
// static GameObject_visit_T GameObject_visit_orig = nullptr;
// static void GameObject_visit(GameObject* self) {
//     geode::log::info("visit called!");
//     if (
//         Bot::get()->updater().m_layoutMode->inner()
//         && self->m_objectType == GameObjectType::Decoration
//     ) {
//         return;
//     }
//
//     GameObject_visit_orig(self);
// }
//
// $execute {
//     const auto vmtOffset =
//     geode::addresser::getVirtualOffset(&cocos2d::CCNode::visit); uintptr_t
//     vmtAddress = 0;
//     {
//         auto instance = GameObject();
//         vmtAddress = *reinterpret_cast<uintptr_t*>(&instance);
//     };
//     auto vmtEntry = reinterpret_cast<uintptr_t*>(vmtAddress + vmtOffset);
//
//     geode::log::info("Patching GameObject::update at {:x} (offset {:x})",
//     reinterpret_cast<uintptr_t>(vmtEntry), vmtOffset); GameObject_visit_orig
//     = reinterpret_cast<GameObject_visit_T>(*vmtEntry);
//     geode::log::info("GameObject::visit orig is at {:x}",
//     reinterpret_cast<uintptr_t>(GameObject_visit_orig));
//
//     if (auto result = Mod::get()->patch(vmtEntry,
//     geode::toBytes(&GameObject_visit)); result.isErr()) {
//         geode::log::error("Failed to patch GameObject::visit: {}",
//         result.err());
//     }
//
//     geode::log::info("Wrote {} at {:x}", geode::toBytes(&GameObject_visit),
//     reinterpret_cast<uintptr_t>(vmtEntry));
// }
