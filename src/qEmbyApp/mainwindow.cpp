#include "mainwindow.h"
#include <qembycore.h>
#include "views/public/loginview.h"
#include "views/media/playerview.h" 
#include "views/user/homeview.h"
#include "components/searchcompleterpopup.h"
#include "components/searchhistorypopup.h"
#include "managers/thememanager.h" 
#include "managers/searchhistorymanager.h"
#include "managers/traymanager.h"  
#include "config/configstore.h"    
#include "config/config_keys.h"    
#include "utils/contextmenuutils.h"
#include <services/manager/servermanager.h>
#include <QStackedWidget>
#include <QDebug>
#include <QApplication>
#include <QGuiApplication>
#include <QAbstractItemView>
#include <QFile>
#include <QTimer>
#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QAction>
#include <QCompleter>
#include <QFrame>
#include <QStringListModel>
#include <QPropertyAnimation>
#include <QScreen>
#include <QParallelAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <services/auth/authservice.h>
#include "components/themetransitionwidget.h"
#include "components/moderntoast.h" 
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QFocusEvent>
#include <QLineF>
#include <QtMath>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDialog>

#include <QWKWidgets/widgetwindowagent.h>
#include <widgetframe/windowbar.h>
#include <widgetframe/windowbutton.h>

static inline void emulateLeaveEvent(QWidget *widget) {
    Q_ASSERT(widget);
    if (!widget) return;
    QTimer::singleShot(0, widget, [widget]() {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        const QScreen *screen = widget->screen();
#else
        const QScreen *screen = widget->windowHandle()->screen();
#endif
        const QPoint globalPos = QCursor::pos(screen);
        if (!QRect(widget->mapToGlobal(QPoint{0, 0}), widget->size()).contains(globalPos)) {
            QCoreApplication::postEvent(widget, new QEvent(QEvent::Leave));
            if (widget->testAttribute(Qt::WA_Hover)) {
                const QPoint localPos = widget->mapFromGlobal(globalPos);
                const QPoint scenePos = widget->window()->mapFromGlobal(globalPos);
                static constexpr const auto oldPos = QPoint{};
                const Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
#if (QT_VERSION >= QT_VERSION_CHECK(6, 4, 0))
                const auto event = new QHoverEvent(QEvent::HoverLeave, scenePos, globalPos, oldPos, modifiers);
                Q_UNUSED(localPos);
#elif (QT_VERSION >= QT_VERSION_CHECK(6, 3, 0))
                const auto event =  new QHoverEvent(QEvent::HoverLeave, localPos, globalPos, oldPos, modifiers);
                Q_UNUSED(scenePos);
#else
                const auto event =  new QHoverEvent(QEvent::HoverLeave, localPos, oldPos, modifiers);
                Q_UNUSED(scenePos);
#endif
                QCoreApplication::postEvent(widget, event);
            }
        }
    });
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_core = new QEmbyCore(this);

    
    m_trayManager = new TrayManager(this);
    connect(m_trayManager, &TrayManager::showMainRequested, this, [this]() {
        showNormal();
        activateWindow();

        
        
        if (m_wasPausedByTray) {
            m_wasPausedByTray = false;
            QTimer::singleShot(500, this, [this]() {
                if (auto *player = m_homeView->activePlayerView()) {
                    player->resumePlayback();
                }
            });
        }
    });
    connect(m_trayManager, &TrayManager::quitRequested, this, [this]() {
        m_realQuit = true; 
        
        if (auto *player = m_homeView->activePlayerView()) {
            player->stopAndReport();
            
            QElapsedTimer wait;
            wait.start();
            while (wait.elapsed() < 500) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            }
        }
        if(m_core && m_core->authService()) {
            m_core->authService()->logout();
        }
        qApp->quit(); 
    });
    

    
    m_backClickTimer.invalidate();

    auto agent = new QWK::WidgetWindowAgent(this);
    agent->setup(this);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(241, 245, 249)); 
    setPalette(pal);
    setAutoFillBackground(true);

    
    auto titleLabel = new QLabel(qApp->applicationName());
    titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop); 
    titleLabel->setObjectName(QStringLiteral("win-title-label"));
    titleLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    titleLabel->setContentsMargins(0, 5, 0, 0);

    auto iconButton = new QWK::WindowButton();
    iconButton->setObjectName(QStringLiteral("icon-button"));
    iconButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto themeButton = new QWK::WindowButton();
    themeButton->setCheckable(true);
    themeButton->setObjectName(QStringLiteral("theme-button"));
    themeButton->setProperty("system-button", true);
    themeButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    themeButton->setIconNormal(QIcon(":/svg/moon.svg"));
    themeButton->setIconSize(QSize(15, 15));
    themeButton->setChecked(true);

    auto minButton = new QWK::WindowButton();
    minButton->setObjectName(QStringLiteral("min-button"));
    minButton->setProperty("system-button", true);
    minButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto maxButton = new QWK::WindowButton();
    maxButton->setCheckable(true);
    maxButton->setObjectName(QStringLiteral("max-button"));
    maxButton->setProperty("system-button", true);
    maxButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto closeButton = new QWK::WindowButton();
    closeButton->setObjectName(QStringLiteral("close-button"));
    closeButton->setProperty("system-button", true);
    closeButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto backButton = new QWK::WindowButton();
    backButton->setObjectName(QStringLiteral("back-button"));
    backButton->setProperty("system-button", true);
    backButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    backButton->setShortcut(QKeySequence::Back);
    backButton->hide();

    auto homeButton = new QWK::WindowButton();
    homeButton->setObjectName(QStringLiteral("home-button"));
    homeButton->setProperty("system-button", true);
    homeButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    homeButton->hide();

    auto favButton = new QWK::WindowButton();
    favButton->setObjectName(QStringLiteral("fav-button"));
    favButton->setProperty("system-button", true);
    favButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    favButton->hide();

    
    
    
    connect(backButton, &QWK::WindowButton::clicked, this, [this]() {
        if (m_viewStack->currentWidget() == m_homeView) {
            if (m_homeView->canNavigateBack()) {
                
                m_homeView->navigateBack();
                m_backClickTimer.invalidate(); 
            } else if (m_homeView->canGoHome()) {
                
                m_homeView->goHome();
                m_backClickTimer.invalidate();
            } else {
                
                if (m_backClickTimer.isValid() && m_backClickTimer.elapsed() < 2000) {
                    
                    m_backClickTimer.invalidate(); 
                    navigateToLogin(); 
                } else {
                    
                    m_backClickTimer.start();
                    ModernToast::showMessage(tr("Press Back again to logout"));
                }
            }
        }
    });

    connect(homeButton, &QWK::WindowButton::clicked, this, [this]() {
        if (m_viewStack->currentWidget() == m_homeView) {
            
            if (!m_homeView->canGoHome()) {
                ModernToast::showMessage(tr("Refreshing Home..."), 1000);
            }
            m_homeView->goHome();
        }
    });

    connect(favButton, &QWK::WindowButton::clicked, this, [this]() {
        if (m_viewStack->currentWidget() == m_homeView) {
            
            if (!m_homeView->canGoFav()) {
                ModernToast::showMessage(tr("Refreshing Favorites..."), 1000);
            }
            m_homeView->goFav();
        }
    });

    auto windowBar = new QWK::WindowBar();
    windowBar->setIconButton(iconButton);
    windowBar->setPinButton(themeButton);
    windowBar->setMinButton(minButton);
    windowBar->setMaxButton(maxButton);
    windowBar->setCloseButton(closeButton);
    windowBar->setTitleLabel(titleLabel);
    windowBar->setBackButton(backButton);
    windowBar->setHomeButton(homeButton);
    windowBar->setFavButton(favButton);
    windowBar->setHostWidget(this);

    
    m_globalSearchBox = new QLineEdit(windowBar);
    m_globalSearchBox->setObjectName("titlebar-search");
    m_globalSearchBox->setPlaceholderText(tr("Search..."));
    m_globalSearchBox->setFixedSize(380, 32); 
    m_globalSearchBox->setClearButtonEnabled(true);
    
    auto *searchAction = new QAction(QIcon(":/svg/light/search.svg"), tr("Search"), this);
    m_globalSearchBox->addAction(searchAction, QLineEdit::LeadingPosition);
    m_globalSearchBox->hide();

    connect(m_globalSearchBox, &QLineEdit::returnPressed, this,
            [this]() { submitGlobalSearch(m_globalSearchBox->text()); });

    auto *centerContainer = new QWidget(windowBar);
    auto *centerLayout = new QHBoxLayout(centerContainer);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);
    centerLayout->addStretch();
    centerLayout->addWidget(m_globalSearchBox, 0, Qt::AlignVCenter); 
    centerLayout->addStretch();
    centerContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    windowBar->setCenterWidget(centerContainer);


    agent->setTitleBar(windowBar);

    agent->setHitTestVisible(themeButton, true);
    agent->setHitTestVisible(m_globalSearchBox, true); 
    agent->setSystemButton(QWK::WindowAgentBase::WindowIcon, iconButton);
    agent->setSystemButton(QWK::WindowAgentBase::Minimize, minButton);
    agent->setSystemButton(QWK::WindowAgentBase::Maximize, maxButton);
    agent->setSystemButton(QWK::WindowAgentBase::Close, closeButton);
    agent->setSystemButton(QWK::WindowAgentBase::Back, backButton);
    agent->setSystemButton(QWK::WindowAgentBase::Home, homeButton);
    agent->setSystemButton(QWK::WindowAgentBase::Fav, favButton);

    setMenuWidget(windowBar);

    
    m_viewStack = new QStackedWidget(this);
    m_viewStack->setObjectName("main-view-stack"); 
    m_viewStack->setPalette(pal);
    m_viewStack->setAutoFillBackground(true);
    setCentralWidget(m_viewStack);

    m_loginView = new LoginView(m_core, this);
    m_homeView = new HomeView(m_core, this);
    setupGlobalSearchHistory();

    m_viewStack->addWidget(m_loginView);
    m_viewStack->addWidget(m_homeView);

    
    connect(m_viewStack, &QStackedWidget::currentChanged, this, [this](int index) {
        QWidget *currentView = m_viewStack->widget(index);
        if (!currentView) return;

        bool showSearch = currentView->property("showGlobalSearch").toBool();
        m_globalSearchBox->setVisible(showSearch);
        if (!showSearch) {
            hideGlobalSearchTransientUi();
        }
        
        auto back = findChild<QWK::WindowButton*>("back-button");
        auto icon = findChild<QWK::WindowButton*>("icon-button");
        auto home = findChild<QWK::WindowButton*>("home-button");
        auto fav = findChild<QWK::WindowButton*>("fav-button");
        
        
        if (currentView == m_homeView) {
            if (back) {
                back->setVisible(true);
                
                back->setEnabled(true); 
            }
            if (icon) icon->setVisible(false);
            if (home)
            {
                home->setVisible(true);
                
                home->setEnabled(true); 
            }
            if (fav)
            {
                fav->setVisible(true);
                
                fav->setEnabled(true); 
            }
        } else {
            
            bool showBack = currentView->property("showGlobalBack").toBool();
            if (back) {
                back->setVisible(showBack);
                back->setEnabled(true); 
            }
            if (icon) icon->setVisible(!showBack);
            bool showHome = currentView->property("showGlobalHome").toBool();
            if (home)
            {
                home->setVisible(showHome);
                home->setEnabled(true);
            }
            bool showFav = currentView->property("showGlobalFav").toBool();
            if (fav)
            {
                fav->setVisible(showFav);
                fav->setEnabled(true);
            }
        }

        QVariant customTitle = currentView->property("viewTitle");
        if (customTitle.isValid()) {
            if (auto tLabel = findChild<QLabel*>("win-title-label")) {
                tLabel->setText(customTitle.toString());
            }
        }
    });

    
    connect(m_homeView, &HomeView::immersiveStateChanged, this, [this](bool isImmersive) {
        if (this->menuWidget()) {
            this->menuWidget()->setVisible(!isImmersive);
        }
    });

    
    connect(m_homeView, &HomeView::canNavigateBackChanged, this, [this](bool canBack) {
        
        
        
        
        
    });

    connect(m_homeView, &HomeView::homeContentSwitched, this, [this] () {
        
        
        
        
        
        
        
    });

    connect(m_loginView, &LoginView::loginCompleted, this, &MainWindow::navigateToHome);
    connect(m_homeView, &HomeView::logoutRequested, this, &MainWindow::navigateToLogin);


    connect(windowBar, &QWK::WindowBar::pinRequested, this, [this, themeButton](bool checked) {
        themeButton->setChecked(checked);

        hideGlobalSearchTransientUi();

        
        QString newMode = checked ? "dark" : "light";
        
        themeButton->blockSignals(true);
        if (checked) {
            themeButton->setIconNormal(QIcon(":/svg/moon.svg"));
        } else {
            themeButton->setIconNormal(QIcon(":/svg/sun.svg"));
        }
        themeButton->blockSignals(false);

        
        bool reduceAnimations = ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
        if (reduceAnimations) {
            m_themeAnimating = true;
            ConfigStore::instance()->set(ConfigKeys::ThemeMode, newMode);
            m_themeAnimating = false;
            return;
        }

        
        QPixmap snapshot = this->grab();

        
        QPoint center = themeButton->mapTo(this, themeButton->rect().center());

        
        auto transition = new ThemeTransitionWidget(snapshot, center, this);
        transition->setGeometry(this->rect());
        transition->raise(); 
        transition->show();

        
        int maxRadius = qMax(
                            qMax(QLineF(center, QPoint(0, 0)).length(), QLineF(center, QPoint(width(), 0)).length()),
                            qMax(QLineF(center, QPoint(0, height())).length(), QLineF(center, QPoint(width(), height())).length())
                            ) + 10; 

        
        m_themeAnimating = true; 
        ConfigStore::instance()->set(ConfigKeys::ThemeMode, newMode);
        m_themeAnimating = false;

        
        auto anim = new QPropertyAnimation(transition, "radius", transition);
        anim->setDuration(450); 
        anim->setStartValue(0);
        anim->setEndValue(maxRadius);
        
        anim->setEasingCurve(QEasingCurve::InOutCubic);

        
        connect(anim, &QPropertyAnimation::finished, transition, &QObject::deleteLater);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    });


    connect(windowBar, &QWK::WindowBar::minimizeRequested, this, [this]() {
        hideGlobalSearchTransientUi();
        showMinimized();
    });
    connect(windowBar, &QWK::WindowBar::maximizeRequested, this, [this, maxButton](bool max) {
        if (max) {
            showMaximized();
        } else {
            showNormal();
            
            QRect screenGeo = screen()->availableGeometry();
            int w = 1280, h = 800;
            setGeometry(
                screenGeo.x() + (screenGeo.width() - w) / 2,
                screenGeo.y() + (screenGeo.height() - h) / 2,
                w, h);
        }
        emulateLeaveEvent(maxButton);
    });
    
    
    connect(windowBar, &QWK::WindowBar::closeRequested, this, &QWidget::close);

    
    QString savedThemeMode = ConfigStore::instance()->get<QString>(ConfigKeys::ThemeMode, "system");
    ThemeManager::instance()->applyThemeMode(savedThemeMode);
    
    bool isDark = ThemeManager::instance()->isDarkMode();
    themeButton->setChecked(isDark);
    themeButton->setIconNormal(isDark ? QIcon(":/svg/moon.svg") : QIcon(":/svg/sun.svg"));

    
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            [this, themeButton](const QString &key, const QVariant &newValue) {
        
        if (key == ConfigKeys::FontSize) {
            
            ThemeManager::instance()->setTheme(ThemeManager::instance()->currentTheme());
            return;
        }

        
        if (key != ConfigKeys::ThemeMode) return;
        QString mode = newValue.toString();
        hideGlobalSearchTransientUi();

        
        if (m_themeAnimating) {
            ThemeManager::instance()->applyThemeMode(mode);
            bool dark = ThemeManager::instance()->isDarkMode();
            themeButton->blockSignals(true);
            themeButton->setChecked(dark);
            themeButton->setIconNormal(dark ? QIcon(":/svg/moon.svg") : QIcon(":/svg/sun.svg"));
            themeButton->blockSignals(false);
            return;
        }

        
        bool reduceAnimations = ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
        if (!reduceAnimations) {
            
            QPixmap snapshot = this->grab();
            QPoint center = this->rect().center(); 

            
            ThemeManager::instance()->applyThemeMode(mode);
            bool dark = ThemeManager::instance()->isDarkMode();
            themeButton->blockSignals(true);
            themeButton->setChecked(dark);
            themeButton->setIconNormal(dark ? QIcon(":/svg/moon.svg") : QIcon(":/svg/sun.svg"));
            themeButton->blockSignals(false);

            
            auto transition = new ThemeTransitionWidget(snapshot, center, this);
            transition->setGeometry(this->rect());
            transition->raise();
            transition->show();

            int maxRadius = qMax(
                qMax(QLineF(center, QPoint(0, 0)).length(), QLineF(center, QPoint(width(), 0)).length()),
                qMax(QLineF(center, QPoint(0, height())).length(), QLineF(center, QPoint(width(), height())).length())
            ) + 10;

            
            auto anim = new QPropertyAnimation(transition, "radius", transition);
            anim->setDuration(450);
            anim->setStartValue(0);
            anim->setEndValue(maxRadius);
            anim->setEasingCurve(QEasingCurve::InOutCubic);
            connect(anim, &QPropertyAnimation::finished, transition, &QObject::deleteLater);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
            
            ThemeManager::instance()->applyThemeMode(mode);
            bool dark = ThemeManager::instance()->isDarkMode();
            themeButton->blockSignals(true);
            themeButton->setChecked(dark);
            themeButton->setIconNormal(dark ? QIcon(":/svg/moon.svg") : QIcon(":/svg/sun.svg"));
            themeButton->blockSignals(false);
        }
    });

    
    minButton->hide();
    maxButton->hide();
    m_viewStack->setCurrentWidget(m_loginView);
    resize(m_defaultWidth, m_defaultHeight);

    
    
    
    qApp->installEventFilter(this);
    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
                if (state != Qt::ApplicationActive) {
                    hideGlobalSearchTransientUi();
                }
            });
}

MainWindow::~MainWindow() {}

void MainWindow::setupGlobalSearchHistory()
{
    if (!m_globalSearchBox) {
        return;
    }

    m_globalSearchModel = new QStringListModel(this);
    m_globalSearchCompleter = new QCompleter(m_globalSearchModel, this);
    m_globalSearchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_globalSearchCompleter->setFilterMode(Qt::MatchContains);
    m_globalSearchCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_globalSearchCompleter->setMaxVisibleItems(8);
    m_globalSearchCompleter->setPopup(new SearchCompleterPopup());
    if (auto *popup = qobject_cast<SearchCompleterPopup *>(
            m_globalSearchCompleter->popup())) {
        popup->setMaxVisibleRows(m_globalSearchCompleter->maxVisibleItems());
    }
    m_globalSearchBox->setCompleter(m_globalSearchCompleter);

    m_globalSearchHistoryPopup = new SearchHistoryPopup(this);
    connect(m_globalSearchHistoryPopup, &SearchHistoryPopup::termActivated, this,
            [this](const QString &term) {
                m_globalSearchBox->setText(term);
                submitGlobalSearch(term);
            });
    connect(m_globalSearchHistoryPopup, &SearchHistoryPopup::clearHistoryRequested, this,
            [this]() {
                SearchHistoryManager::instance()->clearHistory(currentSearchServerId());
            });
    connect(m_globalSearchHistoryPopup, &SearchHistoryPopup::removeHistoryTermRequested,
            this, [this](const QString &term) {
                SearchHistoryManager::instance()->removeHistoryTerm(
                    currentSearchServerId(), term);
            });

    connect(m_globalSearchBox, &QLineEdit::textEdited, this,
            [this](const QString &text) {
                updateGlobalSearchCompleter(text);
                hideGlobalSearchTransientUi();
            });

    connect(SearchHistoryManager::instance(), &SearchHistoryManager::historyChanged,
            this, [this](const QString &serverId) {
                if (serverId == currentSearchServerId()) {
                    updateGlobalSearchCompleter(m_globalSearchBox
                                                    ? m_globalSearchBox->text()
                                                    : QString());
                }
            });
    connect(SearchHistoryManager::instance(), &SearchHistoryManager::enabledChanged,
            this, [this](bool enabled) {
                Q_UNUSED(enabled);
                updateGlobalSearchCompleter(m_globalSearchBox
                                                ? m_globalSearchBox->text()
                                                : QString());
                hideGlobalSearchTransientUi();
            });
    connect(SearchHistoryManager::instance(),
            &SearchHistoryManager::autocompleteEnabledChanged, this,
            [this](bool ) {
                updateGlobalSearchCompleter(m_globalSearchBox
                                                ? m_globalSearchBox->text()
                                                : QString());
            });

    if (m_core && m_core->serverManager()) {
        connect(m_core->serverManager(), &ServerManager::activeServerChanged, this,
                [this](const ServerProfile &profile) {
                    Q_UNUSED(profile);
                    updateGlobalSearchCompleter(m_globalSearchBox
                                                    ? m_globalSearchBox->text()
                                                    : QString());
                    hideGlobalSearchTransientUi();
                });
    }

    updateGlobalSearchCompleter();
}

void MainWindow::hideGlobalSearchTransientUi()
{
    if (m_globalSearchHistoryPopup) {
        const bool immediateHide =
            QGuiApplication::applicationState() != Qt::ApplicationActive ||
            !isVisible() || isMinimized() || !isActiveWindow();
        m_globalSearchHistoryPopup->dismiss(immediateHide);
    }
    if (m_globalSearchCompleter && m_globalSearchCompleter->popup()) {
        m_globalSearchCompleter->popup()->hide();
    }
}

void MainWindow::updateGlobalSearchCompleter(const QString &text)
{
    if (!m_globalSearchModel || !m_globalSearchCompleter || !m_globalSearchBox) {
        return;
    }

    const QStringList suggestions =
        SearchHistoryManager::instance()->completionSuggestions(
            currentSearchServerId(), text, 8);
    m_globalSearchModel->setStringList(suggestions);

    if (!SearchHistoryManager::instance()->isAutocompleteEnabled() ||
        text.trimmed().isEmpty() ||
        suggestions.isEmpty() || !m_globalSearchBox->hasFocus()) {
        if (m_globalSearchCompleter->popup()) {
            m_globalSearchCompleter->popup()->hide();
        }
        return;
    }

    if (auto *popup = qobject_cast<SearchCompleterPopup *>(
            m_globalSearchCompleter->popup())) {
        popup->setHighlightText(text);
        popup->syncWidthToAnchor(m_globalSearchBox);
    }

    m_globalSearchCompleter->setCompletionPrefix(text);
    if (auto *popup = qobject_cast<SearchCompleterPopup *>(
            m_globalSearchCompleter->popup())) {
        m_globalSearchCompleter->complete(
            popup->popupRectForAnchor(m_globalSearchBox));
        return;
    }

    m_globalSearchCompleter->complete();
}

void MainWindow::showGlobalSearchHistoryPopup(const QString &filterText)
{
    if (!m_globalSearchBox || !m_globalSearchHistoryPopup ||
        !SearchHistoryManager::instance()->isEnabled()) {
        return;
    }

    if (!isVisible() || isMinimized() || !m_globalSearchBox->isVisible() ||
        !m_globalSearchBox->hasFocus() ||
        QGuiApplication::applicationState() != Qt::ApplicationActive ||
        !isActiveWindow()) {
        hideGlobalSearchTransientUi();
        return;
    }

    const QString serverId = currentSearchServerId();
    const auto historyEntries =
        SearchHistoryManager::instance()->historyEntries(serverId, filterText);

    m_globalSearchHistoryPopup->setEntries(historyEntries);
    if (!m_globalSearchHistoryPopup->hasContent()) {
        hideGlobalSearchTransientUi();
        return;
    }

    m_globalSearchHistoryPopup->dismiss(true);
    if (m_globalSearchCompleter && m_globalSearchCompleter->popup()) {
        m_globalSearchCompleter->popup()->hide();
    }
    m_globalSearchHistoryPopup->showBelow(m_globalSearchBox);
}

void MainWindow::submitGlobalSearch(const QString &query)
{
    if (!m_homeView || m_viewStack->currentWidget() != m_homeView) {
        return;
    }

    const QString trimmedQuery = query.trimmed();
    if (trimmedQuery.isEmpty()) {
        return;
    }

    hideGlobalSearchTransientUi();

    m_homeView->triggerSearch(trimmedQuery);
    m_globalSearchBox->clear();
    hideGlobalSearchTransientUi();
}

QString MainWindow::currentSearchServerId() const
{
    if (!m_core || !m_core->serverManager()) {
        return {};
    }
    return m_core->serverManager()->activeProfile().id;
}

void MainWindow::navigateToHome()
{
    
    m_homeView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    
    bool reduceAnimations = ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
    if (reduceAnimations) {
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

        QString windowState = ConfigStore::instance()->get<QString>(
            ConfigKeys::StartupWindowState, "normal");
        bool shouldMaximize = (windowState == "maximized");

        setMinimumSize(900, 600);
        if (auto* minBtn = findChild<QWidget*>("min-button")) minBtn->show();
        if (auto* maxBtn = findChild<QWidget*>("max-button")) maxBtn->show();

        m_viewStack->setCurrentWidget(m_homeView);

        if (shouldMaximize) {
            showMaximized();
        } else {
            QRect screenGeo = screen()->availableGeometry();
            int w = 1280, h = 800;
            setGeometry(
                screenGeo.x() + (screenGeo.width() - w) / 2,
                screenGeo.y() + (screenGeo.height() - h) / 2,
                w, h);
        }
        return;
    }

    
    auto *loginOpacity = new QGraphicsOpacityEffect(m_loginView);
    loginOpacity->setOpacity(1.0);
    m_loginView->setGraphicsEffect(loginOpacity);

    auto *fadeOut = new QPropertyAnimation(loginOpacity, "opacity", this);
    fadeOut->setDuration(120);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);

    connect(fadeOut, &QPropertyAnimation::finished, this, [this]() {
        
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

        
        QString windowState = ConfigStore::instance()->get<QString>(
            ConfigKeys::StartupWindowState, "normal");
        bool shouldMaximize = (windowState == "maximized");

        QRect startGeo = this->geometry();
        QRect targetGeo;
        if (shouldMaximize) {
            
            targetGeo = screen()->availableGeometry();
        } else {
            
            int targetWidth = 1280;
            int targetHeight = 800;
            targetGeo = QRect(
                startGeo.x() - (targetWidth - startGeo.width()) / 2,
                startGeo.y() - (targetHeight - startGeo.height()) / 2,
                targetWidth, targetHeight);

            
            
            QRect screenGeo = screen()->availableGeometry();
            if (targetGeo.left() < screenGeo.left())
                targetGeo.moveLeft(screenGeo.left());
            if (targetGeo.top() < screenGeo.top())
                targetGeo.moveTop(screenGeo.top());
            if (targetGeo.right() > screenGeo.right())
                targetGeo.moveRight(screenGeo.right());
            if (targetGeo.bottom() > screenGeo.bottom())
                targetGeo.moveBottom(screenGeo.bottom());
        }

        auto *geoAnim = new QPropertyAnimation(this, "geometry", this);
        
        geoAnim->setDuration(shouldMaximize ? 500 : 350);
        geoAnim->setStartValue(startGeo);
        geoAnim->setEndValue(targetGeo);
        geoAnim->setEasingCurve(QEasingCurve::InOutCubic);

        connect(geoAnim, &QPropertyAnimation::finished, this, [this, shouldMaximize]() {
            
            setMinimumSize(900, 600);
            
            if (auto* minBtn = findChild<QWidget*>("min-button")) minBtn->show();
            if (auto* maxBtn = findChild<QWidget*>("max-button")) maxBtn->show();

            
            m_loginView->setGraphicsEffect(nullptr);

            
            if (shouldMaximize) {
                showMaximized();
            }

            
            auto *homeOpacity = new QGraphicsOpacityEffect(m_homeView);
            homeOpacity->setOpacity(0.0);
            m_homeView->setGraphicsEffect(homeOpacity);

            
            m_viewStack->setCurrentWidget(m_homeView);

            auto *fadeIn = new QPropertyAnimation(homeOpacity, "opacity", this);
            fadeIn->setDuration(250);
            fadeIn->setStartValue(0.0);
            fadeIn->setEndValue(1.0);
            fadeIn->setEasingCurve(QEasingCurve::OutQuad);

            connect(fadeIn, &QPropertyAnimation::finished, this, [this]() {
                m_homeView->setGraphicsEffect(nullptr); 
            });
            fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
        });
        geoAnim->start(QAbstractAnimation::DeleteWhenStopped);
    });
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::navigateToLogin() {
    
    
    m_homeView->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    
    bool reduceAnimations = ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
    if (reduceAnimations) {
        if (auto* m = findChild<QWidget*>("min-button")) m->hide();
        if (auto* x = findChild<QWidget*>("max-button")) x->hide();

        if (isMaximized()) showNormal();

        setMinimumSize(0, 0);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

        m_viewStack->setCurrentWidget(m_loginView);
        resize(m_defaultWidth, m_defaultHeight);

        
        QRect screenGeo = screen()->availableGeometry();
        move(screenGeo.x() + (screenGeo.width() - m_defaultWidth) / 2,
             screenGeo.y() + (screenGeo.height() - m_defaultHeight) / 2);

        m_core->authService()->logout();
        return;
    }

    
    auto *homeOpacity = new QGraphicsOpacityEffect(m_homeView);
    homeOpacity->setOpacity(1.0);
    m_homeView->setGraphicsEffect(homeOpacity);
    
    auto *fadeOut = new QPropertyAnimation(homeOpacity, "opacity", this);
    fadeOut->setDuration(120);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);

    connect(fadeOut, &QPropertyAnimation::finished, this, [this]() {
        
        if (auto* m = findChild<QWidget*>("min-button")) m->hide();
        if (auto* x = findChild<QWidget*>("max-button")) x->hide();

        
        if (isMaximized()) {
            QRect maxGeo = geometry(); 
            showNormal();
            setGeometry(maxGeo);       
        }

        
        setMinimumSize(0, 0); 
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        
        QRect startGeo = geometry();
        int targetWidth = m_defaultWidth;
        int targetHeight = m_defaultHeight;
        
        
        QRect targetGeo(startGeo.x() + (startGeo.width() - targetWidth) / 2,
                        startGeo.y() + (startGeo.height() - targetHeight) / 2,
                        targetWidth, targetHeight);

        auto *geoAnim = new QPropertyAnimation(this, "geometry", this);
        
        geoAnim->setDuration(startGeo.width() > 1300 ? 500 : 350);
        geoAnim->setStartValue(startGeo);
        geoAnim->setEndValue(targetGeo);
        geoAnim->setEasingCurve(QEasingCurve::InOutCubic);

        connect(geoAnim, &QPropertyAnimation::finished, this, [this]() {
            
            
            
            m_homeView->setGraphicsEffect(nullptr);
            
            
            auto *loginOpacity = new QGraphicsOpacityEffect(m_loginView);
            loginOpacity->setOpacity(0.0);
            m_loginView->setGraphicsEffect(loginOpacity);
            
            m_viewStack->setCurrentWidget(m_loginView);

            auto *fadeIn = new QPropertyAnimation(loginOpacity, "opacity", this);
            fadeIn->setDuration(250);
            fadeIn->setStartValue(0.0);
            fadeIn->setEndValue(1.0);
            fadeIn->setEasingCurve(QEasingCurve::OutQuad);

            connect(fadeIn, &QPropertyAnimation::finished, this, [this]() {
                m_loginView->setGraphicsEffect(nullptr);
                
                
                m_core->authService()->logout();
            });
            fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
        });
        geoAnim->start(QAbstractAnimation::DeleteWhenStopped);
    });
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qApp && event->type() == QEvent::ApplicationStateChange) {
        if (QGuiApplication::applicationState() != Qt::ApplicationActive) {
            hideGlobalSearchTransientUi();
        }
    }

    if (watched == this) {
        if (event->type() == QEvent::WindowDeactivate ||
            event->type() == QEvent::Move ||
            event->type() == QEvent::Resize ||
            event->type() == QEvent::Hide ||
            event->type() == QEvent::Close ||
            event->type() == QEvent::WindowStateChange) {
            hideGlobalSearchTransientUi();
        }
    }

    if (watched == m_globalSearchHistoryPopup && m_globalSearchHistoryPopup) {
        if (event->type() == QEvent::WindowDeactivate ||
            event->type() == QEvent::Hide ||
            event->type() == QEvent::Close) {
            hideGlobalSearchTransientUi();
        }
    }

    if (watched == m_globalSearchBox && m_globalSearchBox) {
        if (event->type() == QEvent::FocusIn) {
            auto *focusEvent = static_cast<QFocusEvent *>(event);
            if (focusEvent->reason() != Qt::MouseFocusReason &&
                m_globalSearchBox->text().trimmed().isEmpty()) {
                QTimer::singleShot(0, this, [this]() {
                    showGlobalSearchHistoryPopup();
                });
            }
        } else if (event->type() == QEvent::MouseButtonPress) {
            if (m_globalSearchBox->text().trimmed().isEmpty()) {
                QTimer::singleShot(0, this, [this]() {
                    showGlobalSearchHistoryPopup();
                });
            }
        } else if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Down) {
                showGlobalSearchHistoryPopup(m_globalSearchBox->text());
                return true;
            }
        } else if (event->type() == QEvent::Hide) {
            hideGlobalSearchTransientUi();
        }
    }

    if ((event->type() == QEvent::MouseButtonPress ||
         event->type() == QEvent::MouseButtonDblClick) &&
        m_globalSearchHistoryPopup && m_globalSearchHistoryPopup->isVisible() &&
        m_globalSearchBox) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QPoint globalPos = mouseEvent->globalPosition().toPoint();
        const QRect popupRect(m_globalSearchHistoryPopup->mapToGlobal(QPoint(0, 0)),
                              m_globalSearchHistoryPopup->size());
        const QRect searchRect(m_globalSearchBox->mapToGlobal(QPoint(0, 0)),
                               m_globalSearchBox->size());
        if (!popupRect.contains(globalPos) && !searchRect.contains(globalPos)) {
            hideGlobalSearchTransientUi();
        }
    }

    
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::BackButton || mouseEvent->button() == Qt::XButton1) {
            
            
            if (auto *modal = QApplication::activeModalWidget()) {
                if (auto *dlg = qobject_cast<QDialog *>(modal)) {
                    dlg->reject();
                }
                return true;
            }
            auto back = findChild<QWK::WindowButton*>("back-button");
            
            if (back && back->isVisible()) {
                back->click(); 
                return true;   
            }
        }
    } else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::BackButton || mouseEvent->button() == Qt::XButton1) {
            auto back = findChild<QWK::WindowButton*>("back-button");
            if (back && back->isVisible()) {
                return true; 
            }
        }
    }

    if (ContextMenuUtils::showStyledTextContextMenu(watched, event)) {
        return true;
    }
    if (ContextMenuUtils::showStyledLabelContextMenu(watched, event)) {
        return true;
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    hideGlobalSearchTransientUi();

    
    bool useTray = ConfigStore::instance()->get<bool>(ConfigKeys::CloseToTray, false);

    
    if (!m_realQuit && useTray && QSystemTrayIcon::isSystemTrayAvailable()) {
        
        if (auto *player = m_homeView->activePlayerView()) {
            if (player->isMediaPlaying()) {
                player->pausePlayback();
                m_wasPausedByTray = true;
            }
        }

        hide();             
        event->ignore();    
    } else {
        
        if (auto *player = m_homeView->activePlayerView()) {
            player->stopAndReport();
            
            QElapsedTimer wait;
            wait.start();
            while (wait.elapsed() < 500) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            }
        }
        event->accept();    
    }
}
