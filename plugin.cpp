#include "PCH.h"
#include "SIF_API.h"
#include <unordered_set>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <chrono>

struct Settings {
    float delay{ 0.0f };
    bool allCorpses{ true };
    std::uint32_t toggleKey{ RE::BSKeyboardDevice::Keys::kF8 };

    void Load() {
        const auto path = std::filesystem::path{"Data/SKSE/Plugins/UnsearchedCorpsesIndicator.ini"};
        if (!std::filesystem::exists(path)) return;

        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;

            auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            auto key = line.substr(0, eq);
            auto val = line.substr(eq + 1);

            // trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            val.erase(0, val.find_first_not_of(" \t"));
            val.erase(val.find_last_not_of(" \t\r\n") + 1);

            if (key == "fDelay") delay = std::max(0.0f, std::stof(val));
            else if (key == "bAllCorpses") allCorpses = (val == "1" || val == "true");
            else if (key == "iToggleKey") toggleKey = static_cast<std::uint32_t>(std::stoul(val));
        }
    }
};

static Settings g_settings;
static std::unordered_map<RE::FormID, float> g_deathTimes; // FormID -> death time
static std::unordered_map<RE::FormID, float> g_playerKilled; // FormID -> kill time
static std::unordered_set<RE::FormID> g_searched;
static bool g_iconsEnabled = true;

namespace Serialization {
    constexpr std::uint32_t FourCC(char a, char b, char c, char d) {
        return static_cast<std::uint32_t>(a) |
            (static_cast<std::uint32_t>(b) << 8) |
            (static_cast<std::uint32_t>(c) << 16) |
            (static_cast<std::uint32_t>(d) << 24);
    }

    constexpr std::uint32_t kUniqueID = FourCC('U', 'C', 'I', 'S');
    constexpr std::uint32_t kSearchedRecord = FourCC('S', 'R', 'C', 'H');
    constexpr std::uint32_t kSearchedVersion = 1;

    void Save(SKSE::SerializationInterface* intfc) {
        if (!intfc || !intfc->OpenRecord(kSearchedRecord, kSearchedVersion)) return;

        const auto count = static_cast<std::uint32_t>(g_searched.size());
        if (!intfc->WriteRecordData(count)) return;

        for (const auto formID : g_searched) {
            if (!intfc->WriteRecordData(formID)) return;
        }
    }

    void Load(SKSE::SerializationInterface* intfc) {
        g_deathTimes.clear();
        g_playerKilled.clear();
        g_searched.clear();
        g_iconsEnabled = true;

        if (!intfc) return;

        std::uint32_t type = 0;
        std::uint32_t version = 0;
        std::uint32_t length = 0;
        while (intfc->GetNextRecordInfo(type, version, length)) {
            if (type != kSearchedRecord || version != kSearchedVersion) {
                continue;
            }

            std::uint32_t count = 0;
            if (intfc->ReadRecordData(count) != sizeof(count)) {
                return;
            }

            for (std::uint32_t i = 0; i < count; ++i) {
                RE::FormID oldFormID = 0;
                if (intfc->ReadRecordData(oldFormID) != sizeof(oldFormID)) {
                    return;
                }

                RE::FormID resolvedFormID = 0;
                if (intfc->ResolveFormID(oldFormID, resolvedFormID)) {
                    g_searched.insert(resolvedFormID);
                }
            }
        }
    }

    void Revert(SKSE::SerializationInterface*) {
        g_deathTimes.clear();
        g_playerKilled.clear();
        g_searched.clear();
        g_iconsEnabled = true;
    }

    void Install() {
        const auto* intfc = SKSE::GetSerializationInterface();
        if (!intfc) return;

        intfc->SetUniqueID(kUniqueID);
        intfc->SetSaveCallback(Save);
        intfc->SetLoadCallback(Load);
        intfc->SetRevertCallback(Revert);
    }
}

static float GetRealTime() {
    using clock = std::chrono::steady_clock;
    static auto start = clock::now();
    return std::chrono::duration<float>(clock::now() - start).count();
}

static bool HasVisibleInventoryEntry(const RE::InventoryEntryData* entry, std::int32_t count) {
    if (!entry || !entry->object || count <= 0) return false;

    return entry->countDelta > 0;
}

static bool MatchesCorpseScope(RE::TESObjectREFR* ref) {
    if (!ref) return false;

    if (g_settings.allCorpses) {
        if (g_settings.delay > 0.0f) {
            auto it = g_deathTimes.find(ref->GetFormID());
            if (it != g_deathTimes.end()) {
                float elapsed = GetRealTime() - it->second;
                if (elapsed < g_settings.delay) return false;
            }
        }

        return true;
    }

    auto it = g_playerKilled.find(ref->GetFormID());
    if (it == g_playerKilled.end()) return false;

    if (g_settings.delay > 0.0f) {
        float elapsed = GetRealTime() - it->second;
        if (elapsed < g_settings.delay) return false;
    }

    return true;
}

class InputSink : public RE::BSTEventSink<RE::InputEvent*> {
public:
    static InputSink* GetSingleton() {
        static InputSink instance;
        return &instance;
    }

    RE::BSEventNotifyControl ProcessEvent(
        RE::InputEvent* const* events,
        RE::BSTEventSource<RE::InputEvent*>*) override
    {
        if (!events || g_settings.toggleKey == RE::BSKeyboardDevice::Keys::kNone)
            return RE::BSEventNotifyControl::kContinue;

        for (auto* event = *events; event; event = event->next) {
            if (event->GetDevice() != RE::INPUT_DEVICE::kKeyboard)
                continue;

            auto* button = event->AsButtonEvent();
            if (!button || !button->IsDown())
                continue;

            if (button->GetIDCode() == g_settings.toggleKey) {
                g_iconsEnabled = !g_iconsEnabled;
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

class DeathSink : public RE::BSTEventSink<RE::TESDeathEvent> {
public:
    static DeathSink* GetSingleton() {
        static DeathSink instance;
        return &instance;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESDeathEvent* event,
        RE::BSTEventSource<RE::TESDeathEvent>*) override
    {
        if (!event || !event->actorDying)
            return RE::BSEventNotifyControl::kContinue;

        auto* dying = event->actorDying.get();
        if (!dying)
            return RE::BSEventNotifyControl::kContinue;

        const auto deathTime = GetRealTime();
        g_deathTimes.insert_or_assign(dying->GetFormID(), deathTime);

        auto* killer = event->actorKiller.get();
        if (killer && killer->IsPlayerRef()) {
            g_playerKilled.insert_or_assign(dying->GetFormID(), deathTime);
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

class ActivateSink : public RE::BSTEventSink<RE::TESActivateEvent> {
public:
    static ActivateSink* GetSingleton() {
        static ActivateSink instance;
        return &instance;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESActivateEvent* event,
        RE::BSTEventSource<RE::TESActivateEvent>*) override
    {
        if (!event || !event->objectActivated || !event->actionRef)
            return RE::BSEventNotifyControl::kContinue;

        auto* activator = event->actionRef.get();
        if (!activator || !activator->IsPlayerRef())
            return RE::BSEventNotifyControl::kContinue;

        auto* target = event->objectActivated.get();
        if (!target) return RE::BSEventNotifyControl::kContinue;

        auto* actor = target->As<RE::Actor>();
        if (actor && actor->IsDead())
            g_searched.insert(target->GetFormID());

        return RE::BSEventNotifyControl::kContinue;
    }
};

class ContainerChangedSink : public RE::BSTEventSink<RE::TESContainerChangedEvent> {
public:
    static ContainerChangedSink* GetSingleton() {
        static ContainerChangedSink instance;
        return &instance;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESContainerChangedEvent* event,
        RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
    {
        if (!event || event->itemCount <= 0)
            return RE::BSEventNotifyControl::kContinue;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || event->newContainer != player->GetFormID())
            return RE::BSEventNotifyControl::kContinue;

        auto* source = RE::TESForm::LookupByID<RE::TESObjectREFR>(event->oldContainer);
        if (!source)
            return RE::BSEventNotifyControl::kContinue;

        auto* actor = source->As<RE::Actor>();
        if (actor && actor->IsDead())
            g_searched.insert(source->GetFormID());

        return RE::BSEventNotifyControl::kContinue;
    }
};

class HasLootCondition : public SIF::ICondition {
public:
    bool Match(RE::TESObjectREFR* ref) const override {
        if (!ref) return false;
        if (!g_iconsEnabled) return false;
        if (!MatchesCorpseScope(ref)) return false;

        auto inv = ref->GetInventory([](const RE::TESBoundObject& obj) {
            return obj.GetPlayable();
        });

        for (const auto& [obj, data] : inv) {
            if (obj && data.second && HasVisibleInventoryEntry(data.second.get(), data.first)) {
                return true;
            }
        }

        return false;
    }
};

class NotSearchedCondition : public SIF::ICondition {
public:
    bool Match(RE::TESObjectREFR* ref) const override {
        return ref && !g_searched.contains(ref->GetFormID());
    }
};

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    g_settings.Load();
    Serialization::Install();

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* msg) {
        if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
            holder->AddEventSink<RE::TESDeathEvent>(DeathSink::GetSingleton());
            holder->AddEventSink<RE::TESActivateEvent>(ActivateSink::GetSingleton());
            holder->AddEventSink<RE::TESContainerChangedEvent>(ContainerChangedSink::GetSingleton());

            auto* input = RE::BSInputDeviceManager::GetSingleton();
            if (input)
                input->AddEventSink(InputSink::GetSingleton());
        }
    });

    SIF::ListenForRegistration([](SKSE::MessagingInterface::Message* msg) {
        if (msg->type != SIF::kMessage_GetAPI) return;

        auto* api = static_cast<SIF::IAPI*>(msg->data);
        if (!api) return;

        api->RegisterCondition("hasLoot", [](const Json::Value&, RE::FormType) {
            return std::make_unique<HasLootCondition>();
        });
        api->RegisterCondition("notSearched", [](const Json::Value&, RE::FormType) {
            return std::make_unique<NotSearchedCondition>();
        });
    });

    return true;
}
