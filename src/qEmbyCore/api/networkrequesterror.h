#ifndef NETWORKREQUESTERROR_H
#define NETWORKREQUESTERROR_H

#include "../qEmbyCore_global.h"
#include <QByteArray>
#include <QString>
#include <stdexcept>



class QEMBYCORE_EXPORT NetworkRequestError : public std::runtime_error
{
public:
    NetworkRequestError(const QString& message, int httpStatus,
                        const QByteArray& responseBody)
        : std::runtime_error(message.toStdString()),
          m_httpStatus(httpStatus),
          m_responseBody(responseBody.left(4096))
    {
    }

    int httpStatus() const noexcept { return m_httpStatus; }
    const QByteArray& responseBody() const noexcept { return m_responseBody; }

private:
    int m_httpStatus;
    QByteArray m_responseBody;
};

#endif 
