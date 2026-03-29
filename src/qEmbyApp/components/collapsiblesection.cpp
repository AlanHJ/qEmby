#include "collapsiblesection.h"

#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStyle>
#include <QStyleOptionButton>
#include <QVBoxLayout>

namespace {
class SectionToggleButton : public QPushButton {
public:
  explicit SectionToggleButton(QWidget *parent = nullptr)
      : QPushButton(parent) {}

  bool expanded = false;

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QStyleOptionButton opt;
    opt.initFrom(this);
    style()->drawControl(QStyle::CE_PushButton, &opt, &p, this);

    QColor arrowColor = palette().color(QPalette::ButtonText);
    arrowColor.setAlphaF(0.7f);
    p.setPen(Qt::NoPen);
    p.setBrush(arrowColor);

    const int arrowSize = 8;
    const int leftMargin = 2;
    const int cy = height() / 2;

    QPainterPath path;
    if (expanded) {
      const qreal ax = leftMargin;
      const qreal ay = cy - arrowSize / 3.0;
      path.moveTo(ax, ay);
      path.lineTo(ax + arrowSize, ay);
      path.lineTo(ax + arrowSize / 2.0, ay + arrowSize * 2.0 / 3.0);
    } else {
      const qreal ax = leftMargin;
      const qreal ay = cy - arrowSize / 2.0;
      path.moveTo(ax, ay);
      path.lineTo(ax + arrowSize * 2.0 / 3.0, ay + arrowSize / 2.0);
      path.lineTo(ax, ay + arrowSize);
    }
    path.closeSubpath();
    p.drawPath(path);

    QFont f = font();
    f.setBold(true);
    p.setFont(f);
    p.setPen(palette().color(QPalette::ButtonText));
    const int textLeft = leftMargin + arrowSize + 6;
    const QRect textRect = rect().adjusted(textLeft, 0, -8, 0);
    const QFontMetrics fm(f);
    const int textWidth = fm.horizontalAdvance(text());
    p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());

    QColor lineColor = arrowColor;
    lineColor.setAlphaF(0.25f);
    p.setPen(QPen(lineColor, 1));
    const int lineX = textLeft + textWidth + 10;
    const int lineRight = width() - 4;
    if (lineX < lineRight) {
      p.drawLine(lineX, cy, lineRight, cy);
    }
  }
};
} 

CollapsibleSection::CollapsibleSection(const QString &title, QWidget *parent)
    : QWidget(parent), m_title(title) {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  m_toggle = new SectionToggleButton(this);
  m_toggle->setObjectName("LibAdvancedToggle");
  m_toggle->setCursor(Qt::PointingHandCursor);
  m_toggle->setFlat(true);
  m_toggle->setMinimumHeight(32);
  mainLayout->addWidget(m_toggle);

  m_content = new QWidget(this);
  m_content->setObjectName("LibAdvancedPanel");
  m_content->setVisible(false);

  m_contentLayout = new QVBoxLayout(m_content);
  m_contentLayout->setContentsMargins(12, 10, 12, 10);
  m_contentLayout->setSpacing(8);
  mainLayout->addWidget(m_content);

  updateToggleText();

  connect(m_toggle, &QPushButton::clicked, this,
          [this]() { setExpanded(!m_expanded); });
}

void CollapsibleSection::setExpanded(bool expanded) {
  m_expanded = expanded;
  m_content->setVisible(expanded);
  updateToggleText();
}

void CollapsibleSection::updateToggleText() {
  m_toggle->setText(m_title);
  static_cast<SectionToggleButton *>(m_toggle)->expanded = m_expanded;
  m_toggle->update();
}
