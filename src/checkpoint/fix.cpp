//
// Created by peony on 29.10.2024.
//

#include "fix.hpp"

#include <Geode/Geode.hpp>
#include <algorithm>
#include <vector>

#include "checkpoint/checkpoint.hpp"

using namespace geode::prelude;

#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/UILayer.hpp>

#include "bot/bot.hpp"
#include "bot/updater.hpp"
#include "replay/system.hpp"

// taken from https://stackoverflow.com/a/60971856
template <std::ranges::range R>
static constexpr auto to_vector(R&& r) {
    using elem_t = std::decay_t<std::ranges::range_value_t<R>>;
    return std::vector<elem_t>{r.begin(), r.end()};
}

SavedCheckpoint PracticeFix::createCheckpoint(CheckpointObject* obj,
                                              uint64_t attemptStartFrame) {
    if (auto pl = PlayLayer::get(); pl) {
        SavedCheckpoint checkpoint = {
            .m_player1 = SavedPlayerCheckpoint::create(pl->m_player1),
            .m_player2 = SavedPlayerCheckpoint::create(pl->m_player2),
            .m_checkpoint = obj,
            .m_stackSize = m_platformerCheckpoints.size(),
            .m_attemptStartFrame = attemptStartFrame,
            .m_frame = Bot::get()->updater().getFrame(),
            .m_seedState = Bot::get()->replaySystem().getCurrentRandomState(),
            .m_shakeRandomState = Bot::get()->replaySystem().m_shakeRandomState,
            .m_teleportRandomState =
                Bot::get()->replaySystem().m_teleportRandomState,
            .m_tps = Bot::get()->updater().m_tps->inner(),
            .m_timewarpTime = Bot::get()->updater().m_timewarpTime,
            .m_persistentItemMap =
                pl->m_effectManager->m_persistentItemCountMap,
            .m_varianceValues = pl->m_varianceValues,
            .m_calcNonEffectObjects = pl->m_calcNonEffectObjects,
            .m_calcNonEffectObjectsSize = pl->m_calcNonEffectObjectsSize,
            .m_brokenObjects = this->m_brokenObjects,
            .m_advRandStates = to_vector(
                m_advancedRandom | std::views::transform([](SavedAdvRand& s) {
                    return *s.m_randomState;
                })),
        };

        return checkpoint;
    }

    return {};
}

void PracticeFix::applyCheckpoint(SavedCheckpoint& cp) {
    if (auto pl = PlayLayer::get(); pl) {
        cp.m_player1.apply(pl->m_player1);
        cp.m_player2.apply(pl->m_player2);

        pl->m_effectManager->m_persistentItemCountMap = cp.m_persistentItemMap;
        pl->m_varianceValues = cp.m_varianceValues;

        pl->m_calcNonEffectObjects = cp.m_calcNonEffectObjects;
        pl->m_calcNonEffectObjectsSize = cp.m_calcNonEffectObjectsSize;

        while (cp.m_stackSize < m_platformerCheckpoints.size()) {
            // if platformer checkpoint is in stack earlier then don't actually
            // reset
            bool isEarlier = std::any_of(
                m_platformerCheckpoints.begin(),
                m_platformerCheckpoints.end() - 1, [this](const auto& pair) {
                    return pair.second ==
                           this->m_platformerCheckpoints.back().second;
                });

            if (!isEarlier) {
                m_platformerCheckpoints.back().second->m_checkpointActivated =
                    false;
                m_platformerCheckpoints.back().second->resetCheckpoint();
            }

            m_platformerCheckpoints.pop_back();
        }

        auto bot = Bot::get();
        auto& updater = bot->updater();
        updater.m_frameOnLastAttempt = cp.m_attemptStartFrame;
        updater.m_timewarpTime = cp.m_timewarpTime;
        updater.setFrame(cp.m_frame - cp.m_attemptStartFrame);
        bot->replaySystem().getCurrentRandomState() = cp.m_seedState;
        bot->replaySystem().m_shakeRandomState = cp.m_shakeRandomState;
        bot->replaySystem().m_teleportRandomState = cp.m_teleportRandomState;
        if (cp.m_tps != bot->updater().m_tps->inner()) {
            bot->updater().m_tps->inner() = cp.m_tps;
            bot->updater().m_tps->notifyChange();
        }

        this->m_brokenObjects = cp.m_brokenObjects;
        for (auto& obj : this->m_brokenObjects) {
            if (obj) {
                obj->m_isDisabled = true;
                obj->m_isDisabled2 = true;
                obj->setOpacity(0.0f);
            }
        }

        for (auto const [i, state] :
             cp.m_advRandStates | std::views::enumerate) {
            *this->m_advancedRandom[i].m_randomState = state;
        }
    }
}

void PracticeFix::saveState(CheckpointObject* obj, uint64_t attemptStartFrame) {
    auto checkpoint = createCheckpoint(obj, attemptStartFrame);

    uint64_t frame = Bot::get()->updater().getFrame();
    if (!m_storedFrames.empty() && m_storedFrames.back().m_frame == frame) {
        return;
    }

    while (m_storedFrames.size() >= m_maxStoredFrames->inner()) {
        m_storedFrames.front().m_checkpoint->release();
        m_storedFrames.pop_front();
    }

    m_storedFrames.push_back(checkpoint);
}

void PracticeFix::clearStoredFrames() {
    for (auto& frame : m_storedFrames) {
        frame.m_checkpoint->release();
    }
    m_storedFrames.clear();
}

bool PracticeFix::canRestoreState() {
    if (!PlayLayer::get()) return false;
    if (m_storedFrames.empty()) return false;

    return true;
}

void PracticeFix::resetWithState(const SavedCheckpoint& state) {
    m_forcedState = const_cast<SavedCheckpoint*>(&state);
    PlayLayer::get()->resetLevel();
    m_forcedState = nullptr;
}

void PracticeFix::restorePreviousFrame(
    std::function<void(CheckpointObject*)> loadFn) {
    if (m_storedFrames.empty()) return;

    SavedCheckpoint checkpoint = std::move(m_storedFrames.back());
    m_storedFrames.pop_back();

    loadFn(checkpoint.m_checkpoint);
    applyCheckpoint(checkpoint);

    if (checkpoint.m_checkpoint) {
        checkpoint.m_checkpoint->release();
    }
}

void PracticeFix::dropLastStoredFrame() {
    m_storedFrames.back().m_checkpoint->release();
    m_storedFrames.pop_back();
}

void PracticeFix::saveCurrent(CheckpointObject* obj,
                              uint64_t attemptStartFrame) {
    auto checkpoint = createCheckpoint(obj, attemptStartFrame);
    m_savedCheckpoints.push_back(checkpoint);
}

void PracticeFix::popLatest() {
    if (m_savedCheckpoints.empty()) {
        return;
    }

    m_savedCheckpoints.pop_back();
}

void PracticeFix::applyLatest() {
    if (m_savedCheckpoints.empty()) {
        clearPlatformer(true);
        return;
    }

    auto& checkpoint = m_savedCheckpoints.back();
    applyCheckpoint(checkpoint);
}

void PracticeFix::reseedAdvancedRandom(uint64_t attemptSeed) {
    for (auto& s : m_advancedRandom) {
        *s.m_randomState =
            attemptSeed ^ (static_cast<uint64_t>(s.m_uniqueID) * 2137);
    }
}

void PracticeFix::removeAll() {
    m_brokenObjects.clear();
    m_advancedRandom.clear();
    m_savedCheckpoints.clear();
}

void PracticeFix::clearPlatformer(bool assumeLoaded) {
    while (!m_platformerCheckpoints.empty()) {
        if (assumeLoaded) {
            m_platformerCheckpoints.back().second->m_checkpointActivated =
                false;
            m_platformerCheckpoints.back().second->resetCheckpoint();

            // release
            // m_platformerCheckpoints.top().first->release();
        }

        m_platformerCheckpoints.pop_back();
    }
}

void PracticeFix::updatePlatformerInputs(
    std::vector<PlayerButtonCommand>& inputs) {
    for (const auto& input : inputs) {
        bool& left = input.m_isPlayer2 ? m_p2Left : m_p1Left;
        bool& right = input.m_isPlayer2 ? m_p2Right : m_p1Right;

        if (input.m_button == PlayerButton::Left) {
            left = input.m_isPush;
        } else if (input.m_button == PlayerButton::Right) {
            right = input.m_isPush;
        }
    }
}

[[maybe_unused]] static std::optional<slc::Action> findLastInputUnused(
    const std::vector<slc::Action>& inputs, uint32_t frame, PlayerButton btn,
    bool p2) {
    const auto& last = std::find_if(
        inputs.rbegin(), inputs.rend(),
        [frame, btn, p2](const slc::Action& input) {
            if (input.m_frame < frame &&
                static_cast<int>(input.m_type) == static_cast<int>(btn) &&
                input.m_player2 == p2) {
                return true;
            }

            return false;
        });

    if (last == inputs.rend()) {
        return std::nullopt;
    }

    return *last;
}

$execute {
    Bot::get()->practiceFix().m_maxStoredFrames->handle([](uint32_t& frames) {
        if (frames <= 0 || frames > 100'000) {
            frames = 1;
        }

        auto& storedFrames = Bot::get()->practiceFix().m_storedFrames;
        while (storedFrames.size() > frames) {
            storedFrames.front().m_checkpoint->release();
            storedFrames.pop_front();
        }
    });
}
