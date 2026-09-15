using namespace SKSE;
using namespace SKSE::log;

namespace {
    // High enough that the whole animation resolves within a single rendered frame instead
    // of visibly playing out over a few - the point isn't "fast", it's "no perceptible
    // transition at all". Still goes through the engine's normal update loop underneath, so
    // any sound/script hook tied to a keyframe still fires, just all within that one frame.
    constexpr float kFastAnimationSpeed = 1000.0f;

    // Practically instant without being exactly 0 - some engines treat a hard zero as a
    // degenerate case in fade/easing math, so this stays just on the safe side of that.
    // Used for fades going INTO a loading screen.
    constexpr float kInstantFadeSeconds = 0.0001f;

    // Fades coming OUT of a loading screen get this instead: cutting straight to a fully
    // rendered scene the instant loading finishes reads as a jarring flash rather than a
    // snappy transition, so these stay short but still perceptible.
    constexpr float kSlightFadeSeconds = 0.3f;

    // Doors/containers don't animate through the Havok behavior graph like actors do - they're
    // driven by plain NiControllerSequences hanging off the loaded 3D. frequency is the engine's
    // own playback-speed multiplier for one of those sequences, so cranking it is enough; we
    // don't need to touch whatever triggers the menu/loading screen once playback finishes.
    void SetAnimationSpeed(RE::TESObjectREFR& a_refr, float a_speedMultiplier) {
        auto* root = a_refr.Get3D();
        if (!root) {
            return;  // not currently loaded in (too far away, cell not attached, etc.)
        }

        for (auto controller = root->controllers.get(); controller; controller = controller->next.get()) {
            auto* manager = controller->AsNiControllerManager();
            if (!manager) {
                continue;
            }
            for (auto& sequence : manager->sequenceArray) {
                if (sequence) {
                    sequence->frequency = a_speedMultiplier;
                }
            }
        }
    }

    // A door only causes a loading screen when it actually hands off to a different cell -
    // e.g. the two sides of a "double door" prop are linked to each other but stay in the
    // same cell. ExtraDataType::kTeleport isn't a precise enough signal on its own: the game
    // tags plenty of same-cell doors as teleporting too (nudging the player a step to the other
    // side), so we resolve the real destination and compare cells directly instead.
    bool LeadsToADifferentCell(const RE::TESObjectREFR& a_door) {
        const auto* teleport = a_door.extraList.GetByType<RE::ExtraTeleport>();
        if (!teleport || !teleport->teleportData) {
            return false;
        }

        auto destination = teleport->teleportData->linkedDoor.get();
        return destination && destination->GetParentCell() != a_door.GetParentCell();
    }

    // Every setting the engine reads from Skyrim.ini is also a live RE::Setting object we can
    // read/write from code - no need to make users hand-edit a file themselves.
    void SetINIFloat(std::string_view a_settingName, float a_value) {
        auto* setting = RE::INISettingCollection::GetSingleton()->GetSetting(a_settingName);
        if (!setting) {
            log::warn("INI setting {} not found; leaving it at its default.", a_settingName);
            return;
        }

        setting->SetFloat(a_value);
        log::info("{} set to {}.", a_settingName, setting->GetFloat());
    }

    void ShortenLoadingTransitions() {
        for (auto settingName : {
                 "fNormalDoorFadeSecs:General"sv,
                 "fAutoDoorFadeSecs:General"sv,
                 "fNormalDoorFadeWait:General"sv,
                 "fLoadGameFadeSecs:General"sv,
                 "fFastTravelFadeSecs:General"sv,
             }) {
            SetINIFloat(settingName, kInstantFadeSeconds);
        }

        for (auto settingName : {"fFadeToBlackFadeSeconds:General"sv, "fMinSecondsForLoadFadeIn:General"sv}) {
            SetINIFloat(settingName, kSlightFadeSeconds);
        }
    }

    // Fires whenever a reference in the world gets activated - opening a door, opening a
    // container, talking to an NPC, picking up an item, etc.
    class ActivationEventSink final : public RE::BSTEventSink<RE::TESActivateEvent> {
    public:
        static ActivationEventSink* GetSingleton() {
            static ActivationEventSink singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* a_event,
                                               RE::BSTEventSource<RE::TESActivateEvent>*) override {
            if (!a_event || !a_event->objectActivated) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // NPCs open doors/containers as part of AI packages that may expect the animation
            // to actually take some time; only the player's own interactions get sped up.
            if (!a_event->actionRef || !a_event->actionRef->IsPlayerRef()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* refr = a_event->objectActivated.get();
            auto* baseObject = refr->GetBaseObject();
            if (!baseObject) {
                return RE::BSEventNotifyControl::kContinue;
            }

            if (baseObject->As<RE::TESObjectDOOR>()) {
                log::info("Door activated: {}", refr->GetName());
                if (LeadsToADifferentCell(*refr)) {
                    SetAnimationSpeed(*refr, kFastAnimationSpeed);
                }
            } else if (baseObject->As<RE::TESObjectCONT>()) {
                log::info("Container activated: {}", refr->GetName());
                SetAnimationSpeed(*refr, kFastAnimationSpeed);
            }

            return RE::BSEventNotifyControl::kContinue;
        }

        ActivationEventSink(const ActivationEventSink&) = delete;
        ActivationEventSink(ActivationEventSink&&) = delete;
        ActivationEventSink& operator=(const ActivationEventSink&) = delete;
        ActivationEventSink& operator=(ActivationEventSink&&) = delete;

    private:
        ActivationEventSink() = default;
        ~ActivationEventSink() override = default;
    };

    // Initialize spdlog. Writes to a debugger console when attached (e.g. from VS Code/Visual Studio),
    // otherwise writes to Documents/My Games/Skyrim Special Edition/SKSE/<PluginName>.log
    void InitializeLogging() {
        auto path = log_directory();
        if (!path) {
            stl::report_and_fail("Unable to lookup SKSE logs directory.");
        }
        *path /= PluginDeclaration::GetSingleton()->GetName();
        *path += L".log";

        std::shared_ptr<spdlog::logger> log;
        if (IsDebuggerPresent()) {
            log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::msvc_sink_mt>());
        } else {
            log = std::make_shared<spdlog::logger>(
                "Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
        }

        log->set_level(spdlog::level::trace);
        log->flush_on(spdlog::level::trace);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    }

    void InitializeMessaging() {
        if (!GetMessagingInterface()->RegisterListener([](MessagingInterface::Message* message) {
                switch (message->type) {
                    case MessagingInterface::kDataLoaded:
                        // All ESM/ESL/ESP plugins have loaded and the main menu is active.
                        // It's now safe to look up forms, records, etc.
                        log::info("Data loaded.");
                        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESActivateEvent>(
                            ActivationEventSink::GetSingleton());
                        break;
                    case MessagingInterface::kNewGame:
                        log::info("New game started.");
                        break;
                    default:
                        break;
                }
            })) {
            stl::report_and_fail("Unable to register message listener.");
        }
    }
}

SKSEPluginLoad(const LoadInterface* skse) {
    InitializeLogging();

    auto* plugin = PluginDeclaration::GetSingleton();
    log::info("{} {} is loading...", plugin->GetName(), plugin->GetVersion());

    Init(skse);
    ShortenLoadingTransitions();
    InitializeMessaging();

    log::info("{} has finished loading.", plugin->GetName());
    return true;
}
