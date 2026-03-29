#ifndef TAGPRESETOPTIONROW_H
#define TAGPRESETOPTIONROW_H

#include <QFrame>
#include <QString>

class QCheckBox;
class QMouseEvent;

class TagPresetOptionRow : public QFrame {
public:
    explicit TagPresetOptionRow(const QString &text, QWidget *parent = nullptr);

    QCheckBox *checkBox() const;
    QString text() const;
    void setChecked(bool checked);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QCheckBox *m_checkBox = nullptr;
};

#endif 
