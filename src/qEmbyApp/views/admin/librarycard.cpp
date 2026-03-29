#include "librarycard.h"
#include "librarygrid.h"
#include "../../managers/thememanager.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QApplication>
#include <QHBoxLayout>




LibraryCard::LibraryCard(const VirtualFolder& folder, int index, QWidget* parent)
    : QFrame(parent), m_folder(folder), m_folderIdx(index)
{
    setFixedSize(LibraryGrid::CardWidth, LibraryGrid::CardHeight);
    setCursor(Qt::OpenHandCursor);
    setMouseTracking(true);

    
    auto makeBtn = [this](const QString& objName) {
        auto* btn = new QPushButton(this);
        btn->setObjectName(objName);
        btn->setFixedSize(24, 24);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    m_btnEdit   = makeBtn("LibCardEditBtn");
    m_btnScan   = makeBtn("LibCardScanBtn");
    m_btnDelete = makeBtn("LibCardDeleteBtn");

    m_btnEdit->setToolTip(tr("Edit"));
    m_btnScan->setToolTip(tr("Scan"));
    m_btnDelete->setToolTip(tr("Delete"));

    updateButtonLayout();

    connect(m_btnEdit,   &QPushButton::clicked, this, [this]() { emit editClicked(m_folderIdx); });
    connect(m_btnScan,   &QPushButton::clicked, this, [this]() { emit scanClicked(m_folderIdx); });
    connect(m_btnDelete, &QPushButton::clicked, this, [this]() { emit deleteClicked(m_folderIdx); });
}

void LibraryCard::updateButtonLayout()
{
    const int buttonSize = 24;
    const int buttonSpacing = 4;
    const int totalWidth = 3 * buttonSize + 2 * buttonSpacing;
    const int startX = width() - totalWidth - 8;
    const int buttonY = height() - buttonSize - 6;

    m_btnEdit->move(startX, buttonY);
    m_btnScan->move(startX + buttonSize + buttonSpacing, buttonY);
    m_btnDelete->move(startX + 2 * (buttonSize + buttonSpacing), buttonY);
}

void LibraryCard::setImage(const QPixmap& pixmap)
{
    m_image = pixmap;
    update();
}

void LibraryCard::setSelected(bool selected)
{
    if (m_selected == selected) {
        return;
    }

    m_selected = selected;
    update();
}

void LibraryCard::setScanProgress(int percent)
{
    m_scanProgress = qBound(0, percent, 100);
    update();
}

void LibraryCard::clearScanProgress()
{
    m_scanProgress = -1;
    update();
}

void LibraryCard::paintEvent(QPaintEvent* event)
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

    
    int imgAreaH = 120;
    QRectF imgRect(4, 4, w - 8, imgAreaH);

    if (!m_image.isNull()) {
        QPainterPath clipPath;
        clipPath.addRoundedRect(imgRect, 9, 9);
        p.save();
        p.setClipPath(clipPath);
        QPixmap scaled = m_image.scaled(
            imgRect.size().toSize() * devicePixelRatioF(),
            Qt::KeepAspectRatioByExpanding,
            Qt::SmoothTransformation);
        int xOff = (scaled.width() / devicePixelRatioF() - imgRect.width()) / 2;
        int yOff = (scaled.height() / devicePixelRatioF() - imgRect.height()) / 2;
        p.drawPixmap(imgRect.toRect(), scaled,
                     QRect(xOff * devicePixelRatioF(), yOff * devicePixelRatioF(),
                           imgRect.width() * devicePixelRatioF(),
                           imgRect.height() * devicePixelRatioF()));
        p.restore();
    } else {
        QLinearGradient grad(imgRect.topLeft(), imgRect.bottomRight());
        grad.setColorAt(0, isDark ? QColor(30, 41, 59) : QColor(241, 245, 249));
        grad.setColorAt(1, isDark ? QColor(51, 65, 85) : QColor(226, 232, 240));
        QPainterPath clipPath;
        clipPath.addRoundedRect(imgRect, 9, 9);
        p.save();
        p.setClipPath(clipPath);
        p.fillRect(imgRect, grad);

        QFont iconFont = font();
        iconFont.setPixelSize(32);
        p.setFont(iconFont);
        p.setPen(isDark ? QColor(148, 163, 184, 80) : QColor(100, 116, 139, 80));
        p.drawText(imgRect, Qt::AlignCenter, "📁");
        p.restore();
    }

    
    int textTop = imgAreaH + 10;
    int textLeft = 10;
    int textWidth = w - 20;

    QFont nameFont = font();
    nameFont.setPixelSize(13);
    nameFont.setWeight(QFont::DemiBold);
    nameFont.setFamilies({"Segoe UI", "Microsoft YaHei", "sans-serif"});
    QFontMetrics nameFm(nameFont);
    QString elidedName = nameFm.elidedText(m_folder.name, Qt::ElideRight, textWidth);

    p.setFont(nameFont);
    p.setPen(nameFg);
    p.drawText(QRect(textLeft, textTop, textWidth, 18), Qt::AlignLeft | Qt::AlignVCenter, elidedName);

    QFont typeFont = font();
    typeFont.setPixelSize(11);
    typeFont.setFamilies({"Segoe UI", "Microsoft YaHei", "sans-serif"});

    QString typeStr = m_folder.collectionTypeDisplayName();
    if (m_folder.locations.size() > 0) {
        typeStr += QString("  ·  %1 %2").arg(m_folder.locations.size())
                       .arg(m_folder.locations.size() > 1 ? tr("paths") : tr("path"));
    }

    p.setFont(typeFont);
    p.setPen(typeFg);
    p.drawText(QRect(textLeft, textTop + 20, textWidth, 16), Qt::AlignLeft | Qt::AlignVCenter, typeStr);

    
    if (m_scanProgress >= 0) {
        
        p.save();
        QPainterPath overlayPath;
        overlayPath.addRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 12, 12);
        p.setClipPath(overlayPath);
        p.fillRect(rect(), QColor(0, 0, 0, isDark ? 140 : 120));
        p.restore();

        
        int arcSize = 36;
        QRectF arcRect((w - arcSize) / 2.0, (h - arcSize) / 2.0 - 10, arcSize, arcSize);

        
        QPen bgPen(QColor(255, 255, 255, 50), 3, Qt::SolidLine, Qt::RoundCap);
        p.setPen(bgPen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(arcRect);

        
        QPen arcPen(QColor(255, 255, 255, 220), 3, Qt::SolidLine, Qt::RoundCap);
        p.setPen(arcPen);

        
        int spanAngle = -static_cast<int>(m_scanProgress * 3.6 * 16);
        p.drawArc(arcRect, 90 * 16, spanAngle);

        
        QFont pctFont = font();
        pctFont.setPixelSize(10);
        pctFont.setWeight(QFont::DemiBold);
        pctFont.setFamilies({"Segoe UI", "Microsoft YaHei", "sans-serif"});
        p.setFont(pctFont);
        p.setPen(QColor(255, 255, 255, 210));
        p.drawText(arcRect, Qt::AlignCenter, QString("%1%").arg(m_scanProgress));
    }
}

void LibraryCard::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    updateButtonLayout();
}

void LibraryCard::mousePressEvent(QMouseEvent* event)
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

void LibraryCard::mouseMoveEvent(QMouseEvent* event)
{
    if ((event->buttons() & Qt::LeftButton) && m_pressCandidate) {
        const QPoint globalPos = event->globalPosition().toPoint();
        if (!m_dragging) {
            const int distance = (globalPos - m_dragStartPos).manhattanLength();
            if (distance >= QApplication::startDragDistance()) {
                m_dragging = true;
                setCursor(Qt::ClosedHandCursor);
                raise();
                if (auto* grid = qobject_cast<LibraryGrid*>(parentWidget())) {
                    grid->onCardDragStart(this, globalPos);
                }
            }
        }

        if (m_dragging) {
            if (auto* grid = qobject_cast<LibraryGrid*>(parentWidget())) {
                grid->onCardDragMove(this, globalPos);
            }
            event->accept();
            return;
        }
    }

    QFrame::mouseMoveEvent(event);
}

void LibraryCard::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_pressCandidate) {
        const bool wasDragging = m_dragging;
        m_pressCandidate = false;

        if (wasDragging) {
            m_dragging = false;
            setCursor(Qt::OpenHandCursor);
            if (auto* grid = qobject_cast<LibraryGrid*>(parentWidget())) {
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

void LibraryCard::toggleSelected()
{
    m_selected = !m_selected;
    update();
    emit selectionChanged(m_folder.itemId, m_selected);
}
