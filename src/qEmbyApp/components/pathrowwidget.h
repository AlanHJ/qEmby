#ifndef PATHROWWIDGET_H
#define PATHROWWIDGET_H

#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;

class PathRowWidget : public QWidget {
    Q_OBJECT
public:
    explicit PathRowWidget(QWidget* parent = nullptr);

    QString path() const;
    void setPath(const QString& path);
    bool isEmpty() const;

signals:
    void browseClicked();
    void removeClicked();

private:
    QLabel* m_pathLabel;
    QPushButton* m_browseBtn;
    QPushButton* m_removeBtn;
};

#endif 
