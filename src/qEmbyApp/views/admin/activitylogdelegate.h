#ifndef ACTIVITYLOGDELEGATE_H
#define ACTIVITYLOGDELEGATE_H

#include <QStyledItemDelegate>
#include <QWidget>
#include <QColor>


class ActivityLogThemeHelper : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor cardBg READ cardBg WRITE setCardBg)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor)
    Q_PROPERTY(QColor nameColor READ nameColor WRITE setNameColor)
    Q_PROPERTY(QColor overviewColor READ overviewColor WRITE setOverviewColor)
    Q_PROPERTY(QColor timeColor READ timeColor WRITE setTimeColor)

public:
    explicit ActivityLogThemeHelper(QWidget *parent = nullptr);

    QColor cardBg() const { return m_cardBg; }
    void setCardBg(const QColor &c) { m_cardBg = c; }

    QColor borderColor() const { return m_borderColor; }
    void setBorderColor(const QColor &c) { m_borderColor = c; }

    QColor nameColor() const { return m_nameColor; }
    void setNameColor(const QColor &c) { m_nameColor = c; }

    QColor overviewColor() const { return m_overviewColor; }
    void setOverviewColor(const QColor &c) { m_overviewColor = c; }

    QColor timeColor() const { return m_timeColor; }
    void setTimeColor(const QColor &c) { m_timeColor = c; }

private:
    QColor m_cardBg;
    QColor m_borderColor;
    QColor m_nameColor;
    QColor m_overviewColor;
    QColor m_timeColor;
};

class ActivityLogDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ActivityLogDelegate(QObject* parent = nullptr);

    
    enum Roles {
        NameRole = Qt::UserRole + 1,
        OverviewRole,
        DateRole,
        SeverityRole,
        TypeRole
    };

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    QString formatExactTime(const QString& isoDate) const;
    QColor severityColor(const QString& severity) const;

    mutable ActivityLogThemeHelper* m_themeHelper = nullptr;
};

#endif 
