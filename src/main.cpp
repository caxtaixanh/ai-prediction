// ─────────────────────────────────────────────────────────────────────────────
// Predictive Input Buffer — Android Port
// Original mod: ai.predictivebuffer by lilWC (Windows, GD 2.2081)
// Android adaptation: PauseLayer cycle button replaces the F6 key shortcut.
//
// Algorithm: trajectory-based 1-2 frame press + wave-release input buffer.
// Presses that arrive before an activatable object is in range are stored and
// fired at the exact tick when collision detection finds the object. A fallback
// fires the stored input after the configured tick limit so cube/ball jumps are
// never lost. Wave-mode releases are buffered symmetrically.
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

// ─── Stage table ─────────────────────────────────────────────────────────────
// Each stage stores {allTickLimit, waveTickLimit}.
// Stage 4 (index 4) is OFF — passes all input through unmodified.

struct Stage { int all, wave; };

static constexpr std::array<Stage, 5> kStages = {{
    { 2,  2 },   // Stage 1 — 0.5 f @ 60 fps
    { 4,  4 },   // Stage 2 — 1.0 f
    { 8,  8 },   // Stage 3 — 2.0 f
    {16,  8 },   // Stage 4 — 4.0 f all / 2.0 f wave
    { 0,  0 },   // Stage 5 — OFF
}};

static constexpr std::array<const char*, 5> kStageNames = {{
    "2 All / 2 Wave",
    "4 All / 4 Wave",
    "8 All / 8 Wave",
    "16 All / 8 Wave",
    "OFF (Raw Input)",
}};

// ─── Global session state ─────────────────────────────────────────────────────
namespace IB {

    int  stage     = 0;      // 0-4; 4 = off
    bool inLevel   = false;
    bool isEditor  = false;
    int  tick      = 0;      // physics ticks since attempt start, driven by p1::update
    bool replaying = false;  // prevents re-entry when we fire a buffered input

    // Per-attempt statistics shown at EndLevelLayer
    int saves      = 0;
    int savedTicks = 0;

    // Per-player pending input (one per player slot)
    struct Pending {
        bool active  = false;
        int  button  = 0;
        bool pushing = true;
        int  frame0  = 0;   // tick when the player's physical input arrived
    };
    Pending p1, p2;

    // Returns the tick limit for the current stage and gamemode
    int getLimit(bool wave) {
        if (stage >= 4) return 0;
        return wave ? kStages[stage].wave : kStages[stage].all;
    }

    // Resets per-attempt state (called on PlayLayer::init, destroyPlayer, onQuit)
    void resetAttempt() {
        tick = saves = savedTicks = 0;
        p1 = p2 = {};
    }

    // Fires a pending input, logs the save, clears the slot.
    // Must be called with the pending slot BEFORE calling handleButton to prevent
    // the hook from buffering the replayed call.
    void firePending(GJBaseGameLayer* gl, Pending& pend, bool isP1, bool wave) {
        if (!pend.active) return;

        int saved = tick - pend.frame0;
        if (saved > 0) {
            log::info(
                "[InputBuffer] *** SAVED +{} TICKS ({:.2f} frames @ 60fps) {}! "
                "(Frame0: {} -> Delayed: {}) [Limit: {}] ***",
                saved,
                static_cast<float>(saved) / 60.f,
                pend.pushing ? "PRESS" : "RELEASE",
                pend.frame0,
                tick,
                getLimit(wave)
            );
            saves++;
            savedTicks += saved;
        }

        int  btn  = pend.button;
        bool push = pend.pushing;
        pend = {};              // clear BEFORE calling handleButton

        replaying = true;
        gl->handleButton(push, btn, isP1);
        replaying = false;
    }

    // Advances to the next stage (wraps 0→4→0) and prints the new stage name
    void cycleStage() {
        stage = (stage + 1) % 5;
        if (stage < 4) {
            log::info(
                "[InputBuffer] Android Cycle -> STAGE {}: {} All | {} Wave ({:.1f}f / {:.1f}f @ 60fps)",
                stage + 1,
                kStages[stage].all,  kStages[stage].wave,
                kStages[stage].all  / 60.f,
                kStages[stage].wave / 60.f
            );
        } else {
            log::info("[InputBuffer] Android Cycle -> STAGE 5: OFF (100%% Raw Input)");
        }
    }

} // namespace IB

// ─── HUD helper ──────────────────────────────────────────────────────────────
// Shows a short fade-out label inside the play layer when the stage changes.
static void showStageHUD() {
    auto* pl = PlayLayer::get();
    if (!pl) return;

    if (auto* old = pl->getChildByTag(8310)) old->removeFromParent();

    std::string text = (IB::stage < 4)
        ? fmt::format("Buffer Stage {}: {}", IB::stage + 1, kStageNames[IB::stage])
        : std::string("Buffer: OFF (Raw Input)");

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

// ─── PlayLayer ────────────────────────────────────────────────────────────────
struct BufPlayLayer : Modify<BufPlayLayer, PlayLayer> {
    bool init(GJGameLevel* level) {
        if (!PlayLayer::init(level)) return false;

        IB::resetAttempt();
        IB::inLevel  = true;
        IB::isEditor = false;

        log::info(" [InputBuffer] v1.0.0 LOADED (Zero-Lag + Android Port)");
        log::info(" [InputBuffer] Starting Mode: STAGE {} ({})",
            IB::stage + 1, kStageNames[IB::stage]);
        log::info(" [InputBuffer] Tap [Buffer Sn] in the pause menu to cycle stages.");

        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // Flush buffers on death so they don't carry into the next attempt
        IB::p1 = IB::p2 = {};
        PlayLayer::destroyPlayer(player, obj);
    }

    void onQuit() {
        IB::inLevel = false;
        IB::resetAttempt();
        PlayLayer::onQuit();
    }

    void playEndAnimationToPos(CCPoint pos) {
        // Level complete — flush buffers
        IB::p1 = IB::p2 = {};
        PlayLayer::playEndAnimationToPos(pos);
    }
};

// ─── LevelEditorLayer ─────────────────────────────────────────────────────────
// Disable the buffer inside the editor to avoid interfering with playtesting.
struct BufEditor : Modify<BufEditor, LevelEditorLayer> {
    bool init(GJGameLevel* lvl, bool unk) {
        IB::isEditor = true;
        return LevelEditorLayer::init(lvl, unk);
    }
};

// ─── EndLevelLayer ────────────────────────────────────────────────────────────
// Displays per-attempt buffer statistics on the completion screen ("Impressive!" overlay).
struct BufEndLevel : Modify<BufEndLevel, EndLevelLayer> {
    void customSetup() {
        EndLevelLayer::customSetup();

        if (IB::saves > 0) {
            log::info(
                "[InputBuffer] Level complete! Buffer saved {} input(s) ({} total ticks).",
                IB::saves, IB::savedTicks
            );

            auto txt = fmt::format("Buffer: {}× saves / {} ticks", IB::saves, IB::savedTicks);
            auto* lbl = CCLabelBMFont::create(txt.c_str(), "goldFont.fnt");
            lbl->setScale(0.48f);

            auto ws = CCDirector::get()->getWinSize();
            lbl->setPosition({ws.width * 0.5f, 58.f});
            this->addChild(lbl, 10);
        }
    }
};

// ─── GJBaseGameLayer — core buffer logic ──────────────────────────────────────
struct BufBaseGameLayer : Modify<BufBaseGameLayer, GJBaseGameLayer> {

    // handleButton: intercepts physical button presses and releases.
    // Presses (and wave releases) are stored instead of forwarded immediately.
    // The buffer fires later from collisionCheckObjects (predictive) or
    // PlayerObject::update (tick-expiry fallback).
    void handleButton(bool pushing, int button, bool isPlayer1) {
        // Always pass through: replayed inputs, editor, buffer off
        if (IB::replaying || !IB::inLevel || IB::isEditor) {
            GJBaseGameLayer::handleButton(pushing, button, isPlayer1);
            return;
        }

        auto* pl = PlayLayer::get();
        PlayerObject* player = pl
            ? (isPlayer1 ? pl->m_player1 : pl->m_player2)
            : nullptr;

        bool wave  = player && (player->m_gamemode == IconType::Wave);
        int  limit = IB::getLimit(wave);

        // Buffer disabled for this gamemode/stage — pass straight through
        if (limit == 0) {
            GJBaseGameLayer::handleButton(pushing, button, isPlayer1);
            return;
        }

        auto& pend = isPlayer1 ? IB::p1 : IB::p2;

        // Already buffering this player — don't overwrite, let input through
        if (pend.active) {
            GJBaseGameLayer::handleButton(pushing, button, isPlayer1);
            return;
        }

        // Buffer presses always; buffer releases only in wave mode
        if (!pushing && !wave) {
            GJBaseGameLayer::handleButton(pushing, button, isPlayer1);
            return;
        }

        // Store and withhold — fired later from collision or update
        pend = { true, button, pushing, IB::tick };
    }

    // canBeActivatedByPlayer: pass-through (used internally by collisionCheckObjects).
    bool canBeActivatedByPlayer(PlayerObject* p, EffectGameObject* obj) {
        return GJBaseGameLayer::canBeActivatedByPlayer(p, obj);
    }

    // collisionCheckObjects: scans the nearby-object array each tick.
    // If any EffectGameObject in the array is currently activatable AND we have a
    // buffered input, fire the buffer NOW — before the normal collision pass runs.
    // This is the "predictive" firing path: zero additional lag relative to a
    // perfectly timed input.
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

                    // Found an activatable object — fire the buffered input
                    IB::firePending(this, pend, isP1, wave);
                    break;
                }
            }
        }

        GJBaseGameLayer::collisionCheckObjects(player, arr, p2, p3);
    }

    // playerTouchedRing: ring was physically touched — clear the buffer so the
    // stored input doesn't double-fire after the ring is already active.
    void playerTouchedRing(PlayerObject* player, RingObject* ring) {
        auto* pl = PlayLayer::get();
        if (pl) (player == pl->m_player1 ? IB::p1 : IB::p2) = {};
        GJBaseGameLayer::playerTouchedRing(player, ring);
    }
};

// ─── PlayerObject ─────────────────────────────────────────────────────────────
struct BufPlayerObject : Modify<BufPlayerObject, PlayerObject> {

    // update: called every physics tick.
    // Advances the global tick counter (via p1 only, once per tick).
    // Fires the pending buffer as a fallback if it has exceeded the tick limit
    // without the predictive path (collisionCheckObjects) having fired it.
    void update(float dt) {
        PlayerObject::update(dt);

        if (!IB::inLevel || IB::isEditor) return;

        auto* pl = PlayLayer::get();
        if (!pl) return;

        bool isP1 = (this == pl->m_player1);
        if (isP1) IB::tick++;   // one tick per physics step, driven by player 1

        auto& pend = isP1 ? IB::p1 : IB::p2;
        if (!pend.active) return;

        bool wave  = (m_gamemode == IconType::Wave);
        int  limit = IB::getLimit(wave);
        int  age   = IB::tick - pend.frame0;

        if (limit > 0 && age >= limit) {
            // Tick limit expired — fire as fallback (also covers cube/ball jumps)
            IB::firePending(static_cast<GJBaseGameLayer*>(pl), pend, isP1, wave);
        }
    }

    // incrementJumps: player jumped (cube, ball, etc.) — clear press buffer because
    // the jump consumed the input; no further activation needed.
    void incrementJumps() {
        auto* pl = PlayLayer::get();
        if (pl) (this == pl->m_player1 ? IB::p1 : IB::p2) = {};
        PlayerObject::incrementJumps();
    }

    // playSpiderDashEffect: spider dash fired — clear buffer.
    void playSpiderDashEffect(CCPoint from, CCPoint to) {
        auto* pl = PlayLayer::get();
        if (pl) (this == pl->m_player1 ? IB::p1 : IB::p2) = {};
        PlayerObject::playSpiderDashEffect(from, to);
    }

    // ringJump: the ring jump animation fired — clear buffer.
    void ringJump(RingObject* ring, bool unk) {
        auto* pl = PlayLayer::get();
        if (pl) (this == pl->m_player1 ? IB::p1 : IB::p2) = {};
        PlayerObject::ringJump(ring, unk);
    }
};

// ─── HardStreak (wave trail) ─────────────────────────────────────────────────
// Hooked to mirror the original mod's hook set. The wave trail fires every
// physics tick while in wave mode, which can be used to track wave trajectory.
struct BufHardStreak : Modify<BufHardStreak, HardStreak> {
    void addPoint(CCPoint pt) {
        HardStreak::addPoint(pt);
    }
};

// ─── GameObject ──────────────────────────────────────────────────────────────
// playShineEffect fires when a ring/pad activates (the visual "shine" burst).
// This is a reliable signal that an activation just happened — clear both
// buffers so neither player double-activates an already-triggered object.
struct BufGameObject : Modify<BufGameObject, GameObject> {
    void playShineEffect() {
        if (IB::inLevel && !IB::isEditor) IB::p1 = IB::p2 = {};
        GameObject::playShineEffect();
    }
};

// ─── EffectGameObject ─────────────────────────────────────────────────────────
// triggerObject fires when a portal/trigger activates. Pass-through; hooked to
// match the original mod's hook surface for future extension.
struct BufEffectGameObject : Modify<BufEffectGameObject, EffectGameObject> {
    void triggerObject(GJBaseGameLayer* gl, int p1, CCPoint const* p2) {
        EffectGameObject::triggerObject(gl, p1, p2);
    }
};

// ─── PauseLayer — Android cycle button ───────────────────────────────────────
// On PC the original mod uses F6 to cycle stages. On Android we inject a small
// "[ Buffer S1 – … ]" button at the bottom of the pause menu. Tapping it cycles
// to the next stage and shows a brief HUD notification inside the play layer.
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
        return fmt::format("[ Buffer S{} \u2013 {} ]",
            IB::stage + 1, kStageNames[IB::stage]);
    }
};
