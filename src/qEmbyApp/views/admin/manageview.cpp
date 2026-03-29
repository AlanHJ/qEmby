#include "manageview.h"
#include "managepagebase.h"
#include "pagedashboard.h"
#include "pagecollections.h"
#include "pagelibraries.h"
#include "pagelogs.h"
#include "pagetasks.h"
#include "pagetranscoding.h"
#include "pageusers.h"
#include "../../components/slidingstackedwidget.h"
#include "../../managers/thememanager.h"
#include <qembycore.h>
#include <services/manager/servermanager.h>
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWheelEvent>

ManageView::ManageView(QEmbyCore *core, QWidget *parent)
    : BaseView(core, parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("SettingsRootView"); 

    setupUi();
    setupConnections();
}





void ManageView::showEvent(QShowEvent* event)
{
    BaseView::showEvent(event);
    m_core->serverManager()->connectWebSocket();
    qDebug() << "[ManageView] Entered management view — WebSocket connecting";
}

void ManageView::hideEvent(QHideEvent* event)
{
    m_core->serverManager()->disconnectWebSocket();
    qDebug() << "[ManageView] Left management view — WebSocket disconnected";
    BaseView::hideEvent(event);
}

void ManageView::setupUi() {
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    
    m_leftPanel = new QWidget(this);
    m_leftPanel->setFixedWidth(260);
    m_leftPanel->setObjectName("SettingsLeftPanel"); 
    auto *leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(32, 32, 32, 32);
    leftLayout->setSpacing(24);

    m_titleLabel = new QLabel(tr("Management"), m_leftPanel);
    m_titleLabel->setObjectName("SettingsMainTitle"); 

    m_navMenu = new QListWidget(m_leftPanel);
    m_navMenu->setObjectName("SettingsNavMenu"); 
    m_navMenu->setFocusPolicy(Qt::NoFocus);
    m_navMenu->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navMenu->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    
    auto *itemDashboard = new QListWidgetItem(
        ThemeManager::getAdaptiveIcon(":/svg/dark/manage-dashboard.svg"), tr(" Dashboard"));
    itemDashboard->setData(Qt::UserRole, ":/svg/dark/manage-dashboard.svg");

    auto *itemLibraries = new QListWidgetItem(
        ThemeManager::getAdaptiveIcon(":/svg/dark/manage-libraries.svg"), tr(" Libraries"));
    itemLibraries->setData(Qt::UserRole, ":/svg/dark/manage-libraries.svg");

    auto *itemCollections = new QListWidgetItem(
        ThemeManager::getAdaptiveIcon(":/svg/dark/manage-collections.svg"), tr(" Lists"));
    itemCollections->setData(Qt::UserRole, ":/svg/dark/manage-collections.svg");

    auto *itemTranscoding = new QListWidgetItem(
        ThemeManager::getAdaptiveIcon(":/svg/dark/video-output.svg"), tr(" Transcoding"));
    itemTranscoding->setData(Qt::UserRole, ":/svg/dark/video-output.svg");

    auto *itemUsers = new QListWidgetItem(
        ThemeManager::getAdaptiveIcon(":/svg/dark/manage-users.svg"), tr(" Users"));
    itemUsers->setData(Qt::UserRole, ":/svg/dark/manage-users.svg");

    auto *itemTasks = new QListWidgetItem(
        ThemeManager::getAdaptiveIcon(":/svg/dark/manage-tasks.svg"), tr(" Tasks"));
    itemTasks->setData(Qt::UserRole, ":/svg/dark/manage-tasks.svg");

    auto *itemLogs = new QListWidgetItem(
        ThemeManager::getAdaptiveIcon(":/svg/dark/manage-logs.svg"), tr(" Logs"));
    itemLogs->setData(Qt::UserRole, ":/svg/dark/manage-logs.svg");

    
    QSize itemSize(220, 44);
    itemDashboard->setSizeHint(itemSize);
    itemLibraries->setSizeHint(itemSize);
    itemCollections->setSizeHint(itemSize);
    itemTranscoding->setSizeHint(itemSize);
    itemUsers->setSizeHint(itemSize);
    itemTasks->setSizeHint(itemSize);
    itemLogs->setSizeHint(itemSize);

    m_navMenu->addItem(itemDashboard);
    m_navMenu->addItem(itemLibraries);
    m_navMenu->addItem(itemCollections);
    m_navMenu->addItem(itemTranscoding);
    m_navMenu->addItem(itemUsers);
    m_navMenu->addItem(itemTasks);
    m_navMenu->addItem(itemLogs);

    leftLayout->addWidget(m_titleLabel);
    leftLayout->addWidget(m_navMenu);

    
    m_stack = new SlidingStackedWidget(this);
    m_stack->setObjectName("SettingsStack"); 

    
    auto wrapInScrollArea = [this](QWidget *page) -> QScrollArea * {
        page->setAttribute(Qt::WA_StyledBackground, true);

        auto *scroll = new QScrollArea();
        scroll->setObjectName("SettingsScrollArea");
        scroll->setWidget(page);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->viewport()->setAutoFillBackground(false);

        m_scrollAreas.append(scroll);
        scroll->viewport()->installEventFilter(this);

        return scroll;
    };

    
    m_stack->addWidget(wrapInScrollArea(new PageDashboard(m_core, m_stack)));

    
    auto *libPage = new PageLibraries(m_core, m_stack);
    libPage->setAttribute(Qt::WA_StyledBackground, true);
    m_stack->addWidget(libPage);
    
    m_scrollAreas.append(libPage->scrollArea());
    libPage->scrollArea()->viewport()->installEventFilter(this);

    
    auto *collPage = new PageCollections(m_core, m_stack);
    collPage->setAttribute(Qt::WA_StyledBackground, true);
    m_stack->addWidget(collPage);
    m_scrollAreas.append(collPage->scrollArea());
    collPage->scrollArea()->viewport()->installEventFilter(this);

    auto *transcodingPage = new PageTranscoding(m_core, m_stack);
    transcodingPage->setAttribute(Qt::WA_StyledBackground, true);
    m_stack->addWidget(transcodingPage);
    m_scrollAreas.append(transcodingPage->scrollArea());
    transcodingPage->scrollArea()->viewport()->installEventFilter(this);

    m_stack->addWidget(wrapInScrollArea(new PageUsers(m_core, m_stack)));

    auto *taskPage = new PageTasks(m_core, m_stack);
    taskPage->setAttribute(Qt::WA_StyledBackground, true);
    m_stack->addWidget(taskPage);
    m_scrollAreas.append(taskPage->scrollArea());
    taskPage->scrollArea()->viewport()->installEventFilter(this);

    auto *logPage = new PageLogs(m_core, m_stack);
    logPage->setAttribute(Qt::WA_StyledBackground, true);
    m_stack->addWidget(logPage);

    mainLayout->addWidget(m_leftPanel);
    mainLayout->addWidget(m_stack, 1);

    m_navMenu->setCurrentRow(0);

    
    m_scrollTargets.resize(m_scrollAreas.size(), 0);
    for (auto *sa : m_scrollAreas) {
        auto *anim = new QPropertyAnimation(sa->verticalScrollBar(), "value", this);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setDuration(450);
        m_scrollAnims.append(anim);
    }

    qApp->postEvent(this, new QEvent(QEvent::StyleChange));
}

void ManageView::setupConnections() {
    connect(m_navMenu, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) {
            m_stack->setCurrentIndex(row);
        }
    });

    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            &ManageView::onThemeChanged);
}

void ManageView::onThemeChanged() {
    for (int i = 0; i < m_navMenu->count(); ++i) {
        auto *item = m_navMenu->item(i);
        QString svgPath = item->data(Qt::UserRole).toString();
        if (!svgPath.isEmpty()) {
            item->setIcon(ThemeManager::getAdaptiveIcon(svgPath));
        }
    }
}

bool ManageView::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::Wheel) {
        for (int i = 0; i < m_scrollAreas.size(); ++i) {
            if (obj == m_scrollAreas[i]->viewport()) {
                auto *we   = static_cast<QWheelEvent *>(event);
                auto *vBar = m_scrollAreas[i]->verticalScrollBar();
                auto *anim = m_scrollAnims[i];

                if (vBar) {
                    int currentVal = vBar->value();

                    if (anim->state() == QAbstractAnimation::Running) {
                        currentVal = m_scrollTargets[i];
                    }

                    int step      = we->angleDelta().y();
                    int newTarget = currentVal - step;
                    newTarget = qBound(vBar->minimum(), newTarget, vBar->maximum());

                    if (newTarget != vBar->value()) {
                        m_scrollTargets[i] = newTarget;
                        anim->stop();
                        anim->setStartValue(vBar->value());
                        anim->setEndValue(newTarget);
                        anim->start();
                    }
                }
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}
