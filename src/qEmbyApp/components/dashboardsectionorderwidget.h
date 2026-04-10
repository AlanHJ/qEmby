#ifndef DASHBOARDSECTIONORDERWIDGET_H
#define DASHBOARDSECTIONORDERWIDGET_H

#include <QStringList>
#include <QWidget>

class DashboardSectionOrderStrip;
class QLabel;

class DashboardSectionOrderWidget : public QWidget {
    Q_OBJECT

public:
    explicit DashboardSectionOrderWidget(QWidget* parent = nullptr);

    void setSectionOrder(const QStringList& order);
    QStringList sectionOrder() const;

signals:
    void sectionOrderChanged(const QStringList& order);

private:
    QString titleForSectionId(const QString& sectionId) const;
    void updateSectionOrder(const QStringList& order);

    QLabel* m_hintLabel = nullptr;
    DashboardSectionOrderStrip* m_orderStrip = nullptr;
};

#endif 
