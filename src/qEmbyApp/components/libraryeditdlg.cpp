#include "libraryeditdlg.h"

#include <qembycore.h>

namespace {

QStringList normalizePaths(const QStringList &paths) {
  QStringList result;
  for (const QString &path : paths) {
    const QString trimmed = path.trimmed();
    if (!trimmed.isEmpty() && !result.contains(trimmed)) {
      result.append(trimmed);
    }
  }
  return result;
}

} 

LibraryEditDialog::LibraryEditDialog(const VirtualFolder &folder,
                                     QEmbyCore *core,
                                     const QStringList &metadataFetchers,
                                     const QStringList &imageFetchers,
                                     const QStringList &subtitleDownloaders,
                                     const QStringList &lyricsFetchers,
                                     const QList<LocalizationCulture> &cultures,
                                     const QList<LocalizationCountry> &countries,
                                     QWidget *parent)
     : LibraryCreateDialog(core, metadataFetchers, imageFetchers,
                          subtitleDownloaders, lyricsFetchers,
                          cultures, countries, parent),
      m_originalPaths(normalizePaths(folder.locations)) {
  setTitle(tr("Edit Library — \"%1\"").arg(folder.name));
  setConfirmButtonText(tr("Save"));
  loadFromFolder(folder);
  setTypeEditable(false);
}

QString LibraryEditDialog::newName() const { return libraryName(); }

QStringList LibraryEditDialog::addedPaths() const {
  const QStringList currentPaths = normalizePaths(paths());
  QStringList added;
  for (const QString &path : currentPaths) {
    if (!m_originalPaths.contains(path)) {
      added.append(path);
    }
  }
  return added;
}

QStringList LibraryEditDialog::removedPaths() const {
  const QStringList currentPaths = normalizePaths(paths());
  QStringList removed;
  for (const QString &path : m_originalPaths) {
    if (!currentPaths.contains(path)) {
      removed.append(path);
    }
  }
  return removed;
}
