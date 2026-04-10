#include "dashboardsectionorderwidget.h"

#include "dashboardsectionorderstrip.h"
#include "../utils/dashboardsectionorderutils.h"
#include <QHash>
#include <QLabel>
#include <QVBoxLayout>

DashboardSectionOrderWidget::DashboardSectionOrderWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("DashboardSectionOrderWidget");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_hintLabel = new QLabel(
        tr("Drag the items below to change the order of home screen sections. "
           "Left to right matches top to bottom on the home screen."),
        this);
    m_hintLabel->setObjectName("SettingsCardDesc");
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    m_orderStrip = new DashboardSectionOrderStrip(this);
    layout->addWidget(m_orderStrip);

    connect(m_orderStrip, &DashboardSectionOrderStrip::sectionOrderChanged, this,
            &DashboardSectionOrderWidget::sectionOrderChanged);

    setSectionOrder(DashboardSectionOrderUtils::defaultSectionOrder());
}

void DashboardSectionOrderWidget::setSectionOrder(const QStringList& order)
{
    updateSectionOrder(DashboardSectionOrderUtils::normalizeSectionOrder(order));
}

QStringList DashboardSectionOrderWidget::sectionOrder() const
{
    if (!m_orderStrip) {
        return {};
    }

    return DashboardSectionOrderUtils::normalizeSectionOrder(
        m_orderStrip->sectionOrder());
}

QString DashboardSectionOrderWidget::titleForSectionId(
    const QString& sectionId) const
{
    if (sectionId == QLatin1String(
                         DashboardSectionOrderUtils::ContinueWatchingSectionId)) {
        return tr("Continue Watching");
    }
    if (sectionId ==
        QLatin1String(DashboardSectionOrderUtils::LatestMediaSectionId)) {
        return tr("Latest Media");
    }
    if (sectionId ==
        QLatin1String(DashboardSectionOrderUtils::RecommendedSectionId)) {
        return tr("Recommended");
    }
    if (sectionId ==
        QLatin1String(DashboardSectionOrderUtils::AllLibrariesSectionId)) {
        return tr("All Libraries");
    }
    if (sectionId == QLatin1String(
                         DashboardSectionOrderUtils::
                             EachLibrarySectionsSectionId)) {
        return tr("Each Library Sections");
    }

    return sectionId;
}

void DashboardSectionOrderWidget::updateSectionOrder(const QStringList& order)
{
    if (!m_orderStrip) {
        return;
    }

    QHash<QString, QString> titles;
    const QStringList normalizedOrder =
        DashboardSectionOrderUtils::normalizeSectionOrder(order);

    for (const QString& sectionId : normalizedOrder) {
        titles.insert(sectionId, titleForSectionId(sectionId));
    }

    m_orderStrip->setSectionOrder(normalizedOrder, titles);
}
