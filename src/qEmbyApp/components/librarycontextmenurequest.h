#ifndef LIBRARYCONTEXTMENUREQUEST_H
#define LIBRARYCONTEXTMENUREQUEST_H

#include <QMetaType>
#include <QString>

enum class LibraryContextMenuAction {
    None,
    EditImages,
    RefreshMetadata,
    ScanLibraryFiles
};

struct LibraryContextMenuRequest {
    LibraryContextMenuAction action = LibraryContextMenuAction::None;
    QString stringValue;

    bool isValid() const
    {
        return action != LibraryContextMenuAction::None;
    }
};

Q_DECLARE_METATYPE(LibraryContextMenuRequest)

#endif 
