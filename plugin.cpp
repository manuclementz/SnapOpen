using namespace SKSE;
using namespace SKSE::log;

namespace {
    // Fires whenever the player (or anything else) activates a reference in the world -
    // opening a door, opening a container, talking to an NPC, picking up an item, etc.
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

            auto* refr = a_event->objectActivated.get();
            auto* baseObject = refr->GetBaseObject();
            if (!baseObject) {
                return RE::BSEventNotifyControl::kContinue;
            }

            if (baseObject->As<RE::TESObjectDOOR>()) {
                log::info("Door activated: {}", refr->GetName());
            } else if (baseObject->As<RE::TESObjectCONT>()) {
                log::info("Container activated: {}", refr->GetName());
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
    InitializeMessaging();

    log::info("{} has finished loading.", plugin->GetName());
    return true;
}
