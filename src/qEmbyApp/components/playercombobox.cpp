#include "playercombobox.h"
#include "../managers/thememanager.h"
#include <QPainter>
#include <QStylePainter>
#include <QStyleOptionComboBox>
#include <QMouseEvent>
#include <QAbstractItemView>
#include <QFileInfo>
#include <QApplication>



static constexpr int kItemHeight   = 36;
static constexpr int kIconSize     = 20;  
static constexpr int kBadgeSize    = 14;  
static constexpr int kDeleteSize   = 14;  
static constexpr int kPadding      = 10;
static constexpr int kSpacing      = 6;

PlayerItemDelegate::PlayerItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

QSize PlayerItemDelegate::sizeHint(const QStyleOptionViewItem & ,
                                   const QModelIndex & ) const {
    return QSize(200, kItemHeight);
}

QRect PlayerItemDelegate::deleteButtonRect(const QStyleOptionViewItem &option) const {
    
    int x = option.rect.right() - kPadding - kDeleteSize;
    int y = option.rect.top() + (option.rect.height() - kDeleteSize) / 2;
    return QRect(x, y, kDeleteSize, kDeleteSize);
}

int PlayerItemDelegate::hitDeleteButton(const QStyleOptionViewItem &option,
                                        const QPoint &pos) const {
    QRect btnRect = deleteButtonRect(option);
    
    btnRect.adjust(-4, -4, 4, 4);
    return btnRect.contains(pos) ? 0 : -1;
}

void PlayerItemDelegate::paint(QPainter *painter,
                               const QStyleOptionViewItem &option,
                               const QModelIndex &index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    bool isDark = ThemeManager::instance()->isDarkMode();
    QString source = index.data(PlayerComboBox::SourceRole).toString();
    QString path   = index.data(Qt::UserRole).toString();
    QString text   = index.data(Qt::DisplayRole).toString();
    bool isCustomEntry = (source == PlayerComboBox::SourceCustom);

    
    if (option.state & QStyle::State_Selected) {
        QColor sel = isDark ? QColor(59, 130, 246, 50) : QColor(59, 130, 246, 25);
        painter->fillRect(option.rect, sel);
    } else if (option.state & QStyle::State_MouseOver) {
        QColor hov = isDark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 12);
        painter->fillRect(option.rect, hov);
    }

    int x = option.rect.left() + kPadding;
    int centerY = option.rect.center().y();

    
    if (!isCustomEntry && !path.isEmpty()) {
        QFileInfo fi(path);
        QIcon exeIcon = m_iconProvider.icon(fi);
        if (!exeIcon.isNull()) {
            QRect iconRect(x, centerY - kIconSize / 2, kIconSize, kIconSize);
            exeIcon.paint(painter, iconRect);
        }
        x += kIconSize + kSpacing;
    }

    
    int rightX = option.rect.right() - kPadding;

    
    if (!isCustomEntry) {
        QRect delRect(rightX - kDeleteSize, centerY - kDeleteSize / 2,
                      kDeleteSize, kDeleteSize);
        QIcon closeIcon = ThemeManager::getAdaptiveIcon(
            ":/svg/dark/close.svg",
            (option.state & QStyle::State_MouseOver) 
                ? (isDark ? QColor("#EF4444") : QColor("#DC2626"))
                : (isDark ? QColor("#94A3B8") : QColor("#64748B")));
        closeIcon.paint(painter, delRect);
        rightX -= kDeleteSize + kSpacing;
    }

    
    if (!isCustomEntry) {
        QString badgeSvg = (source == PlayerComboBox::SourceAuto)
                           ? ":/svg/dark/search.svg"
                           : ":/svg/dark/external-player.svg";
        QRect badgeRect(rightX - kBadgeSize, centerY - kBadgeSize / 2,
                        kBadgeSize, kBadgeSize);
        QIcon badgeIcon = ThemeManager::getAdaptiveIcon(
            badgeSvg,
            isDark ? QColor("#64748B") : QColor("#94A3B8"));
        badgeIcon.paint(painter, badgeRect);
        rightX -= kBadgeSize + kSpacing;
    }

    
    QRect textRect(x, option.rect.top(), rightX - x - kSpacing, option.rect.height());
    QColor textColor = isDark ? QColor("#E2E8F0") : QColor("#1E293B");
    if (option.state & QStyle::State_Selected) {
        textColor = isDark ? QColor("#93C5FD") : QColor("#2563EB");
    }
    painter->setPen(textColor);
    QFont f = option.font;
    f.setPointSizeF(9.5);
    painter->setFont(f);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                      painter->fontMetrics().elidedText(text, Qt::ElideMiddle, textRect.width()));

    painter->restore();
}



PlayerComboBox::PlayerComboBox(QWidget *parent) : ModernComboBox(parent) {
    m_delegate = new PlayerItemDelegate(this);
    setItemDelegate(m_delegate);

    
    view()->installEventFilter(this);
    view()->viewport()->installEventFilter(this);
}

void PlayerComboBox::addPlayer(const QString &displayName, const QString &path,
                               const QString &source) {
    int idx = count();
    
    for (int i = 0; i < count(); ++i) {
        if (itemData(i, SourceRole).toString() == SourceCustom) {
            idx = i;
            break;
        }
    }
    insertItem(idx, displayName, path);
    setItemData(idx, source, SourceRole);
    updateGeometry(); 
}

void PlayerComboBox::removePlayer(int index) {
    if (index < 0 || index >= count()) return;

    QString source = itemData(index, SourceRole).toString();
    if (source == SourceCustom) return; 

    QString path = itemData(index).toString();
    bool wasCurrent = (currentIndex() == index);

    blockSignals(true);
    removeItem(index);
    blockSignals(false);
    updateGeometry(); 

    if (wasCurrent && count() > 0) {
        
        int newIdx = qMin(index, count() - 1);
        if (itemData(newIdx, SourceRole).toString() == SourceCustom && newIdx > 0) {
            newIdx--;
        }
        setCurrentIndex(newIdx);
    }

    emit playerRemoved(index, path);
}

void PlayerComboBox::removeAutoDetected() {
    for (int i = count() - 1; i >= 0; --i) {
        if (itemData(i, SourceRole).toString() == SourceAuto) {
            removeItem(i);
        }
    }
    updateGeometry();
}

QString PlayerComboBox::currentPlayerPath() const {
    return currentData(Qt::UserRole).toString();
}

void PlayerComboBox::paintEvent(QPaintEvent *) {
    QStylePainter painter(this);

    QStyleOptionComboBox opt;
    initStyleOption(&opt);

    
    QString text = opt.currentText;
    opt.currentText = "";

    
    painter.drawComplexControl(QStyle::CC_ComboBox, opt);

    
    const int arrowWidth = 32;
    const int iconSz = 18;
    const int iconPad = 6;
    QRect availRect = rect().adjusted(0, 0, -arrowWidth, 0);

    QString path = currentData(Qt::UserRole).toString();
    QString source = currentData(SourceRole).toString();
    int textLeft = availRect.left();

    if (source != SourceCustom && !path.isEmpty()) {
        QFileInfo fi(path);
        QIcon exeIcon = m_iconProvider.icon(fi);
        if (!exeIcon.isNull()) {
            int iconY = availRect.center().y() - iconSz / 2;
            int iconX = availRect.left() + iconPad;
            QRect iconRect(iconX, iconY, iconSz, iconSz);
            exeIcon.paint(&painter, iconRect);
            textLeft = iconX + iconSz + iconPad;
        }
    }

    
    QRect textRect(textLeft, availRect.top(), availRect.right() - textLeft, availRect.height());
    painter.setPen(opt.palette.color(QPalette::ButtonText));
    painter.setFont(this->font());
    painter.drawText(textRect, Qt::AlignCenter | Qt::TextSingleLine, text);
}


bool PlayerComboBox::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        QAbstractItemView *v = view();

        
        QModelIndex idx = v->indexAt(me->pos());
        if (idx.isValid()) {
            QString source = idx.data(SourceRole).toString();
            if (source != SourceCustom) {
                
                QStyleOptionViewItem opt;
                opt.rect = v->visualRect(idx);
                opt.state = QStyle::State_Enabled;

                if (m_delegate->hitDeleteButton(opt, me->pos()) >= 0) {
                    
                    removePlayer(idx.row());
                    event->accept();
                    return true; 
                }
            }
        }
    }
    return ModernComboBox::eventFilter(obj, event);
}
