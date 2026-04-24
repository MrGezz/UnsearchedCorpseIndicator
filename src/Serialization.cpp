#include "PCH.h"
#include "Serialization.h"
#include "State.h"

namespace UCI::Serialization {
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
        ResetRuntimeState();

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
        ResetRuntimeState();
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
