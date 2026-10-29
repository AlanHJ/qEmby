#ifndef MEDIAIMAGEEDITCARD_H
#define MEDIAIMAGEEDITCARD_H

#include <QFrame>
#include <QPixmap>
#include <QString>

class QLabel;
class QPushButton;
class QHBoxLayout;


class MediaImageEditCard : public QFrame
{
    Q_OBJECT
public:
    enum class Action { View, ChooseFile, ImageUrl, Download, Remove };
    Q_ENUM(Action)

    explicit MediaImageEditCard(QString title, QString details, bool hasImage,
                               QWidget* parent = nullptr);
    QLabel* previewLabel() const { return m_preview; }
    void setPreview(const QPixmap& pixmap);
    void setPreviewMessage(const QString& message);
    void setBusy(bool busy);
    void setDropActive(bool active);

signals:
    void actionRequested(MediaImageEditCard::Action action);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QPushButton* addAction(QHBoxLayout* layout, Action action,
                          const QString& tooltip, const QString& iconName);
    void updateIcons();

    QLabel* m_preview = nullptr;
    QPushButton* m_choose = nullptr;
    QPushButton* m_url = nullptr;
    QPushButton* m_download = nullptr;
    QPushButton* m_remove = nullptr;
    bool m_hasImage = false;
    bool m_busy = false;
    bool m_previewPressed = false;
};

#endif 
