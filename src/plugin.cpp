#include "Conditions.h"
#include "Events.h"
#include "Serialization.h"
#include "Settings.h"
#include <SKSE/API.h>
#include <SKSE/Interfaces.h>

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    UCI::Settings::Load();
    UCI::Events::Register();
    UCI::Serialization::Install();
    UCI::Conditions::Register();

    return true;
}
