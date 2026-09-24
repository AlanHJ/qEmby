#ifndef SIDEBARLIBRARYROW_H
#define SIDEBARLIBRARYROW_H
#include <QWidget>
#include <QString>
class QPushButton;

class SidebarLibraryRow : public QWidget
{
    Q_OBJECT
public:
    SidebarLibraryRow(QString serverId, QString libraryId, const QString &title,
                      QWidget *parent = nullptr);
protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
private:
    void refreshVisibility();
    QString m_serverId;
    QString m_libraryId;
    QPushButton *m_visibilityButton = nullptr;
};
#endif
