#ifndef LIBRARYCREATEDLG_P_H
#define LIBRARYCREATEDLG_P_H

#include "../utils/libraryoptionsserializer.h"
#include "../utils/librarytypeprofile.h"
#include <QList>
#include <QString>
#include <optional>

class FetcherRowWidget;
class LibraryCreateDialog;
class ModernComboBox;
class ModernSwitch;

class LibraryCreateDialogOptionsBuilder {
public:
    LibraryCreateDialogOptionsBuilder(
        const LibraryCreateDialog& dialog,
        const LibraryTypeUtils::LibraryTypeProfile& profile,
        ServerProfile::ServerType serverType);

    LibraryOptionsSerializer::LibraryOptionsState build(const QString& contentType) const;

private:
    static void assignChecked(std::optional<bool>& target,
                              const ModernSwitch* toggle);
    static void assignCurrentInt(std::optional<int>& target,
                                 const ModernComboBox* combo);

    void collectBasicState(LibraryOptionsSerializer::LibraryOptionsState& state) const;
    void collectMetadataState(LibraryOptionsSerializer::LibraryOptionsState& state) const;
    void collectAdvancedState(LibraryOptionsSerializer::LibraryOptionsState& state) const;
    void collectImageState(LibraryOptionsSerializer::LibraryOptionsState& state) const;
    void collectSubtitleState(LibraryOptionsSerializer::LibraryOptionsState& state) const;
    void collectLyricsState(LibraryOptionsSerializer::LibraryOptionsState& state) const;
    void collectPlaybackState(LibraryOptionsSerializer::LibraryOptionsState& state) const;
    void collectFetcherState(LibraryOptionsSerializer::LibraryOptionsState& state) const;
    LibraryOptionsSerializer::FetcherSelection collectFetcherSelection(
        const QList<FetcherRowWidget*>& rows,
        LibraryTypeUtils::FetcherKind kind) const;

    const LibraryCreateDialog& m_dialog;
    const LibraryTypeUtils::LibraryTypeProfile& m_profile;
    ServerProfile::ServerType m_serverType;
};

#endif 
