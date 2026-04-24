#include "PCH.h"
#include "Conditions.h"
#include "SIF_API.h"
#include "Settings.h"
#include "State.h"

namespace UCI {
    bool HasVisibleInventoryEntry(const RE::InventoryEntryData* entry, std::int32_t count) {
        if (!entry || !entry->object || count <= 0) return false;

        return entry->countDelta > 0;
    }

    bool MatchesCorpseScope(RE::TESObjectREFR* ref) {
        if (!ref) return false;

        if (Settings::allCorpses) {
            if (Settings::delay > 0.0f) {
                auto it = g_deathTimes.find(ref->GetFormID());
                if (it != g_deathTimes.end()) {
                    float elapsed = GetRealTime() - it->second;
                    if (elapsed < Settings::delay) return false;
                }
            }

            return true;
        }

        auto it = g_playerKilled.find(ref->GetFormID());
        if (it == g_playerKilled.end()) return false;

        if (Settings::delay > 0.0f) {
            float elapsed = GetRealTime() - it->second;
            if (elapsed < Settings::delay) return false;
        }

        return true;
    }

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

}

namespace UCI::Conditions {
    void Register() {
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
    }
}
