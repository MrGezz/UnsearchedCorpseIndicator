#pragma once

#include <cstdint>
#include <functional>
#include <json/json.h>

namespace SIF {
    class ICondition {
    public:
        virtual ~ICondition() = default;
        virtual bool Match(RE::TESObjectREFR* ref) const = 0;
    };

    using ConditionBuilder = std::function<
        std::unique_ptr<ICondition>(const Json::Value& value, RE::FormType type)
    >;

    class IAPI {
    public:
        virtual ~IAPI() = default;

        virtual bool RegisterCondition(
            const char* name,
            ConditionBuilder builder
        ) = 0;

        virtual uint32_t GetVersion() const = 0;
    };

    constexpr uint32_t kMessage_GetAPI = 0xD111;

    inline void ListenForRegistration(SKSE::MessagingInterface::EventCallback cb) {
        SKSE::GetMessagingInterface()->RegisterListener("StatusIndicatorFramework", cb);
    }
}
