#ifndef SECURESECRETBOX_H
#define SECURESECRETBOX_H

#include "../qEmbyCore_global.h"

#include <QByteArray>
#include <QString>

#include <optional>

































class QEMBYCORE_EXPORT SecureSecretBox
{
public:
    

    
    
    static QByteArray encryptWithPassphrase(const QByteArray &plain, const QString &passphrase);

    
    static std::optional<QByteArray> decryptWithPassphrase(const QByteArray &cipher, const QString &passphrase);

    

    
    static QByteArray encryptLocalSecret(const QByteArray &plain);

    
    static std::optional<QByteArray> decryptLocalSecret(const QByteArray &cipher);

    

    
    static QByteArray pbkdf2Sha256(const QByteArray &password, const QByteArray &salt,
                                    int iterations, int outLen);

    
    static QByteArray hmacSha256(const QByteArray &data, const QByteArray &key);

    
    static QByteArray randomBytes(int n);

    
    static bool constantTimeEqual(const QByteArray &a, const QByteArray &b);

    
    static void secureZero(QByteArray &buf);

private:
    SecureSecretBox() = delete;
};

#endif 
