#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QElapsedTimer> 
#include <QCloseEvent>   

class QEmbyCore;
class LoginView;
class HomeView;
class QStackedWidget;
class QLineEdit;
class QCompleter;
class QStringListModel;
class TrayManager;       
class SearchHistoryPopup;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public Q_SLOTS:
    void navigateToHome();
    void navigateToLogin();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override; 

private:
    void setupGlobalSearchHistory();
    void hideGlobalSearchTransientUi();
    void updateGlobalSearchCompleter(const QString &text = QString());
    void showGlobalSearchHistoryPopup(const QString &filterText = QString());
    void submitGlobalSearch(const QString &query);
    QString currentSearchServerId() const;

    QEmbyCore *m_core;
    QStackedWidget *m_viewStack;
    LoginView *m_loginView;
    HomeView *m_homeView;
    QLineEdit *m_globalSearchBox;
    QCompleter *m_globalSearchCompleter = nullptr;
    QStringListModel *m_globalSearchModel = nullptr;
    SearchHistoryPopup *m_globalSearchHistoryPopup = nullptr;
    TrayManager *m_trayManager = nullptr; 

    
    QElapsedTimer m_backClickTimer;

    quint32 m_defaultWidth{450};
    quint32 m_defaultHeight{320};
    bool m_realQuit{false}; 
    bool m_themeAnimating{false}; 
    bool m_wasPausedByTray{false}; 
};

#endif 
