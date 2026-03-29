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

    
    QString fullText() const { return m_fullText; }

protected:
    void resizeEvent(QResizeEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    void updateElidedText();
    QString m_fullText;
};

#endif 
