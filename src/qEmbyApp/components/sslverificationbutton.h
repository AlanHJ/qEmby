#ifndef SSLVERIFICATIONBUTTON_H
#define SSLVERIFICATIONBUTTON_H

#include "inlinetogglebutton.h"


class SslVerificationButton : public InlineToggleButton
{
    Q_OBJECT

public:
    explicit SslVerificationButton(QWidget *parent = nullptr);

    
    void setHttpsEnabled(bool https);
};

#endif 
