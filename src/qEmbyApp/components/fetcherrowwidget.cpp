#include "fetcherrowwidget.h"
#include "modernswitch.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

FetcherRowWidget::FetcherRowWidget(const QString &name, QWidget *parent)
    : QWidget(parent), m_name(name) {
  setObjectName("FetcherRowWidget");
  setAttribute(Qt::WA_StyledBackground, true);
  auto *row = new QHBoxLayout(this);
  row->setContentsMargins(6, 4, 6, 4);
  row->setSpacing(4);

  m_label = new QLabel(name, this);
  m_label->setObjectName("FetcherRowLabel");
  row->addWidget(m_label, 1);

  m_upBtn = new QPushButton(this);
  m_upBtn->setObjectName("FetcherArrowBtn");
  m_upBtn->setFixedSize(24, 24);
  m_upBtn->setCursor(Qt::PointingHandCursor);
  m_upBtn->setToolTip(tr("Move up"));
  m_upBtn->setText(QStringLiteral("\u25B2")); 
  row->addWidget(m_upBtn);

  m_downBtn = new QPushButton(this);
  m_downBtn->setObjectName("FetcherArrowBtn");
  m_downBtn->setFixedSize(24, 24);
  m_downBtn->setCursor(Qt::PointingHandCursor);
  m_downBtn->setToolTip(tr("Move down"));
  m_downBtn->setText(QStringLiteral("\u25BC")); 
  row->addWidget(m_downBtn);

  m_switch = new ModernSwitch(this);
  m_switch->setObjectName("ManageLibSwitch");
  m_switch->setChecked(true);
  row->addWidget(m_switch);

  connect(m_upBtn, &QPushButton::clicked, this,
          &FetcherRowWidget::moveUpClicked);
  connect(m_downBtn, &QPushButton::clicked, this,
          &FetcherRowWidget::moveDownClicked);
}

QString FetcherRowWidget::name() const { return m_name; }

bool FetcherRowWidget::isEnabled() const { return m_switch->isChecked(); }

void FetcherRowWidget::setFetcherEnabled(bool on) { m_switch->setChecked(on); }

void FetcherRowWidget::setUpEnabled(bool on) { m_upBtn->setEnabled(on); }

void FetcherRowWidget::setDownEnabled(bool on) { m_downBtn->setEnabled(on); }
