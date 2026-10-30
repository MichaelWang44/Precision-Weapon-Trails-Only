#include "Events.h"
#include "Hooks.h"
#include "ModAPI.h"
#include "Papyrus.h"
#include "PrecisionAPI.h"
#include "PrecisionHandler.h"
#include "Settings.h"
#include "render/DrawHandler.h"
#include "render/d3d_context.h"

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		Events::SinkEventHandlers();
		Settings::Initialize();
		Settings::ReadSettings();
		PrecisionHandler::GetSingleton()->Initialize();
		break;
	case SKSE::MessagingInterface::kPostLoad:
		break;
	case SKSE::MessagingInterface::kPreLoadGame:
		PrecisionHandler::GetSingleton()->OnPreLoadGame();

		break;
	case SKSE::MessagingInterface::kPostLoadGame:
	case SKSE::MessagingInterface::kNewGame:
		PrecisionHandler::AddPlayerSink();
		PrecisionHandler::GetSingleton()->OnPostLoadGame();
		Settings::OnPostLoadGame();
		break;
	}
}

namespace
{
	void InitializeLog()
	{
#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = logger::log_directory();
		if (!path) {
			util::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= fmt::format("{}.log"sv, Plugin::NAME);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
		const auto level = spdlog::level::trace;
#else
		const auto level = spdlog::level::info;
#endif

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION.pack();

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_SSE_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;

	v.PluginVersion(Plugin::VERSION);
	v.PluginName(Plugin::NAME);
	v.AuthorName("Ersh");
	v.UsesAddressLibrary(true);
	v.CompatibleVersions({ REL::Version{1, 6, 1170, 0} });
	v.UsesStructsPost629();

	return v;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
#ifndef NDEBUG
	while (!IsDebuggerPresent()) {
		Sleep(100);
	}
#endif

	InitializeLog();
	logger::info("{} v{}"sv, Plugin::NAME, Plugin::VERSION.string());


    if (a_skse->RuntimeVersion() != REL::Version{1, 6, 1170, 0}) {
        logger::critical("TrailsOnly requires Skyrim 1.6.1170");
        return false;
    }
    logger::info("Precision 2.0.4 - Weapon Trails Only (source e094209; no combat/physics hooks)");
	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 10);

	auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener("SKSE", MessageHandler)) {
		return false;
	}

    if (!Hooks::ValidateVisualHooks()) { return false; }
	Hooks::Install();
	Papyrus::Register();

	return true;
}

extern "C" DLLEXPORT void* SKSEAPI RequestPluginAPI(const PRECISION_API::InterfaceVersion a_interfaceVersion)
{
	// Workaround so the old version of Accuracy doesn't receive the API. Should've designed the API a bit differently. Too late now.
	HMODULE hModule = nullptr;
	void* retAddr = _ReturnAddress();
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)retAddr, &hModule);

	if (hModule) {
		SKSE::PluginVersionData* versionData = (SKSE::PluginVersionData*)GetProcAddress(hModule, "SKSEPlugin_Version");
		if (versionData && versionData->pluginName == "Accuracy"sv && versionData->pluginVersion < 0x20000) {
			return nullptr;
		}
	}

	auto api = Messaging::PrecisionInterface::GetSingleton();

	logger::info("Precision::RequestPluginAPI called, InterfaceVersion {}", static_cast<uint8_t>(a_interfaceVersion) + 1);

	switch (a_interfaceVersion) {
	case PRECISION_API::InterfaceVersion::V1:
		[[fallthrough]];
	case PRECISION_API::InterfaceVersion::V2:
		[[fallthrough]];
	case PRECISION_API::InterfaceVersion::V3:
		[[fallthrough]];
	case PRECISION_API::InterfaceVersion::V4:
		logger::info("Precision::RequestPluginAPI returned the API singleton");
		return static_cast<void*>(api);
	}

	logger::info("Precision::RequestPluginAPI requested the wrong interface version");
	return nullptr;
}
