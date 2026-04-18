#include "PCH.h"
#include "SIF_API.h"
#include <unordered_set>

static std::unordered_set<RE::FormID> g_playerKilled;

static std::unordered_set<RE::FormID> g_searched;

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
        if (!event || !event->actorDying || !event->actorKiller)
            return RE::BSEventNotifyControl::kContinue;

        auto* killer = event->actorKiller.get();
        if (killer && killer->IsPlayerRef()) {
            auto* dying = event->actorDying.get();
            if (dying)
                g_playerKilled.insert(dying->GetFormID());
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

class HasLootCondition : public SIF::ICondition {
public:
    bool Match(RE::TESObjectREFR* ref) const override {
        if (!ref) return false;
        auto inv = ref->GetInventory([](const RE::TESBoundObject& obj) {
            return obj.GetPlayable();
        });
        return !inv.empty();
    }
};

class KilledByPlayerCondition : public SIF::ICondition {
public:
    bool Match(RE::TESObjectREFR* ref) const override {
        return ref && g_playerKilled.contains(ref->GetFormID());
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

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* msg) {
        if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
            holder->AddEventSink<RE::TESDeathEvent>(DeathSink::GetSingleton());
            holder->AddEventSink<RE::TESActivateEvent>(ActivateSink::GetSingleton());
        }
    });

    SIF::ListenForRegistration([](SKSE::MessagingInterface::Message* msg) {
        if (msg->type != SIF::kMessage_GetAPI) return;

        auto* api = static_cast<SIF::IAPI*>(msg->data);
        if (!api) return;

        api->RegisterCondition("hasLoot", [](const Json::Value&, RE::FormType) {
            return std::make_unique<HasLootCondition>();
        });
        api->RegisterCondition("killedByPlayer", [](const Json::Value&, RE::FormType) {
            return std::make_unique<KilledByPlayerCondition>();
        });
        api->RegisterCondition("notSearched", [](const Json::Value&, RE::FormType) {
            return std::make_unique<NotSearchedCondition>();
        });
    });

    return true;
}
