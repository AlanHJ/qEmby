#include "modernsortbutton.h"
#include <QAction>

ModernSortButton::ModernSortButton(QWidget *parent)
    : QPushButton(parent), m_currentIndex(-1), m_descending(true)
{
    m_menu = new QMenu(this);

    
    

    setObjectName("modern-sort-btn");
    setCursor(Qt::PointingHandCursor);

    connect(m_menu, &QMenu::triggered, this, &ModernSortButton::onActionTriggered);

    
    connect(this, &QPushButton::clicked, this, [this]() {
        m_menu->adjustSize(); 
        QPoint pos = mapToGlobal(QPoint(width() / 2, height() + 6)); 
        pos.setX(pos.x() - m_menu->sizeHint().width() / 2);          
        m_menu->exec(pos);
    });
}

void ModernSortButton::setSortItems(const QStringList& items)
{
    m_options.clear();
    m_menu->clear();
    
    for (const QString& item : items) {
        addSortOption(item, item);
    }
}

void ModernSortButton::addSortOption(const QString& name, const QString& key)
{
    m_options.append({name, key});
    QAction* action = m_menu->addAction(name);
    
    action->setData(static_cast<int>(m_options.size() - 1));
    action->setCheckable(true);

    
    if (m_options.size() == 1) {
        m_currentIndex = 0;
        m_descending = true;
        updateAppearance();
    }
}

QString ModernSortButton::currentSortKey() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_options.size()) {
        return m_options.at(m_currentIndex).key;
    }
    return QString();
}

bool ModernSortButton::isAscending() const
{
    return !m_descending;
}


void ModernSortButton::setCurrentIndex(int index)
{
    if (index >= 0 && index < m_options.size() && m_currentIndex != index) {
        m_currentIndex = index;
        updateAppearance(); 
    }
}


void ModernSortButton::setDescending(bool descending)
{
    if (m_descending != descending) {
        m_descending = descending;
        updateAppearance(); 
    }
}

void ModernSortButton::onActionTriggered(QAction *action)
{
    int index = action->data().toInt();
    if (index == m_currentIndex) {
        
        m_descending = !m_descending;
    } else {
        
        m_currentIndex = index;
        m_descending = true;
    }

    updateAppearance();

    
    Q_EMIT sortChanged(currentSortKey(), isAscending());
}

void ModernSortButton::updateAppearance()
{
    const auto actionsList = m_menu->actions();
    for (int i = 0; i < actionsList.size(); ++i) {
        QAction* act = actionsList.at(i);
        if (i == m_currentIndex) {
            act->setChecked(true);
            act->setText(m_options.at(i).name + (m_descending ? " ↓" : " ↑"));
        } else {
            act->setChecked(false);
            act->setText(m_options.at(i).name);
        }
    }

    
    if (m_currentIndex >= 0 && m_currentIndex < m_options.size()) {
        setText(m_options.at(m_currentIndex).name + (m_descending ? " ↓" : " ↑"));
    }
}
