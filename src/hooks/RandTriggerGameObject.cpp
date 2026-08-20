#include <Geode/Geode.hpp>
#include <Geode/binding/RandTriggerGameObject.hpp>
#include <algorithm>

#include "../bot/bot.hpp"
#include "bot/updater.hpp"
#include "checkpoint/fix.hpp"
#include "replay/system.hpp"

using namespace geode::prelude;

#include <Geode/modify/RandTriggerGameObject.hpp>
#include <ranges>
#include <vector>

struct SLRandTriggerGameObject
    : Modify<SLRandTriggerGameObject, RandTriggerGameObject> {
    struct Fields {
        uint64_t m_randomState = std::numeric_limits<uint64_t>::max();
    };

    uint64_t takeNextRandomState() {
        uint64_t state = m_fields->m_randomState;
        m_fields->m_randomState = 214013 * state + 2531011;
        return state;
    }

    int generateValueWithMax(int max) {
        return static_cast<int>(roundf(
            static_cast<float>((this->takeNextRandomState() >> 16) & 0x7FFF) /
            32767.0 * static_cast<float>(max)));
    }

    void customObjectSetup(gd::vector<gd::string>& values,
                           gd::vector<void*>& exists) override {
        RandTriggerGameObject::customObjectSetup(values, exists);
        Bot::get()->practiceFix().m_advancedRandom.push_back({
            .m_randomState = &this->m_fields->m_randomState,
            .m_uniqueID = this->m_uniqueID,
        });
    }

    void triggerObject(GJBaseGameLayer* layer, int uniqueID,
                       gd::vector<int> const* remapKeys) override {
        if (m_objectID != 2068) {
            RandTriggerGameObject::triggerObject(layer, uniqueID, remapKeys);
            return;
        }

        if (m_fields->m_randomState == std::numeric_limits<uint64_t>::max()) {
            m_fields->m_randomState =
                Bot::get()->replaySystem().m_startingSeedThisAttempt ^
                (m_uniqueID * 2137);
        }

        int sum = std::ranges::fold_left(
            m_chanceObjects | std::views::transform([](const ChanceObject& cb) {
                return cb.m_chance;
            }),
            0, std::plus<int>());

        int threshold = this->generateValueWithMax(sum);
        int runningSum = 0, i = 0;

        int groupID = 0;

        while (runningSum < sum) {
            ChanceObject& co = m_chanceObjects[i++];
            runningSum += co.m_chance;
            groupID = co.m_groupID;

            if (runningSum >= threshold) break;
            if (static_cast<size_t>(i) == m_chanceObjects.size()) break;
        }

        std::vector<int> keys;
        if (remapKeys != nullptr) {
            keys = *remapKeys;
        } else {
            keys = {};
        }

        layer->spawnGroup(groupID, false, 0.0, keys, 0, 0);
    }
};
