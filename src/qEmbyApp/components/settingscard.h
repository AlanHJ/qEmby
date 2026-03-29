#ifndef SETTINGSCARD_H
#define SETTINGSCARD_H

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

class ElidedLabel;

class SettingsCard : public QFrame {
  Q_OBJECT
public:
  
  explicit SettingsCard(const QString &iconSvgPath, const QString &title,
                        const QString &description, QWidget *controlWidget,
                        const QString &configKey, QWidget *parent = nullptr,
                        const QVariant &defaultValue = QVariant());

  ~SettingsCard() override;

private:
  void setupUi(const QString &title, const QString &description);
  void setupDataBinding();
  void updateIcon();

private slots:
  void onConfigValueChanged(const QString &key, const QVariant &newValue);
  void onThemeChanged();

private:
  QLabel *m_iconLabel;
  QLabel *m_titleLabel;
  ElidedLabel *m_descLabel;
  QWidget *m_controlWidget;
  QString m_configKey;
  QString m_iconSvgPath;
  QVariant m_defaultValue;

  QHBoxLayout *m_mainLayout;
};

#endif 
