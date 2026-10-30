#ifndef ANIME4KSHADERMANAGER_H
#define ANIME4KSHADERMANAGER_H

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

// Unpacks the bundled Anime4K v4.0.1 GLSL shaders from Qt resources to the app
// data directory (mpv's glsl-shaders option needs real file paths) and provides
// the ordered shader chains of the quality presets.
class Anime4KShaderManager : public QObject {
    Q_OBJECT
public:
    enum class Preset { Fast, Balanced, Quality };

    struct ShaderEntry {
        QString path;
        QString displayName;
        bool bundled = false;
    };

    static Anime4KShaderManager &instance();

    bool ensureShadersExtracted();
    bool isReady() const;

    QStringList shaderChain(Preset preset) const;

    static QString presetId(Preset preset);
    static Preset presetFromId(const QString &id, Preset fallback = Preset::Balanced);
    static QString presetDisplayName(Preset preset);
    static QList<Preset> availablePresets();

    static QString presetIdOfCustom();
    static bool isCustomPresetId(const QString &id);

    QStringList configuredChain() const;
    void setConfiguredChain(const QStringList &paths);
    void applyPresetTemplate(Preset preset);

    // Full catalog: all bundled Anime4K v4.0.1 shaders (in a stable
    // functional-group order) followed by user-imported shaders from
    // customShaderDir().
    QList<ShaderEntry> shaderCatalog() const;

    // User-imported shaders live in a separate data folder, kept apart from
    // the bundled set.
    static QString customShaderDir();
    // Copies srcPath into customShaderDir(), deduplicating file names.
    // Returns the stored path, or an empty string on failure.
    QString importCustomShader(const QString &srcPath);
    // Deletes an imported shader. Only files inside customShaderDir() may be
    // removed. Returns true when the file was deleted.
    bool removeCustomShader(const QString &path);

private:
    explicit Anime4KShaderManager(QObject *parent = nullptr);

    bool m_ready = false;
};

#endif
