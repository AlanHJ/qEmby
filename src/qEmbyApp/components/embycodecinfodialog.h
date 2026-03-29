#ifndef EMBYCODECINFODIALOG_H
#define EMBYCODECINFODIALOG_H

#include "moderndialogbase.h"
#include <QJsonObject>

class EmbyCodecInfoDialog : public ModernDialogBase {
    Q_OBJECT
public:
    explicit EmbyCodecInfoDialog(QJsonObject codecInfo, QWidget* parent = nullptr);

private:
    void setupUi();
    QString codecTitle() const;
    QString codecDescription() const;
    QString maxBitrateText() const;
    QString frameSizesText() const;
    QString frameRatesText() const;
    QString maxInstancesText() const;
    QString colorFormatsText() const;
    QString profileDescription(const QJsonObject& profile) const;
    QString levelDescription(const QJsonObject& level) const;
    QString levelBitrate(const QJsonObject& level) const;
    QString levelResolutions(const QJsonObject& level) const;
    QString bitDepthsText(const QJsonObject& profile) const;

    QJsonObject m_codecInfo;
};

#endif 
