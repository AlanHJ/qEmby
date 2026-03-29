#ifndef LIBRARYCARD_H
#define LIBRARYCARD_H

#include <QFrame>
#include <QPixmap>
#include <QPushButton>
#include <models/admin/adminmodels.h>

class LibraryGrid;




class LibraryCard : public QFrame {
    Q_OBJECT
    Q_PROPERTY(QPoint pos READ pos WRITE move)

public:
    explicit LibraryCard(const VirtualFolder& folder, int index, QWidget* parent = nullptr);

    int folderIndex() const { return m_folderIdx; }
    void setFolderIndex(int idx) { m_folderIdx = idx; }
    const VirtualFolder& folder() const { return m_folder; }
    QString itemId() const { return m_folder.itemId; }
    bool isSelected() const { return m_selected; }
    void setImage(const QPixmap& pixmap);
    void setSelected(bool selected);

    
    void setScanProgress(int percent);
    void clearScanProgress();
    bool isScanActive() const { return m_scanProgress >= 0; }

signals:
    void editClicked(int index);
    void scanClicked(int index);
    void deleteClicked(int index);
    void selectionChanged(const QString& itemId, bool selected);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void updateButtonLayout();
    void toggleSelected();

    VirtualFolder m_folder;
    int m_folderIdx;
    QPixmap m_image;
    bool m_selected = false;
    bool m_dragging = false;
    bool m_pressCandidate = false;
    QPoint m_dragStartPos;
    int m_scanProgress = -1;  

    QPushButton* m_btnEdit;
    QPushButton* m_btnScan;
    QPushButton* m_btnDelete;
};

#endif 
