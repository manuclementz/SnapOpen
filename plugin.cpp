using namespace SKSE;
using namespace SKSE::log;

namespace {
    // perceived as instant in game
    constexpr float kFastAnimationSpeed = 1000.0f;

    // 0 might cause issues, using small value instead
    constexpr float kInstantFadeSeconds = 0.0001f;

    // fade-in after loading
    constexpr float kSlightFadeSeconds = 0.15f;

    struct Config {
        bool overrideFadeSettings = true;
        bool experimentalNpcDoors = false;
    };

    Config g_config;

    void LoadConfig() {
        auto path = std::filesystem::path("Data/SKSE/Plugins") /
                    (std::string(PluginDeclaration::GetSingleton()->GetName()) + ".ini");

        CSimpleIniA ini;
        ini.SetUnicode();
        if (ini.LoadFile(path.string().c_str()) < 0) {
            log::warn("no ini found at {}, using defaults", path.string());
            return;
        }

        g_config.overrideFadeSettings = ini.GetBoolValue("Settings", "bOverrideFadeSettings", g_config.overrideFadeSettings);
        g_config.experimentalNpcDoors = ini.GetBoolValue("Settings", "bExperimentalNPCDoors", g_config.experimentalNpcDoors);

        log::info("overrideFadeSettings={} experimentalNpcDoors={}", g_config.overrideFadeSettings, g_config.experimentalNpcDoors);
    }

    void SetAnimationSpeed(RE::TESObjectREFR& a_refr, float a_speedMultiplier) {
        auto* root = a_refr.Get3D();
        if (!root) {
            return;
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

    bool LeadsToADifferentCell(const RE::TESObjectREFR& a_door) {
        const auto* teleport = a_door.extraList.GetByType<RE::ExtraTeleport>();
        if (!teleport || !teleport->teleportData) {
            return false;
        }

        auto destination = teleport->teleportData->linkedDoor.get();
        return destination && destination->GetParentCell() != a_door.GetParentCell();
    }

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

            const bool isPlayer = a_event->actionRef && a_event->actionRef->IsPlayerRef();

            auto* refr = a_event->objectActivated.get();
            auto* baseObject = refr->GetBaseObject();
            if (!baseObject) {
                return RE::BSEventNotifyControl::kContinue;
            }

            if (baseObject->As<RE::TESObjectDOOR>()) {
                log::info("Door activated: {}", refr->GetName());
                if ((isPlayer || g_config.experimentalNpcDoors) && LeadsToADifferentCell(*refr)) {
                    SetAnimationSpeed(*refr, kFastAnimationSpeed);
                }
            } else if (baseObject->As<RE::TESObjectCONT>()) {
                log::info("Container activated: {}", refr->GetName());
                if (isPlayer) {
                    SetAnimationSpeed(*refr, kFastAnimationSpeed);
                }
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
    LoadConfig();
    if (g_config.overrideFadeSettings) {
        ShortenLoadingTransitions();
    }
    InitializeMessaging();

    log::info("{} has finished loading.", plugin->GetName());
    return true;
}
