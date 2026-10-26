#ifndef STANDALONELOGINWINDOW_H
#define STANDALONELOGINWINDOW_H

#include <QMainWindow>



class StandaloneLoginWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit StandaloneLoginWindow(QWidget *content, const QSize &preferredSize);

Q_SIGNALS:
    void closeRequested();

protected:
    bool event(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QSize m_preferredSize;
    bool m_themeAnimating = false;
};

#endif
