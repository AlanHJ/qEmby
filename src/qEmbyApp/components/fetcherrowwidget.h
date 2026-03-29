#ifndef FETCHERROWWIDGET_H
#define FETCHERROWWIDGET_H

#include <QWidget>

class QPushButton;
class QLabel;
class ModernSwitch;




class FetcherRowWidget : public QWidget {
  Q_OBJECT
public:
  explicit FetcherRowWidget(const QString &name, QWidget *parent = nullptr);

  QString name() const;
  bool isEnabled() const;
  void setFetcherEnabled(bool on);
  void setUpEnabled(bool on);
  void setDownEnabled(bool on);

signals:
  void moveUpClicked();
  void moveDownClicked();

private:
  QString m_name;
  QPushButton *m_upBtn;
  QPushButton *m_downBtn;
  QLabel *m_label;
  ModernSwitch *m_switch;
};

#endif 
