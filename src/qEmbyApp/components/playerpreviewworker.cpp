#include "playerpreviewworker.h"

#include <utility>

PlayerPreviewWorker::PlayerPreviewWorker(QObject *parent)
    : QObject(parent)
{
}

void PlayerPreviewWorker::decode(MpvPreviewUtils::Request request)
{
    emit completed(m_session.decode(std::move(request)));
}

void PlayerPreviewWorker::reset()
{
    m_session.reset();
}

void PlayerPreviewWorker::shutdown()
{
    m_session.reset();
    emit stopped();
}
