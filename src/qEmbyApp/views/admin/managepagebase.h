#ifndef MANAGEPAGEBASE_H
#define MANAGEPAGEBASE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QString>

class QEmbyCore;
class QLabel;

class ManagePageBase : public QWidget {
    Q_OBJECT
public:
    explicit ManagePageBase(QEmbyCore* core, const QString& pageTitle, QWidget* parent = nullptr);
    ~ManagePageBase() override = default;

protected:
    QEmbyCore* m_core;
    QVBoxLayout* m_mainLayout;
};

#endif 
