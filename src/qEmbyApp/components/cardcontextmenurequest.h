#ifndef CARDCONTEXTMENUREQUEST_H
#define CARDCONTEXTMENUREQUEST_H

#include <QMetaType>
#include <QString>

enum class CardContextMenuAction {
    None,
    Play,
    ExternalPlay,
    ViewDetails,
    Download,
    EditMetadata,
    EditImages,
    Identify,
    RefreshMetadata,
    ScanLibraryFiles,
    ToggleFavorite,
    AddToPlaylist,
    RemoveFromPlaylist,
    DeletePlaylist,
    RemoveMedia,
    MarkPlayed,
    MarkUnplayed,
    RemoveFromResume
};

struct CardContextMenuRequest {
    CardContextMenuAction action = CardContextMenuAction::None;
    QString stringValue;

    bool isValid() const
    {
        return action != CardContextMenuAction::None;
    }
};

Q_DECLARE_METATYPE(CardContextMenuRequest)

#endif 
