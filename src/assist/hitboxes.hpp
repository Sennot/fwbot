#include <Geode/Geode.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <deque>

#include "settings/settings.hpp"
#include "shared/value/value.hpp"

struct HitboxTrailUnit {
    cocos2d::CCRect m_rect;
    cocos2d::CCRect m_scaled;
    std::array<cocos2d::CCPoint, 4> m_rotated;

    // inline this shit
    inline bool shouldDraw(float minX, float maxX, float minY, float maxY);
    void draw(cocos2d::CCDrawNode* node, float width, float* colors,
              float fillOpacity);
    void drawRotated(cocos2d::CCDrawNode* node, float width, float* colors,
                     float fillOpacity);
    void drawInner(cocos2d::CCDrawNode* node, float width, float* colors,
                   float fillOpacity);
    void drawCircle(cocos2d::CCDrawNode* node, float width, float* colors,
                    float fillOpacity);
};

class Hitboxes {
    class HitboxesDrawNode : public cocos2d::CCDrawNode {
       public:
        static HitboxesDrawNode* create() {
            HitboxesDrawNode* node = new HitboxesDrawNode();

            if (node && node->init()) {
                node->autorelease();
                node->m_bUseArea = false;
            } else {
                CC_SAFE_DELETE(node);
            }

            return node;
        }
    };

    cocos2d::CCNode* m_container = nullptr;

    HitboxesDrawNode* m_drawNode = nullptr;

    HitboxesDrawNode* m_solidTrailDrawNode = nullptr;
    HitboxesDrawNode* m_rotatedTrailDrawNode = nullptr;
    HitboxesDrawNode* m_innerTrailDrawNode = nullptr;
    HitboxesDrawNode* m_circleTrailDrawNode = nullptr;

    bool _enabled = false;
    std::deque<HitboxTrailUnit> m_trailP1;
    std::deque<HitboxTrailUnit> m_trailP2;

   public:
    bool m_initialized = false;

    SLValuePtr<bool> m_enabled =
        SLValue<bool>::create("hitboxes.enabled", &_enabled);
    SLValuePtr<bool> m_trailEnabled = SLValue<bool>::create(
        "hitboxes.trail_enabled", &SLSettings::get()->hitboxes.trailEnabled);
    SLValuePtr<bool> m_playerAbove = SLValue<bool>::create(
        "hitboxes.player_above", &SLSettings::get()->hitboxes.playerAbove);
    SLValuePtr<bool> m_rotatedAbove = SLValue<bool>::create(
        "hitboxes.rotated_above", &SLSettings::get()->hitboxes.rotatedAbove);
    SLValuePtr<double> m_width = SLValue<double>::create(
        "hitboxes.width", &SLSettings::get()->hitboxes.width);

    void init(GJBaseGameLayer* pl);

    // Lifts the parts of the game's HUD that would otherwise end up under the
    // hitbox layer. Safe to call whenever they get (re)created.
    void raiseGameUI(GJBaseGameLayer* pl);

    // Lifts a single node above the hitbox layer
    void raiseAbove(cocos2d::CCNode* node);

    void clearTrail();
    void saveToTrail(GJBaseGameLayer* pl);

    void draw(GJBaseGameLayer* pl);
    void destroy();
};
