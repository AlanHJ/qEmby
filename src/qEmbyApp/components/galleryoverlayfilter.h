#include <QObject>





class QListView;
class QPushButton;
class GalleryOverlayFilter : public QObject {
public:
    GalleryOverlayFilter(QListView* list, QPushButton* left, QPushButton* right, QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
private:
    QListView* m_list;
    QPushButton* m_left;
    QPushButton* m_right;
};
