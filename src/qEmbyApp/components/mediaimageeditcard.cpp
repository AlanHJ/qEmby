#include "mediaimageeditcard.h"
#include "elidedlabel.h"
#include "../managers/thememanager.h"
#include "../utils/imageutils.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QVariant>
#include <QVBoxLayout>

MediaImageEditCard::MediaImageEditCard(QString title, QString details,
                                     bool hasImage, QWidget* parent)
    : QFrame(parent), m_hasImage(hasImage)
{
    setObjectName("MediaImageEditCard");
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedSize(224, 252);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 10);
    layout->setSpacing(6);

    m_preview = new QLabel(this);
    m_preview->setObjectName("MediaImageCardPreview");
    m_preview->setFixedSize(200, 136);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setWordWrap(true);
    
    m_preview->setAcceptDrops(false);
    m_preview->installEventFilter(this);
    m_preview->setFocusPolicy(hasImage ? Qt::StrongFocus : Qt::NoFocus);
    m_preview->setAccessibleName(hasImage ? tr("View Image") : title);
    m_preview->setText(hasImage ? tr("Loading preview...") : title);
    setToolTip(tr("Drop an image file or image URL here to upload it."));
    m_preview->setToolTip(hasImage
        ? tr("Click to view the image. Drop an image file here to replace it.")
        : tr("Drop an image file or image URL here to upload it."));
    layout->addWidget(m_preview, 0, Qt::AlignHCenter);

    auto* name = new ElidedLabel(this);
    name->setFullText(hasImage ? title : QString());
    name->setObjectName("MediaImageCardTitle");
    name->setAlignment(Qt::AlignCenter);
    name->setFixedHeight(20);
    name->setToolTip(title);
    layout->addWidget(name);

    auto* info = new ElidedLabel(this);
    info->setFullText(hasImage ? details : tr("No image"));
    info->setObjectName("MediaImageCardDetails");
    info->setAlignment(Qt::AlignCenter);
    info->setFixedHeight(18);
    info->setToolTip(details);
    layout->addWidget(info);

    auto* actions = new QHBoxLayout();
    actions->setSpacing(8);
    actions->addStretch();
    m_choose = addAction(actions, Action::ChooseFile,
                         hasImage ? tr("Replace Image") : tr("Choose Image"),
                         hasImage ? "edit.svg" : "plus.svg");
    m_url = addAction(actions, Action::ImageUrl, tr("Image URL"), "url-replace.svg");
    m_download = addAction(actions, Action::Download, tr("Download"), "download-menu.svg");
    m_remove = addAction(actions, Action::Remove, tr("Remove Image"), "delete.svg");
    m_remove->setProperty("destructive", true);
    actions->addStretch();
    layout->addLayout(actions);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this]() { updateIcons(); });
    updateIcons();
    setBusy(false);
}

QPushButton* MediaImageEditCard::addAction(QHBoxLayout* layout, Action action,
                                        const QString& tooltip, const QString& iconName)
{
    auto* button = new QPushButton(this);
    button->setObjectName("MediaImageCardAction");
    button->setProperty("icon-name", iconName);
    button->setFixedSize(30, 30);
    button->setIconSize(QSize(16, 16));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoDefault(false);
    connect(button, &QPushButton::clicked, this,
            [this, action]() { emit actionRequested(action); });
    layout->addWidget(button);
    return button;
}

void MediaImageEditCard::updateIcons()
{
    const QString theme = ThemeManager::instance()->isDarkMode() ? "dark" : "light";
    for (auto* button : {m_choose, m_url, m_download, m_remove})
        button->setIcon(QIcon(QStringLiteral(":/svg/%1/%2")
                                 .arg(theme, button->property("icon-name").toString())));
}

void MediaImageEditCard::setPreview(const QPixmap& pixmap)
{
    m_preview->setText(QString());
    const qreal dpr = devicePixelRatioF();
    QPixmap preview = ImageUtils::containRoundedPixmap(pixmap, m_preview->size() * dpr, qRound(6 * dpr));
    preview.setDevicePixelRatio(dpr);
    m_preview->setPixmap(preview);
}

void MediaImageEditCard::setPreviewMessage(const QString& message)
{
    m_preview->setPixmap(QPixmap());
    m_preview->setText(message);
}

void MediaImageEditCard::setBusy(bool busy)
{
    m_busy = busy;
    m_previewPressed = false;
    m_choose->setEnabled(!busy);
    m_url->setEnabled(!busy);
    m_download->setEnabled(!busy && m_hasImage);
    m_remove->setEnabled(!busy && m_hasImage);
    setAcceptDrops(!busy);
    m_preview->setCursor(!busy && m_hasImage ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (busy)
        setDropActive(false);
}

bool MediaImageEditCard::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_preview && m_hasImage && !m_busy) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_previewPressed = true;
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                const bool activate = m_previewPressed && m_preview->rect().contains(mouse->position().toPoint());
                m_previewPressed = false;
                if (activate)
                    emit actionRequested(Action::View);
                return true;
            }
        } else if (event->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter || key->key() == Qt::Key_Space) {
                if (!key->isAutoRepeat())
                    emit actionRequested(Action::View);
                return true;
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}

void MediaImageEditCard::setDropActive(bool active)
{
    if (property("drag-over").toBool() == active)
        return;
    setProperty("drag-over", active);
    style()->unpolish(this);
    style()->polish(this);
    update();
}
