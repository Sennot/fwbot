//
// Created by peony on 29.10.2024.
//

#ifndef FIX_HPP
#define FIX_HPP

#include <deque>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "Geode/binding/CheckpointGameObject.hpp"
#include "Geode/utils/function.hpp"
#include "checkpoint.hpp"
#include "settings/settings.hpp"
#include "shared/value/value.hpp"

class PracticeFix {
   private:
   public:
    struct SavedAdvRand {
        uint64_t* m_randomState;
        int m_uniqueID;
    };

    SLValuePtr<uint32_t> m_maxStoredFrames = SLValue<uint32_t>::create(
        "practice_fix.max_stored_frames", &SLSettings::get()->stepsToSave);

    SavedCheckpoint* m_forcedState;

    bool m_loadCheckpoint = false;

    std::deque<SavedCheckpoint> m_storedFrames;
    bool m_hasDiedNormally = false;
    bool m_isBackstep = false;
    std::vector<SavedCheckpoint> m_savedCheckpoints;
    std::deque<std::pair<CheckpointObject*, CheckpointGameObject*>>
        m_platformerCheckpoints;

    std::vector<SavedAdvRand> m_advancedRandom;

    // CheckpointObject* m_platformerCheckpoint = nullptr;
    // CheckpointGameObject* m_platformerCheckpointGame = nullptr;

    std::vector<GameObject*> m_brokenObjects;

    SavedCheckpoint createCheckpoint(CheckpointObject* obj,
                                     uint64_t attemptStartFrame);
    void applyCheckpoint(SavedCheckpoint& cp);

    bool canRestoreState();
    void saveState(CheckpointObject* obj, uint64_t attemptStartFrame);
    void clearStoredFrames();
    void restorePreviousFrame(std::function<void(CheckpointObject*)> loadFn);
    void resetWithState(const SavedCheckpoint& state);
    void dropLastStoredFrame();

    void saveCurrent(CheckpointObject* obj, uint64_t attemptStartFrame);
    void applyLatest();
    void popLatest();
    void removeAll();
    void clearPlatformer(bool assumeLoaded = false);

    void reseedAdvancedRandom(uint64_t attemptSeed);

    void updatePlatformerInputs(std::vector<PlayerButtonCommand>& inputs);
    void registerBrokenObject(GameObject* obj) {
        m_brokenObjects.push_back(obj);
    }

    bool m_p1Left = false;
    bool m_p1Right = false;

    bool m_p2Left = false;
    bool m_p2Right = false;

    bool m_shouldLoadPlatformer = false;
};

#endif  // FIX_HPP
