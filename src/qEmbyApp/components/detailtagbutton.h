#ifndef DETAILTAGBUTTON_H
#define DETAILTAGBUTTON_H

#include <QPushButton>
#include <QSize>
#include <QString>

class DetailTagButton : public QPushButton {
public:
  explicit DetailTagButton(const QString &text, QWidget *parent = nullptr);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

private:
  static constexpr int kTagHeight = 28;
};

#endif 
