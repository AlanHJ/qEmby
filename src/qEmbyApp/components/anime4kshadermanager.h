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

    static Anime4KShaderManager &instance();

    bool ensureShadersExtracted();
    bool isReady() const;

    QStringList shaderChain(Preset preset) const;

    static QString presetId(Preset preset);
    static Preset presetFromId(const QString &id, Preset fallback = Preset::Balanced);
    static QString presetDisplayName(Preset preset);
    static QList<Preset> availablePresets();

private:
    explicit Anime4KShaderManager(QObject *parent = nullptr);

    bool m_ready = false;
};

#endif
