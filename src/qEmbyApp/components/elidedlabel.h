#ifndef ELIDEDLABEL_H
#define ELIDEDLABEL_H

#include <QLabel>
#include <QResizeEvent>
#include <QString>

class ElidedLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ElidedLabel(QWidget* parent = nullptr);

    
    void setFullText(const QString& text);

    
    
    
    
    void setMaximumLines(int lines, bool reserveHeight = false);

    
    QString fullText() const { return m_fullText; }

signals:
    void elisionChanged(bool elided);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    void updateElidedText();
    QString m_fullText;
    int m_maximumLines = 1;
    bool m_reserveLineHeight = false;
};

#endif 
