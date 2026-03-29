#include "collectioncard.h"
#include "collectiongrid.h"
#include "../managers/thememanager.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>




CollectionCard::CollectionCard(const QString& itemId,
                               const QString& name,
                               int childCount,
                               const QString& type,
                               QWidget* parent)
    : QFrame(parent)
    , m_itemId(itemId)
    , m_name(name)
    , m_childCount(childCount)
    , m_type(type)
{
    setFixedSize(CollectionGrid::CardWidth, CollectionGrid::CardHeight);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);

    
    auto makeBtn = [this](const QString& objName) {
        auto* btn = new QPushButton(this);
        btn->setObjectName(objName);
        btn->setFixedSize(24, 24);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    m_btnRename = makeBtn("CollCardRenameBtn");
    m_btnDelete = makeBtn("CollCardDeleteBtn");

    m_btnRename->setToolTip(tr("Rename"));
    m_btnDelete->setToolTip(tr("Delete"));

    updateButtonLayout();

    connect(m_btnRename, &QPushButton::clicked, this,
            [this]() { emit renameClicked(m_itemId, m_name); });
    connect(m_btnDelete, &QPushButton::clicked, this,
            [this]() { emit deleteClicked(m_itemId, m_name); });
}

void CollectionCard::updateButtonLayout()
{
    const int buttonSize = 24;
    const int buttonSpacing = 4;
    const int totalWidth = 2 * buttonSize + buttonSpacing;
    const int startX = width() - totalWidth - 8;
    const int buttonY = height() - buttonSize - 6;

    m_btnRename->move(startX, buttonY);
    m_btnDelete->move(startX + buttonSize + buttonSpacing, buttonY);
}

void CollectionCard::setImage(const QPixmap& pixmap)
{
    m_image = pixmap;
    update();
}

void CollectionCard::setDisplayData(const QString& name, int childCount,
                                    const QString& type)
{
    if (m_name == name && m_childCount == childCount && m_type == type) {
        return;
    }

    m_name = name;
    m_childCount = childCount;
    m_type = type;
    update();
}

void CollectionCard::setSelected(bool selected)
{
    if (m_selected == selected) {
        return;
    }

    m_selected = selected;
    update();
}

void CollectionCard::setDragging(bool dragging)
{
    if (m_dragging == dragging) {
        return;
    }

    m_dragging = dragging;
    update();
}

void CollectionCard::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    bool isDark = ThemeManager::instance()->isDarkMode();
    int w = width(), h = height();

    
    QColor cardBg   = isDark ? QColor(51, 65, 85, 140) : QColor(255, 255, 255, 245);
    QColor border   = isDark ? QColor(71, 85, 105, 100) : QColor(226, 232, 240);
    QColor nameFg   = isDark ? QColor("#F1F5F9") : QColor("#0F172A");
    QColor typeFg   = isDark ? QColor("#94A3B8") : QColor("#64748B");
    const QColor accent("#3B82F6");

    if (m_selected) {
        cardBg = isDark ? QColor(30, 41, 59, 220) : QColor(239, 246, 255, 245);
        border = isDark ? QColor(96, 165, 250, 220) : QColor(59, 130, 246, 220);
    }

    if (m_dragging) {
        cardBg = isDark ? QColor(15, 23, 42, 235) : QColor(248, 250, 252, 252);
        border = isDark ? QColor(59, 130, 246, 235) : QColor(37, 99, 235, 230);
    }

    p.setPen(QPen(border, m_selected ? 2 : 1));
    p.setBrush(cardBg);
    p.drawRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 12, 12);

    const QRectF badgeRect(w - 28, 10, 18, 18);
    p.setPen(QPen(m_selected ? accent
                             : (isDark ? QColor(100, 116, 139, 160)
                                       : QColor(148, 163, 184, 180)),
                   1.5));
    p.setBrush(m_selected ? accent
                          : (isDark ? QColor(15, 23, 42, 180)
                                    : QColor(255, 255, 255, 220)));
    p.drawEllipse(badgeRect);

    if (m_selected) {
        QPainterPath checkPath;
        checkPath.moveTo(badgeRect.left() + 4.5, badgeRect.center().y());
        checkPath.lineTo(badgeRect.left() + 7.5, badgeRect.bottom() - 4.5);
        checkPath.lineTo(badgeRect.right() - 4.0, badgeRect.top() + 4.5);
        p.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(checkPath);
    }

    
    const int imagePadding = 4;
    const int defaultImgAreaH = 120;
    const bool useSquareThumb = (m_type == "Playlist");
    const int imgSide = qMax(0, w - imagePadding * 2);
    QRectF imgRect(imagePadding, imagePadding,
                   w - imagePadding * 2,
                   useSquareThumb ? imgSide : defaultImgAreaH);

    QLinearGradient grad(imgRect.topLeft(), imgRect.bottomRight());
    grad.setColorAt(0, isDark ? QColor(30, 41, 59) : QColor(241, 245, 249));
    grad.setColorAt(1, isDark ? QColor(51, 65, 85) : QColor(226, 232, 240));

    QPainterPath clipPath;
    clipPath.addRoundedRect(imgRect, 9, 9);
    p.save();
    p.setClipPath(clipPath);
    p.fillRect(imgRect, grad);

    if (!m_image.isNull()) {
        const Qt::AspectRatioMode aspectMode =
            useSquareThumb ? Qt::KeepAspectRatio : Qt::KeepAspectRatioByExpanding;
        QPixmap scaled = m_image.scaled(imgRect.size().toSize(), aspectMode,
                                        Qt::SmoothTransformation);

        const qreal scaledDpr = scaled.devicePixelRatio();
        QSize scaledSize(qRound(scaled.width() / scaledDpr),
                         qRound(scaled.height() / scaledDpr));
        QRect drawRect(0, 0, scaledSize.width(), scaledSize.height());
        drawRect.moveLeft(qRound(imgRect.x() + (imgRect.width() - drawRect.width()) / 2.0));
        drawRect.moveTop(qRound(imgRect.y() + (imgRect.height() - drawRect.height()) / 2.0));

        p.drawPixmap(drawRect.topLeft(), scaled);
    } else {
        QFont iconFont = font();
        iconFont.setPixelSize(32);
        p.setFont(iconFont);
        p.setPen(isDark ? QColor(148, 163, 184, 80) : QColor(100, 116, 139, 80));
        
        QString placeholder = (m_type == "Playlist") ? QString::fromUtf8("🎵") : QString::fromUtf8("📂");
        p.drawText(imgRect, Qt::AlignCenter, placeholder);
    }
    p.restore();

    
    int textTop = qRound(imgRect.bottom()) + 8;
    int textLeft = 10;
    int textWidth = w - 20;

    QFont nameFont = font();
    nameFont.setPixelSize(13);
    nameFont.setWeight(QFont::DemiBold);
    nameFont.setFamilies({"Segoe UI", "Microsoft YaHei", "sans-serif"});
    QFontMetrics nameFm(nameFont);
    QString elidedName = nameFm.elidedText(m_name, Qt::ElideRight, textWidth);

    p.setFont(nameFont);
    p.setPen(nameFg);
    p.drawText(QRect(textLeft, textTop, textWidth, 18), Qt::AlignLeft | Qt::AlignVCenter, elidedName);

    QFont typeFont = font();
    typeFont.setPixelSize(11);
    typeFont.setFamilies({"Segoe UI", "Microsoft YaHei", "sans-serif"});

    QString typeStr;
    if (m_type == "Playlist") {
        typeStr = tr("Playlist");
    } else {
        typeStr = tr("Collection");
    }
    typeStr += QString("  ·  %1 %2").arg(m_childCount)
                   .arg(m_childCount == 1 ? tr("item") : tr("items"));

    p.setFont(typeFont);
    p.setPen(typeFg);
    p.drawText(QRect(textLeft, textTop + 20, textWidth, 16), Qt::AlignLeft | Qt::AlignVCenter, typeStr);
}

void CollectionCard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (!qobject_cast<QPushButton*>(childAt(event->position().toPoint()))) {
            m_pressCandidate = true;
            m_dragStartPos = event->globalPosition().toPoint();
            setCursor(Qt::OpenHandCursor);
            event->accept();
            return;
        }
    }

    QFrame::mousePressEvent(event);
}

void CollectionCard::mouseMoveEvent(QMouseEvent* event)
{
    if ((event->buttons() & Qt::LeftButton) && m_pressCandidate) {
        const QPoint globalPos = event->globalPosition().toPoint();
        if (!m_dragging) {
            const int distance = (globalPos - m_dragStartPos).manhattanLength();
            if (distance >= QApplication::startDragDistance()) {
                setDragging(true);
                setCursor(Qt::ClosedHandCursor);
                if (auto* grid = qobject_cast<CollectionGrid*>(parentWidget())) {
                    grid->onCardDragStart(this, globalPos);
                }
            }
        }

        if (m_dragging) {
            if (auto* grid = qobject_cast<CollectionGrid*>(parentWidget())) {
                grid->onCardDragMove(this, globalPos);
            }
            event->accept();
            return;
        }
    }

    QFrame::mouseMoveEvent(event);
}

void CollectionCard::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_pressCandidate) {
        const bool wasDragging = m_dragging;
        m_pressCandidate = false;

        if (wasDragging) {
            setDragging(false);
            setCursor(Qt::OpenHandCursor);
            if (auto* grid = qobject_cast<CollectionGrid*>(parentWidget())) {
                grid->onCardDragEnd(this);
            }
            event->accept();
            return;
        }

        toggleSelected();
        event->accept();
        return;
    }

    QFrame::mouseReleaseEvent(event);
}

void CollectionCard::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    updateButtonLayout();
}

void CollectionCard::toggleSelected()
{
    m_selected = !m_selected;
    update();
    emit selectionChanged(m_itemId, m_selected);
}
