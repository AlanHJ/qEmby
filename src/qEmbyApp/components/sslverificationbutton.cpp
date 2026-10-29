#include "sslverificationbutton.h"

#include <QDebug>

SslVerificationButton::SslVerificationButton(QWidget *parent)
    : InlineToggleButton(parent)
{
    const QString title = tr("Ignore SSL certificate verification");
    setAccessibleName(title);
    setToolTip(title + QLatin1Char('\n') + tr(
        "Use this only for trusted self-signed or private CA HTTPS servers."));
    setText(tr("Ignore certificate"));
    setHttpsEnabled(false);

    connect(this, &QToolButton::toggled, this, [this](bool checked) {
        qDebug() << "[SslVerificationButton] State changed"
                 << "| ignoreSslVerification:" << checked
                 << "| focused:" << hasFocus()
                 << "| hovered:" << underMouse();
    });
}

void SslVerificationButton::setHttpsEnabled(bool https)
{
    setVisible(https);
    setEnabled(https);
    if (!https)
        setChecked(false);
}
