// ─────────────────────────────────────────────────────────────────────────────
// Predictive Input Buffer — Android Port (Cleaned for Android NDK Compatibility)
// ─────────────────────────────────────────────────────────────────────────────

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <array>
#include <string>

using namespace geode::prelude;

struct Stage { int all, wave; };

static constexpr std::array<Stage, 5> kStages = {{
    { 2,  2 },
    { 4,  4 },
    { 8,  8 },
    {16,  8 },
    { 0,  0 },
}};

static constexpr std::array<const char*, 5> kStageNames = {{
    "2 All / 2 Wave",
    "4 All / 4 Wave",
    "8 All / 8 Wave",
    "16 All / 8 Wave",
    "OFF (Raw Input)",
}};

namespace IB {
    int  stage     = 0;
    bool inLevel   = false;
    bool isEditor  = false;
    int  tick      = 0;
    bool replaying = false;

    int saves      = 0;
    int savedTicks = 0;

    struct Pending {
        bool active  = false;
        int  button  = 0;
        bool pushing = true;
        int  frame0  = 0;
    };
    Pending p1, p2;

    int getLimit(bool wave) {
        if (stage >= 4) return 0;
        return wave ? kStages[stage].wave : kStages[stage].all;
    }

    void resetAttempt() {
        tick = saves = savedTicks = 0;
        p1 = p2 = {};
    }

    void firePending(GJBaseGameLayer* gl, Pending& pend, bool isP1, bool wave) {
        if (!pend.active) return;

        int saved = tick - pend.frame0;
        if (saved > 0) {
            log::info("[InputBuffer] SAVED TICKS!");
            saves++;
            savedTicks += saved;
        }

        int  btn  = pend.button;
        bool push = pend.pushing;
        pend = {};

        replaying = true;
        gl->handleButton(push, btn, isP1);
        replaying = false;
    }

    void cycleStage() {
        stage = (stage + 1) % 5;
        if (stage < 4) {
            log::info("[InputBuffer] Android Cycle Stage Changed");
        } else {
            log::info("[InputBuffer] Android Cycle -> STAGE 5: OFF");
        }
    }
}

static void showStageHUD() {
    auto* pl = PlayLayer::get();
    if (!pl) return;

    if (auto* old = pl->getChildByTag(8310)) old->removeFromParent();

    std::string text = (IB::stage < 4) ? "Buffer Stage Updated" : "Buffer: OFF";

    auto* lbl = CCLabelBMFont::create(text.c_str(), "chatFont.fnt");
    lbl->setScale(0.70f);
    lbl->setTag(8310);

    auto ws = CCDirector::get()->getWinSize();
    lbl->setPosition({ws.width * 0.5f, ws.height - 40.f});
    pl->addChild(lbl, 999);

    lbl->runAction(CCSequence::create(
        CCDelayTime::create(2.0f),
        CCFadeOut::create(0.35f),
        CCRemoveSelf::create(),
        nullptr
    ));
}

struct BufPlayLayer : Modify<BufPlayLayer, PlayLayer> {
    bool init(GJGameLevel* level) {
        if (!PlayLayer::init(level)) return false;

        IB::resetAttempt();
        IB::inLevel  = true;
        IB::isEditor = false;

        log::info("[InputBuffer] LOADED");
        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        IB::p1 = IB::p2 = {};
        PlayLayer::destroyPlayer(player, obj);
    }

    void onQuit() {
        IB::inLevel = false;
        IB::resetAttempt();
        PlayLayer::onQuit();
    }

    void playEndAnimationToPos(CCPoint pos) {
        IB::p1 = IB::p2 = {};
        PlayLayer::playEndAnimationToPos(pos);
    }
};

struct BufEditor : Modify<BufEditor, LevelEditorLayer> {
    bool init(GJGameLevel* lvl, bool unk) {
        IB::isEditor = true;
        return LevelEditorLayer::init(lvl, unk);
    }
};

struct BufEndLevel : Modify<BufEndLevel, EndLevelLayer> {
    void customSetup() {
        EndLevelLayer::customSetup();
        if (IB::saves > 0) {
            log::info("[InputBuffer] Level complete!");
        }
    }
};

struct BufBaseGameLayer : Modify<BufBaseGameLayer, GJBaseGameLayer> {
    void handleButton(bool pushing, int button, bool isPlayer1) {
        if (IB::replaying || !IB::inLevel || IB::isEditor) {
            GJBaseGameLayer::handleButton(pushing, button, isPlayer1);
            return;
        }

        auto* pl = PlayLayer::get();
        PlayerObject* player = pl ? (isPlayer1 ? pl->m_player1 : pl->m_player2) : nullptr;

        bool wave  = player && (player->m_gamemode == IconType::Wave);
        int  limit = IB::getLimit(wave);

        if (limit == 0) {
            GJBaseGameLayer::handleButton(pushing, button, isPlayer1);
            return;
        }

        auto& pend = isPlayer1 ? IB::p1 : IB::p2;
        if (pend.active) {
            GJBaseGameLayer::handleButton(pushing, button, isPlayer1);
            return;
        }

        if (!pushing && !wave) {
            GJBaseGameLayer::handleButton(pushing, button, isPlayer1);
            return;
        }

        pend = { true, button, pushing, IB::tick };
    }

    bool canBeActivatedByPlayer(PlayerObject* p, EffectGameObject* obj) {
        return GJBaseGameLayer::canBeActivatedByPlayer(p, obj);
    }

    void collisionCheckObjects(PlayerObject* player, CCArray* arr, float p2, float p3) {
        if (IB::inLevel && !IB::isEditor && arr && arr->count() > 0) {
            auto* pl  = PlayLayer::get();
            bool  isP1 = pl && (player == pl->m_player1);
            auto& pend = isP1 ? IB::p1 : IB::p2;

            if (pend.active) {
                bool wave = player && (player->m_gamemode == IconType::Wave);

                for (uint32_t i = 0; i < arr->count(); ++i) {
                    auto* obj = typeinfo_cast<EffectGameObject*>(arr->objectAtIndex(i));
                    if (!obj) continue;
                    if (!GJBaseGameLayer::canBeActivatedByPlayer(player, obj)) continue;

                    IB::firePending(this, pend, isP1, wave);
                    break;
                }
            }
        }

        GJBaseGameLayer::collisionCheckObjects(player, arr, p2, p3);
    }

    void playerTouchedRing(PlayerObject* player, RingObject* ring) {
        auto* pl = PlayLayer::get();
        if (pl) (player == pl->m_player1 ? IB::p1 : IB::p2) = {};
        GJBaseGameLayer::playerTouchedRing(player, ring);
    }
};

struct BufPlayerObject : Modify<BufPlayerObject, PlayerObject> {
    void update(float dt) {
        PlayerObject::update(dt);

        if (!IB::inLevel || IB::isEditor) return;

        auto* pl = PlayLayer::get();
        if (!pl) return;

        bool isP1 = (this == pl->m_player1);
        if (isP1) IB::tick++;

        auto& pend = isP1 ? IB::p1 : IB::p2;
        if (!pend.active) return;

        bool wave  = (m_gamemode == IconType::Wave);
        int  limit = IB::getLimit(wave);
        int  age   = IB::tick - pend.frame0;

        if (limit > 0 && age >= limit) {
            IB::firePending(static_cast<GJBaseGameLayer*>(pl), pend, isP1, wave);
        }
    }

    void incrementJumps() {
        auto* pl = PlayLayer::get();
        if (pl) (this == pl->m_player1 ? IB::p1 : IB::p2) = {};
        PlayerObject::incrementJumps();
    }

    void playSpiderDashEffect(CCPoint from, CCPoint to) {
        auto* pl = PlayLayer::get();
        if (pl) (this == pl->m_player1 ? IB::p1 : IB::p2) = {};
        PlayerObject::playSpiderDashEffect(from, to);
    }

    void ringJump(RingObject* ring, bool unk) {
        auto* pl = PlayLayer::get();
        if (pl) (this == pl->m_player1 ? IB::p1 : IB::p2) = {};
        PlayerObject::ringJump(ring, unk);
    }
};

struct BufHardStreak : Modify<BufHardStreak, HardStreak> {
    void addPoint(CCPoint pt) {
        HardStreak::addPoint(pt);
    }
};

struct BufGameObject : Modify<BufGameObject, GameObject> {
    void playShineEffect() {
        if (IB::inLevel && !IB::isEditor) IB::p1 = IB::p2 = {};
        GameObject::playShineEffect();
    }
};

struct BufEffectGameObject : Modify<BufEffectGameObject, EffectGameObject> {
    void triggerObject(GJBaseGameLayer* gl, int p1, CCPoint const* p2) {
        EffectGameObject::triggerObject(gl, p1, p2);
    }
};

struct BufPauseLayer : Modify<BufPauseLayer, PauseLayer> {
    struct Fields {
        CCLabelBMFont* cycleLabel = nullptr;
    };

    void customSetup() {
        PauseLayer::customSetup();

        auto* lbl = CCLabelBMFont::create(btnText().c_str(), "bigFont.fnt");
        lbl->setScale(0.45f);
        m_fields->cycleLabel = lbl;

        auto* btn = CCMenuItemExt::createSpriteExtra(lbl, [this](auto*) {
            IB::cycleStage();
            if (m_fields->cycleLabel)
                m_fields->cycleLabel->setString(btnText().c_str());
            showStageHUD();
        });

        auto* menu = CCMenu::create();
        menu->addChild(btn);

        auto ws = CCDirector::get()->getWinSize();
        menu->setPosition({ws.width * 0.5f, 22.f});
        this->addChild(menu, 10);
    }

    static std::string btnText() {
        return "[ Buffer Stage ]";
    }
};
