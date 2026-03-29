#include "modernmenubutton.h"
#include <QAction>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>
#include <QWidgetAction>

namespace {

QWidget *createStreamMenuItemWidget(const QString &firstLineText,
                                    const QString &secondLine) {
    auto *host = new QWidget();
    host->setObjectName("stream-menu-item-host");
    host->setAttribute(Qt::WA_StyledBackground, true);

    auto *hostLayout = new QVBoxLayout(host);
    
    hostLayout->setContentsMargins(0, 1, 0, 1);
    hostLayout->setSpacing(0);

    auto *container = new QWidget(host);
    container->setObjectName("stream-menu-item");
    container->setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(14, 6, 28, 6);
    layout->setSpacing(secondLine.isEmpty() ? 0 : 2);

    auto *line1 = new QLabel(firstLineText, container);
    line1->setObjectName("stream-menu-line1");
    layout->addWidget(line1);

    if (!secondLine.isEmpty()) {
        auto *line2 = new QLabel(secondLine, container);
        line2->setObjectName("stream-menu-line2");
        layout->addWidget(line2);
    }

    hostLayout->addWidget(container);
    return host;
}

QWidget *streamMenuVisualWidget(QWidgetAction *action) {
    if (!action || !action->defaultWidget()) {
        return nullptr;
    }
    return action->defaultWidget()->findChild<QWidget *>(
        "stream-menu-item", Qt::FindChildrenRecursively);
}

void refreshStreamMenuItemState(QAction *action, bool selected) {
    action->setChecked(selected);

    auto *widgetAction = qobject_cast<QWidgetAction *>(action);
    if (!widgetAction) {
        return;
    }

    auto *visualWidget = streamMenuVisualWidget(widgetAction);
    if (!visualWidget) {
        return;
    }

    visualWidget->setProperty("checked", selected);
    visualWidget->style()->unpolish(visualWidget);
    visualWidget->style()->polish(visualWidget);
    visualWidget->update();

    const auto labels = visualWidget->findChildren<QLabel *>();
    for (auto *label : labels) {
        label->style()->unpolish(label);
        label->style()->polish(label);
        label->update();
    }
}

} 

ModernMenuButton::ModernMenuButton(QWidget *parent)
    : QPushButton(parent), m_currentIndex(-1)
{
    m_menu = new QMenu(this);

    setObjectName("immersive-stream-btn");
    m_menu->setObjectName("immersive-stream-menu");
    m_menu->setProperty("customItemHighlight", true);
    setStyleSheet("QPushButton::menu-indicator { image: none; }");

    
    m_menu->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    m_menu->setAttribute(Qt::WA_TranslucentBackground, true);
    m_menu->setAutoFillBackground(false);

    setCursor(Qt::PointingHandCursor);

    connect(m_menu, &QMenu::triggered, this, &ModernMenuButton::onActionTriggered);

    connect(this, &QPushButton::clicked, this, [this]() {
        if (m_actions.isEmpty()) {
            return;
        }
        m_menu->adjustSize();
        QPoint pos = mapToGlobal(QPoint(width() / 2, height() + 4));
        pos.setX(pos.x() - m_menu->sizeHint().width() / 2);
        m_menu->exec(pos);
    });
}

void ModernMenuButton::clear()
{
    m_menu->clear();
    m_actions.clear();
    m_currentIndex = -1;
    setText("");

    
    setToolTip("");
}

void ModernMenuButton::addItem(const QString& title, const QString& subtitle,
                                const QString& secondLine, const QVariant& userData)
{
    
    QString firstLineText = title;
    if (!subtitle.isEmpty()) {
        firstLineText += "   |   " + subtitle;
    }

    auto *widgetAction = new QWidgetAction(m_menu);
    widgetAction->setDefaultWidget(
        createStreamMenuItemWidget(firstLineText, secondLine));
    widgetAction->setProperty("shortTitle", title);
    widgetAction->setData(m_actions.size());
    widgetAction->setProperty("realData", userData);
    widgetAction->setProperty(
        "fullText", secondLine.isEmpty() ? firstLineText
                                          : firstLineText + "\n" + secondLine);
    widgetAction->setCheckable(true);

    m_menu->addAction(widgetAction);
    m_actions.append(widgetAction);

    if (m_actions.size() == 1) {
        setCurrentIndex(0);
    }
}

void ModernMenuButton::setCurrentIndex(int index)
{
    if (index >= 0 && index < m_actions.size()) {
        m_currentIndex = index;
        updateAppearance();
    } else if (index == -1) {
        m_currentIndex = -1;
        updateAppearance();
    }
}

QVariant ModernMenuButton::currentData() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_actions.size()) {
        return m_actions.at(m_currentIndex)->property("realData");
    }
    return QVariant();
}

void ModernMenuButton::onActionTriggered(QAction *action)
{
    int index = action->data().toInt();
    if (index != m_currentIndex) {
        m_currentIndex = index;
        updateAppearance();
        Q_EMIT currentIndexChanged(m_currentIndex);
    } else {
        action->setChecked(true);
    }
}

void ModernMenuButton::updateAppearance()
{
    for (int i = 0; i < m_actions.size(); ++i) {
        refreshStreamMenuItemState(m_actions.at(i), i == m_currentIndex);
    }

    if (m_currentIndex >= 0 && m_currentIndex < m_actions.size()) {
        QAction* currentAct = m_actions.at(m_currentIndex);
        QString shortText = currentAct->property("shortTitle").toString();

        setText(shortText + "  ⯆");

        
        setToolTip(currentAct->property("fullText").toString());
    } else {
        setText("");
        setToolTip(""); 
    }
}
