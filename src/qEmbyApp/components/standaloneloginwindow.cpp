#include "standaloneloginwindow.h"
#include "themetransitionwidget.h"

#include "config/config_keys.h"
#include "config/configstore.h"
#include "../managers/thememanager.h"
#include "../utils/layoututils.h"
#include <QApplication>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineF>
#include <QPropertyAnimation>
#include <QSignalBlocker>
#include <QVariant>
#include <QWKWidgets/widgetwindowagent.h>
#include <widgetframe/windowbar.h>
#include <widgetframe/windowbutton.h>

StandaloneLoginWindow::StandaloneLoginWindow(QWidget *content, const QSize &preferredSize)
    : QMainWindow(nullptr), m_preferredSize(preferredSize)
{
    
    setWindowIcon(QApplication::windowIcon());
    setWindowTitle(content->property("viewTitle").toString());
    setAttribute(Qt::WA_StyledBackground);
    auto *agent = new QWK::WidgetWindowAgent(this);
    agent->setup(this);

    auto *title = new QLabel(windowTitle());
    title->setObjectName(QStringLiteral("win-title-label"));
    title->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    title->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    title->setContentsMargins(0, 5, 0, 0);

    auto *icon = new QWK::WindowButton();
    icon->setObjectName(QStringLiteral("icon-button"));
    icon->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    icon->setIconNormal(windowIcon());
    icon->setIconSize(QSize(16, 16));

    auto *theme = new QWK::WindowButton();
    theme->setObjectName(QStringLiteral("theme-button"));
    theme->setProperty("system-button", true);
    theme->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    theme->setCheckable(true);
    theme->setIconSize(QSize(15, 15));
    const auto syncTheme = [theme]() {
        const QSignalBlocker blocker(theme);
        const bool dark = ThemeManager::instance()->isDarkMode();
        theme->setChecked(dark);
        theme->setIconNormal(QIcon(dark ? ":/svg/moon.svg" : ":/svg/sun.svg"));
    };
    syncTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, syncTheme);

    auto *closeButton = new QWK::WindowButton();
    closeButton->setObjectName(QStringLiteral("close-button"));
    closeButton->setProperty("system-button", true);
    closeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    auto *bar = new QWK::WindowBar();
    bar->setIconButton(icon);
    bar->setTitleLabel(title);
    
    
    auto *center = new QWidget(bar);
    center->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->setCenterWidget(center);
    bar->setPinButton(theme);
    bar->setCloseButton(closeButton);
    bar->setHostWidget(this);
    setMenuWidget(bar);
    agent->setTitleBar(bar);
    agent->setSystemButton(QWK::WindowAgentBase::WindowIcon, icon);
    agent->setSystemButton(QWK::WindowAgentBase::Close, closeButton);
    agent->setHitTestVisible(theme, true);
    connect(bar, &QWK::WindowBar::closeRequested, this, &QWidget::close);
    connect(bar, &QWK::WindowBar::pinRequested, this, [this, theme, syncTheme](bool checked) {
        
        
        if (m_themeAnimating) {
            syncTheme();
            return;
        }
        const QString mode = checked ? QStringLiteral("dark") : QStringLiteral("light");
        if (ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false)) {
            ConfigStore::instance()->set(ConfigKeys::ThemeMode, mode);
            return;
        }

        const QPixmap snapshot = grab();
        const QPoint center = theme->mapTo(this, theme->rect().center());
        auto *transition = new ThemeTransitionWidget(snapshot, center, this);
        transition->setGeometry(rect());
        transition->show();
        transition->raise();
        m_themeAnimating = true;

        
        
        ConfigStore::instance()->set(ConfigKeys::ThemeMode, mode);
        const int maxRadius = int(qMax(
            qMax(QLineF(center, rect().topLeft()).length(),
                 QLineF(center, rect().topRight()).length()),
            qMax(QLineF(center, rect().bottomLeft()).length(),
                 QLineF(center, rect().bottomRight()).length()))) + 10;
        auto *animation = new QPropertyAnimation(transition, "radius", transition);
        animation->setDuration(450);
        animation->setStartValue(0);
        animation->setEndValue(maxRadius);
        animation->setEasingCurve(QEasingCurve::InOutCubic);
        connect(animation, &QPropertyAnimation::finished, this, [this, transition]() {
            transition->hide();
            transition->deleteLater();
            m_themeAnimating = false;
        });
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    });

    setCentralWidget(content);
    
    
    content->show();
    LayoutUtils::fixWindowToContents(this, m_preferredSize);
}

bool StandaloneLoginWindow::event(QEvent *event)
{
    const bool handled = QMainWindow::event(event);
    if (event->type() == QEvent::LayoutRequest && centralWidget()) {
        LayoutUtils::fixWindowToContents(this, m_preferredSize);
    }
    return handled;
}

void StandaloneLoginWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    Q_EMIT closeRequested();
}

void StandaloneLoginWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}
