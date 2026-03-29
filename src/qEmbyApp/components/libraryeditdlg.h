#ifndef LIBRARYEDITDLG_H
#define LIBRARYEDITDLG_H

#include "librarycreatedlg.h"

class QEmbyCore;

class LibraryEditDialog : public LibraryCreateDialog {
    Q_OBJECT
public:
    explicit LibraryEditDialog(const VirtualFolder& folder,
                               QEmbyCore* core,
                               const QStringList& metadataFetchers = {},
                               const QStringList& imageFetchers = {},
                               const QStringList& subtitleDownloaders = {},
                               const QStringList& lyricsFetchers = {},
                               const QList<LocalizationCulture>& cultures = {},
                               const QList<LocalizationCountry>& countries = {},
                               QWidget *parent = nullptr);

    QString newName() const;
    QStringList addedPaths() const;
    QStringList removedPaths() const;

private:
    QStringList m_originalPaths;
};

#endif 
