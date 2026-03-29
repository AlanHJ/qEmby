#include "detailbottominfowidget.h"
#include "flowlayout.h"
#include "horizontalwidgetgallery.h"
#include <QDesktopServices> 
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>


DetailBottomInfoWidget::DetailBottomInfoWidget(QWidget *parent)
    : QWidget(parent) {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(40, 8, 40, 0);
  mainLayout->setSpacing(12);

  
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

  m_tagsBottomTitle = new QLabel(tr("Tags"), this);
  m_tagsBottomTitle->setObjectName("detail-section-title");
  m_tagsBottomWidget = new QWidget(this);
  m_tagsBottomLayout = new FlowLayout(m_tagsBottomWidget, 0, 8, 8);

  m_studiosTitle = new QLabel(tr("Studios"), this);
  m_studiosTitle->setObjectName("detail-section-title");
  m_studiosWidget = new QWidget(this);
  m_studiosLayout = new FlowLayout(m_studiosWidget, 0, 8, 8);

  
  m_externalLinksTitle = new QLabel(tr("External Links"), this);
  m_externalLinksTitle->setObjectName("detail-section-title");
  m_externalLinksWidget = new QWidget(this);
  m_externalLinksLayout = new FlowLayout(m_externalLinksWidget, 0, 8, 8);

  m_mediaInfoTitle = new QLabel(tr("Media Info"), this);
  m_mediaInfoTitle->setObjectName("detail-section-title");

  m_fileInfoWidget = new QWidget(this);
  auto *fileInfoLayout = new QVBoxLayout(m_fileInfoWidget);
  fileInfoLayout->setContentsMargins(0, 0, 0, 4);
  fileInfoLayout->setSpacing(6);

  m_filePathLabel = new QLabel(m_fileInfoWidget);
  m_filePathLabel->setObjectName("detail-filepath");
  m_filePathLabel->setWordWrap(true);
  m_filePathLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

  m_fileMetaLabel = new QLabel(m_fileInfoWidget);
  m_fileMetaLabel->setObjectName("detail-filemeta");

  fileInfoLayout->addWidget(m_filePathLabel);
  fileInfoLayout->addWidget(m_fileMetaLabel);

  m_mediaInfoGallery = new HorizontalWidgetGallery(this);
  m_mediaInfoGallery->setObjectName("detail-media-info-gallery");

  m_tagsBottomWrapper = wrapMaxWidth(m_tagsBottomWidget, 1200);
  mainLayout->addWidget(m_tagsBottomTitle);
  mainLayout->addWidget(m_tagsBottomWrapper);

  m_studiosWrapper = wrapMaxWidth(m_studiosWidget, 1200);
  mainLayout->addWidget(m_studiosTitle);
  mainLayout->addWidget(m_studiosWrapper);

  
  m_externalLinksWrapper = wrapMaxWidth(m_externalLinksWidget, 1200);
  mainLayout->addWidget(m_externalLinksTitle);
  mainLayout->addWidget(m_externalLinksWrapper);

  mainLayout->addWidget(m_mediaInfoTitle);
  mainLayout->addWidget(m_fileInfoWidget);
  mainLayout->addWidget(m_mediaInfoGallery);

  clear();
}

QWidget *DetailBottomInfoWidget::wrapMaxWidth(QWidget *child, int maxW) {
  child->setMaximumWidth(maxW);
  child->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  QWidget *wrapper = new QWidget(this);
  wrapper->setObjectName("detail-maxWidth-wrapper");
  wrapper->setStyleSheet(
      "QWidget#detail-maxWidth-wrapper { background:transparent; }");
  wrapper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  auto *hl = new QHBoxLayout(wrapper);
  hl->setContentsMargins(0, 0, 0, 0);
  hl->setSpacing(0);
  hl->addWidget(child, 1);
  hl->addStretch(0);
  return wrapper;
}

void DetailBottomInfoWidget::clear() {
  clearLayout(m_tagsBottomLayout);
  clearLayout(m_studiosLayout);
  clearLayout(m_externalLinksLayout); 
  m_mediaInfoGallery->clear();

  m_tagsBottomTitle->hide();
  m_tagsBottomWrapper->hide();
  m_studiosTitle->hide();
  m_studiosWrapper->hide();
  m_externalLinksTitle->hide();
  m_externalLinksWrapper->hide();
  m_mediaInfoTitle->hide();
  m_fileInfoWidget->hide();
  m_mediaInfoGallery->hide();
}

void DetailBottomInfoWidget::clearLayout(QLayout *layout) {
  if (!layout)
    return;
  QLayoutItem *child;
  while ((child = layout->takeAt(0)) != nullptr) {
    if (child->widget()) {
      child->widget()->hide();
      child->widget()->setParent(nullptr);
      child->widget()->deleteLater();
    } else if (child->layout()) {
      clearLayout(child->layout());
    }
    delete child;
  }
}

QString DetailBottomInfoWidget::formatSize(long long bytes) {
  if (bytes < 1024)
    return QString::number(bytes) + " B";
  if (bytes < 1024 * 1024)
    return QString::number(bytes / 1024.0, 'f', 1) + " KB";
  if (bytes < 1024 * 1024 * 1024)
    return QString::number(bytes / 1048576.0, 'f', 1) + " MB";
  return QString::number(bytes / 1073741824.0, 'f', 1) + " GB";
}

void DetailBottomInfoWidget::addInfoRow(QGridLayout *layout, int &row,
                                        const QString &key,
                                        const QString &value) {
  if (value.isEmpty())
    return;
  QLabel *kLabel = new QLabel(key);
  kLabel->setObjectName("stream-info-key");
  QLabel *vLabel = new QLabel(value);
  vLabel->setObjectName("stream-info-value");
  vLabel->setWordWrap(true);
  layout->addWidget(kLabel, row, 0, Qt::AlignTop | Qt::AlignLeft);
  layout->addWidget(vLabel, row, 1, Qt::AlignTop | Qt::AlignLeft);
  row++;
}

void DetailBottomInfoWidget::setInfo(const MediaItem &item,
                                     const QList<MediaSourceInfo> &sources) {
  clear();

  if (!item.tags.isEmpty()) {
    m_tagsBottomTitle->show();
    m_tagsBottomWrapper->show();
    for (const QString &tag : item.tags) {
      auto *btn = new QPushButton(tag, m_tagsBottomWidget);
      btn->setObjectName("detail-genre-tag");
      btn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
      btn->setCursor(Qt::PointingHandCursor);
      connect(btn, &QPushButton::clicked, this,
              [this, tag]() { Q_EMIT filterClicked("Tag", tag); });
      m_tagsBottomLayout->addWidget(btn);
    }
  }

  if (!item.studios.isEmpty()) {
    m_studiosTitle->show();
    m_studiosWrapper->show();
    for (const auto &studio : item.studios) {
      auto *btn = new QPushButton(studio.name, m_studiosWidget);
      btn->setObjectName("detail-genre-tag");
      btn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
      btn->setCursor(Qt::PointingHandCursor);
      connect(btn, &QPushButton::clicked, this,
              [this, name = studio.name]() { Q_EMIT filterClicked("Studio", name); });
      m_studiosLayout->addWidget(btn);
    }
  }

  
  if (!item.externalUrls.isEmpty()) {
    m_externalLinksTitle->show();
    m_externalLinksWrapper->show();
    for (const auto &link : item.externalUrls) {
      auto *btn = new QPushButton(link.name, m_externalLinksWidget);
      
      btn->setObjectName("detail-genre-tag");
      btn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
      btn->setCursor(Qt::PointingHandCursor);
      
      connect(btn, &QPushButton::clicked, this,
              [link]() { QDesktopServices::openUrl(QUrl(link.url)); });
      m_externalLinksLayout->addWidget(btn);
    }
  }

  if (sources.isEmpty())
    return;

  m_mediaInfoTitle->show();
  m_fileInfoWidget->show();
  m_mediaInfoGallery->show();

  const auto &source = sources.first();
  QString actualPath = source.path.isEmpty() ? item.path : source.path;
  m_filePathLabel->setText(QUrl::fromPercentEncoding(actualPath.toUtf8()));

  QStringList metaParts;
  QString container =
      source.container.isEmpty() ? item.container : source.container;
  if (!container.isEmpty())
    metaParts << container.toUpper();

  long long size = source.size > 0 ? source.size : item.size;
  if (size > 0)
    metaParts << formatSize(size);
  if (!item.dateCreated.isEmpty())
    metaParts << tr("Added %1").arg(item.dateCreated);
  m_fileMetaLabel->setText(metaParts.join("  •  "));

  for (const auto &stream : source.mediaStreams) {
    QWidget *card = new QWidget();
    card->setObjectName("stream-card");
    card->setMinimumWidth(280);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    QString typeText = stream.type;
    if (typeText == "Video")
      typeText = tr("Video");
    else if (typeText == "Audio")
      typeText = tr("Audio");
    else if (typeText == "Subtitle")
      typeText = tr("Subtitle");

    QLabel *typeLabel = new QLabel(typeText, card);
    typeLabel->setObjectName("stream-type");
    layout->addWidget(typeLabel);

    QGridLayout *grid = new QGridLayout();
    grid->setContentsMargins(0, 4, 0, 0);
    grid->setSpacing(8);
    grid->setColumnMinimumWidth(0, 80);
    int row = 0;

    addInfoRow(grid, row, tr("Title"), stream.displayTitle);
    addInfoRow(grid, row, tr("Codec"), stream.codec.toUpper());

    if (stream.type == "Video") {
      if (!stream.language.isEmpty())
        addInfoRow(grid, row, tr("Language"), stream.language);
      if (!stream.codecTag.isEmpty())
        addInfoRow(grid, row, tr("Codec Tag"), stream.codecTag);
      if (!stream.profile.isEmpty())
        addInfoRow(grid, row, tr("Profile"), stream.profile);
      if (stream.level > 0)
        addInfoRow(grid, row, tr("Level"), QString::number(stream.level));
      if (stream.width > 0 && stream.height > 0)
        addInfoRow(grid, row, tr("Resolution"),
                   QString("%1x%2").arg(stream.width).arg(stream.height));
      if (!stream.aspectRatio.isEmpty())
        addInfoRow(grid, row, tr("Aspect Ratio"), stream.aspectRatio);
      if (stream.isInterlaced)
        addInfoRow(grid, row, tr("Interlaced"), tr("Yes"));
      if (stream.realFrameRate > 0)
        addInfoRow(grid, row, tr("Framerate"),
                   QString::number(stream.realFrameRate, 'f', 2));
      if (stream.bitRate > 0)
        addInfoRow(grid, row, tr("Bitrate"),
                   QString("%1 kbps").arg(stream.bitRate / 1000));
      if (stream.bitDepth > 0)
        addInfoRow(grid, row, tr("Bit Depth"),
                   QString("%1 bit").arg(stream.bitDepth));
      if (!stream.pixelFormat.isEmpty())
        addInfoRow(grid, row, tr("Pixel Format"), stream.pixelFormat);
      if (stream.refFrames > 0)
        addInfoRow(grid, row, tr("Ref Frames"),
                   QString::number(stream.refFrames));
    } else if (stream.type == "Audio") {
      if (!stream.language.isEmpty())
        addInfoRow(grid, row, tr("Language"), stream.language);
      if (!stream.codecTag.isEmpty())
        addInfoRow(grid, row, tr("Codec Tag"), stream.codecTag);
      if (!stream.profile.isEmpty())
        addInfoRow(grid, row, tr("Profile"), stream.profile);
      if (!stream.channelLayout.isEmpty())
        addInfoRow(grid, row, tr("Layout"), stream.channelLayout);
      if (stream.channels > 0)
        addInfoRow(grid, row, tr("Channels"),
                   QString("%1 ch").arg(stream.channels));
      if (stream.bitRate > 0)
        addInfoRow(grid, row, tr("Bitrate"),
                   QString("%1 kbps").arg(stream.bitRate / 1000));
      if (stream.sampleRate > 0)
        addInfoRow(grid, row, tr("Sample Rate"),
                   QString("%1 Hz").arg(stream.sampleRate));
      if (stream.isDefault)
        addInfoRow(grid, row, tr("Default"), tr("Yes"));
    } else if (stream.type == "Subtitle") {
      QString displayLang = stream.displayLanguage.isEmpty()
                                ? stream.language
                                : stream.displayLanguage;
      if (!displayLang.isEmpty())
        addInfoRow(grid, row, tr("Language"), displayLang);
      if (stream.index >= 0)
        addInfoRow(grid, row, tr("Index"), QString::number(stream.index));
      if (stream.isDefault)
        addInfoRow(grid, row, tr("Default"), tr("Yes"));
      if (stream.isForced)
        addInfoRow(grid, row, tr("Forced"), tr("Yes"));
      if (stream.isHearingImpaired)
        addInfoRow(grid, row, tr("Hearing Impaired"), tr("Yes"));
      if (stream.isExternal)
        addInfoRow(grid, row, tr("External"), tr("Yes"));
      if (stream.isTextSubtitleStream)
        addInfoRow(grid, row, tr("Text Stream"), tr("Yes"));
      if (stream.supportsExternalStream)
        addInfoRow(grid, row, tr("Ext. Stream Supported"), tr("Yes"));
      if (stream.isExternalUrl)
        addInfoRow(grid, row, tr("External URL"), tr("Yes"));
      if (!stream.deliveryMethod.isEmpty())
        addInfoRow(grid, row, tr("Delivery Method"), stream.deliveryMethod);
      if (!stream.protocol.isEmpty() && stream.protocol != "None")
        addInfoRow(grid, row, tr("Protocol"), stream.protocol);
      if (!stream.extendedVideoType.isEmpty() &&
          stream.extendedVideoType != "None")
        addInfoRow(grid, row, tr("Ext. Video Type"), stream.extendedVideoType);
      if (!stream.extendedVideoSubType.isEmpty() &&
          stream.extendedVideoSubType != "None")
        addInfoRow(grid, row, tr("Ext. Video SubType"),
                   stream.extendedVideoSubType);
      if (stream.attachmentSize > 0)
        addInfoRow(grid, row, tr("Size"), formatSize(stream.attachmentSize));
      QFileInfo fi(stream.path);
      if (!stream.path.isEmpty())
        addInfoRow(grid, row, tr("Path"), fi.completeBaseName());
      if (!stream.deliveryUrl.isEmpty())
        addInfoRow(grid, row, tr("Delivery URL"), stream.deliveryUrl);
    }

    layout->addLayout(grid);
    layout->addStretch();
    m_mediaInfoGallery->addWidget(card);
  }
  m_mediaInfoGallery->adjustHeightToContent();
}
