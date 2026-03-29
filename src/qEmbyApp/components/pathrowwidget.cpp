#include "pathrowwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

PathRowWidget::PathRowWidget(QWidget *parent) : QWidget(parent) {
  auto *row = new QHBoxLayout(this);
  row->setContentsMargins(0, 2, 0, 2);
  row->setSpacing(4);

  m_pathLabel = new QLabel(this);
  m_pathLabel->setObjectName("ManageLibPathLabel");
  m_pathLabel->setMinimumHeight(28);
  m_pathLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  row->addWidget(m_pathLabel, 1);

  m_browseBtn = new QPushButton(this);
  m_browseBtn->setObjectName("ManageLibPathBrowseBtn");
  m_browseBtn->setFixedSize(28, 28);
  m_browseBtn->setCursor(Qt::PointingHandCursor);
  m_browseBtn->setToolTip(tr("Browse server directory"));
  row->addWidget(m_browseBtn);

  m_removeBtn = new QPushButton(this);
  m_removeBtn->setObjectName("ManageLibPathRemoveBtn");
  m_removeBtn->setFixedSize(28, 28);
  m_removeBtn->setCursor(Qt::PointingHandCursor);
  m_removeBtn->setToolTip(tr("Remove this path"));
  row->addWidget(m_removeBtn);

  connect(m_browseBtn, &QPushButton::clicked, this,
          &PathRowWidget::browseClicked);
  connect(m_removeBtn, &QPushButton::clicked, this,
          &PathRowWidget::removeClicked);
}

QString PathRowWidget::path() const { return m_pathLabel->toolTip(); }

void PathRowWidget::setPath(const QString &path) {
  m_pathLabel->setToolTip(path);
  if (path.isEmpty()) {
    m_pathLabel->setText(tr("Click + to browse..."));
    m_pathLabel->setProperty("empty", true);
  } else {
    m_pathLabel->setText(path);
    m_pathLabel->setProperty("empty", false);
  }

  m_pathLabel->style()->unpolish(m_pathLabel);
  m_pathLabel->style()->polish(m_pathLabel);
}

bool PathRowWidget::isEmpty() const { return path().isEmpty(); }
