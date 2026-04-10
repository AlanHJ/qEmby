#ifndef PLAYLISTCREATEEDITORROW_H
#define PLAYLISTCREATEEDITORROW_H

#include <QWidget>

class QLineEdit;
class QPushButton;

class PlaylistCreateEditorRow : public QWidget {
    Q_OBJECT

public:
    explicit PlaylistCreateEditorRow(QWidget *parent = nullptr);

    QString text() const;
    void setText(const QString &text);
    void setPlaceholderText(const QString &text);
    void focusEditor();

signals:
    void confirmed(QString text);
    void cancelled();
    void textChanged(QString text);

private:
    void updateConfirmButtonState();

    QLineEdit *m_lineEdit = nullptr;
    QPushButton *m_confirmButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
};

#endif 
