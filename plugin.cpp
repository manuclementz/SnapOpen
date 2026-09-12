using namespace SKSE;
using namespace SKSE::log;

namespace {
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
