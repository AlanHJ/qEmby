#ifndef MODERNCOMBOBOX_H
#define MODERNCOMBOBOX_H

#include <QComboBox>
#include <QStyledItemDelegate>



class CenterAlignDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit CenterAlignDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {}

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
};


class ModernComboBox : public QComboBox {
  Q_OBJECT

public:
  explicit ModernComboBox(QWidget *parent = nullptr);
  ~ModernComboBox() override = default;

  
  
  void setMaxTextWidth(int pixels);

  
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  
  void paintEvent(QPaintEvent *event) override;
  void showPopup() override;

private:
  void polishPopupView();
  int m_maxTextWidth = 0;
};

#endif 
