#include "PCH.h"
#include "Events.h"
#include "Settings.h"
#include "State.h"

namespace UCI {
    void MarkCorpseSearched(RE::FormID formID) {
        g_searched.insert(formID);
        g_deathTimes.erase(formID);
        g_playerKilled.erase(formID);
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
            if (!events || Settings::toggleKey == RE::BSKeyboardDevice::Keys::kNone)
                return RE::BSEventNotifyControl::kContinue;

            for (auto* event = *events; event; event = event->next) {
                if (event->GetDevice() != RE::INPUT_DEVICE::kKeyboard)
                    continue;

                auto* button = event->AsButtonEvent();
                if (!button || !button->IsDown())
                    continue;

                if (button->GetIDCode() == Settings::toggleKey) {
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
                MarkCorpseSearched(target->GetFormID());

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
                MarkCorpseSearched(source->GetFormID());

            return RE::BSEventNotifyControl::kContinue;
        }
    };

}

namespace UCI::Events {
    void Register() {
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
    }
}
