#include "securesecretbox.h"

#include "aes_lite.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QStandardPaths>

#include <cstring>

namespace
{



constexpr char kMagic[4] = {'Q', 'S', 'B', '1'};
constexpr uint8_t kVersion = 0x01;
constexpr uint8_t kFlagLocal = 0x00;
constexpr uint8_t kFlagPassphrase = 0x01;

constexpr int kHeaderLen = 4  + 1  + 1  + 16  + 16 ;
constexpr int kSaltLen = 16;
constexpr int kNonceLen = 16;
constexpr int kAesKeyLen = 32;
constexpr int kHmacKeyLen = 32;
constexpr int kHmacLen = 32;
constexpr int kPbkdf2Iterations = 100000;
constexpr int kKdfOutLen = kAesKeyLen + kHmacKeyLen; 



QString localKeyFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(dir).filePath(QStringLiteral("secret-box-key.bin"));
}



QByteArray loadOrCreateLocalKeyMaterial()
{
    const QString path = localKeyFilePath();

    QFile file(path);
    if (file.exists() && file.open(QIODevice::ReadOnly))
    {
        const QByteArray content = file.readAll();
        file.close();
        if (content.size() == kKdfOutLen)
        {
            return content;
        }
        qWarning() << "[SecureSecretBox] local key file exists but size mismatch:"
                   << content.size() << "expected:" << kKdfOutLen
                   << "| path:" << path;
        
        return QByteArray();
    }

    
    const QString dirPath = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dirPath))
    {
        qWarning() << "[SecureSecretBox] failed to create local key dir:" << dirPath;
        return QByteArray();
    }

    QByteArray fresh = SecureSecretBox::randomBytes(kKdfOutLen);
    if (fresh.size() != kKdfOutLen)
    {
        qWarning() << "[SecureSecretBox] randomBytes failed when creating local key";
        return QByteArray();
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qWarning() << "[SecureSecretBox] failed to open local key file for write:" << path
                   << "|" << file.errorString();
        return QByteArray();
    }
    if (file.write(fresh) != fresh.size())
    {
        qWarning() << "[SecureSecretBox] failed to write local key file:" << path;
        file.close();
        return QByteArray();
    }
    file.close();

    
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    qDebug() << "[SecureSecretBox] generated new local key file at" << path;
    return fresh;
}


void splitKeyMaterial(const QByteArray &material, QByteArray &aesKey, QByteArray &hmacKey)
{
    aesKey = material.left(kAesKeyLen);
    hmacKey = material.mid(kAesKeyLen, kHmacKeyLen);
}



QByteArray encryptCommon(const QByteArray &plain, const QByteArray &aesKey,
                         const QByteArray &hmacKey, const QByteArray &salt,
                         uint8_t flags)
{
    if (aesKey.size() != kAesKeyLen || hmacKey.size() != kHmacKeyLen ||
        salt.size() != kSaltLen)
    {
        qWarning() << "[SecureSecretBox] encryptCommon: invalid key/salt sizes";
        return QByteArray();
    }

    const QByteArray nonce = SecureSecretBox::randomBytes(kNonceLen);
    if (nonce.size() != kNonceLen)
    {
        return QByteArray();
    }

    
    QByteArray cipher = plain; 
    AES_ctx ctx;
    AES_init_ctx_iv(&ctx,
                    reinterpret_cast<const uint8_t *>(aesKey.constData()),
                    reinterpret_cast<const uint8_t *>(nonce.constData()));
    AES_CTR_xcrypt_buffer(&ctx,
                          reinterpret_cast<uint8_t *>(cipher.data()),
                          static_cast<size_t>(cipher.size()));

    
    QByteArray header;
    header.reserve(kHeaderLen);
    header.append(kMagic, 4);
    header.append(static_cast<char>(kVersion));
    header.append(static_cast<char>(flags));
    header.append(salt);
    header.append(nonce);

    
    QByteArray macInput;
    macInput.reserve(header.size() + cipher.size());
    macInput.append(header);
    macInput.append(cipher);
    const QByteArray mac = SecureSecretBox::hmacSha256(macInput, hmacKey);
    if (mac.size() != kHmacLen)
    {
        return QByteArray();
    }

    QByteArray out;
    out.reserve(header.size() + cipher.size() + mac.size());
    out.append(header);
    out.append(cipher);
    out.append(mac);
    return out;
}


bool parseHeader(const QByteArray &cipher, uint8_t &flagsOut, QByteArray &saltOut,
                 QByteArray &nonceOut, QByteArray &payloadOut, QByteArray &macOut)
{
    if (cipher.size() < kHeaderLen + kHmacLen)
    {
        return false;
    }

    if (std::memcmp(cipher.constData(), kMagic, 4) != 0)
    {
        return false;
    }
    if (static_cast<uint8_t>(cipher.at(4)) != kVersion)
    {
        return false;
    }

    flagsOut = static_cast<uint8_t>(cipher.at(5));
    saltOut = cipher.mid(6, kSaltLen);
    nonceOut = cipher.mid(6 + kSaltLen, kNonceLen);

    const int payloadLen = cipher.size() - kHeaderLen - kHmacLen;
    payloadOut = cipher.mid(kHeaderLen, payloadLen);
    macOut = cipher.right(kHmacLen);
    return true;
}

std::optional<QByteArray> decryptCommon(const QByteArray &cipher, const QByteArray &aesKey,
                                        const QByteArray &hmacKey)
{
    uint8_t flags = 0;
    QByteArray salt, nonce, payload, mac;
    if (!parseHeader(cipher, flags, salt, nonce, payload, mac))
    {
        qWarning() << "[SecureSecretBox] decrypt: header parse failed";
        return std::nullopt;
    }

    
    const QByteArray header = cipher.left(kHeaderLen);
    QByteArray macInput;
    macInput.reserve(header.size() + payload.size());
    macInput.append(header);
    macInput.append(payload);
    const QByteArray expectedMac = SecureSecretBox::hmacSha256(macInput, hmacKey);
    if (!SecureSecretBox::constantTimeEqual(expectedMac, mac))
    {
        qWarning() << "[SecureSecretBox] decrypt: HMAC mismatch";
        return std::nullopt;
    }

    
    QByteArray plain = payload;
    AES_ctx ctx;
    AES_init_ctx_iv(&ctx,
                    reinterpret_cast<const uint8_t *>(aesKey.constData()),
                    reinterpret_cast<const uint8_t *>(nonce.constData()));
    AES_CTR_xcrypt_buffer(&ctx,
                          reinterpret_cast<uint8_t *>(plain.data()),
                          static_cast<size_t>(plain.size()));
    return plain;
}

} 





QByteArray SecureSecretBox::randomBytes(int n)
{
    if (n <= 0)
    {
        return QByteArray();
    }
    QByteArray buf(n, Qt::Uninitialized);
    QRandomGenerator *gen = QRandomGenerator::system();
    
    int filled = 0;
    while (filled < n)
    {
        const quint32 word = gen->generate();
        const int chunk = std::min(4, n - filled);
        std::memcpy(buf.data() + filled, &word, chunk);
        filled += chunk;
    }
    return buf;
}

QByteArray SecureSecretBox::hmacSha256(const QByteArray &data, const QByteArray &key)
{
    QMessageAuthenticationCode mac(QCryptographicHash::Sha256);
    mac.setKey(key);
    mac.addData(data);
    return mac.result();
}

QByteArray SecureSecretBox::pbkdf2Sha256(const QByteArray &password, const QByteArray &salt,
                                         int iterations, int outLen)
{
    if (iterations <= 0 || outLen <= 0 || password.isEmpty())
    {
        return QByteArray();
    }

    constexpr int hLen = 32; 
    const int blocks = (outLen + hLen - 1) / hLen;

    QByteArray result;
    result.reserve(blocks * hLen);

    for (int i = 1; i <= blocks; ++i)
    {
        
        QByteArray block = salt;
        block.append(static_cast<char>((i >> 24) & 0xff));
        block.append(static_cast<char>((i >> 16) & 0xff));
        block.append(static_cast<char>((i >> 8) & 0xff));
        block.append(static_cast<char>(i & 0xff));

        QByteArray u = hmacSha256(block, password);
        QByteArray t = u;

        
        for (int j = 1; j < iterations; ++j)
        {
            u = hmacSha256(u, password);
            for (int k = 0; k < hLen; ++k)
            {
                t[k] = static_cast<char>(static_cast<uint8_t>(t[k]) ^
                                          static_cast<uint8_t>(u[k]));
            }
        }
        result.append(t);
    }
    return result.left(outLen);
}

bool SecureSecretBox::constantTimeEqual(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    unsigned int diff = 0;
    for (int i = 0; i < a.size(); ++i)
    {
        diff |= static_cast<unsigned int>(static_cast<uint8_t>(a[i]) ^ static_cast<uint8_t>(b[i]));
    }
    return diff == 0;
}

void SecureSecretBox::secureZero(QByteArray &buf)
{
    if (buf.isEmpty())
    {
        return;
    }
    volatile char *p = buf.data();
    for (int i = 0; i < buf.size(); ++i)
    {
        p[i] = 0;
    }
}





QByteArray SecureSecretBox::encryptWithPassphrase(const QByteArray &plain, const QString &passphrase)
{
    if (passphrase.isEmpty())
    {
        qWarning() << "[SecureSecretBox] encryptWithPassphrase: empty passphrase";
        return QByteArray();
    }

    const QByteArray salt = randomBytes(kSaltLen);
    if (salt.size() != kSaltLen)
    {
        return QByteArray();
    }

    QByteArray pwBytes = passphrase.toUtf8();
    QByteArray material = pbkdf2Sha256(pwBytes, salt, kPbkdf2Iterations, kKdfOutLen);
    secureZero(pwBytes);

    if (material.size() != kKdfOutLen)
    {
        return QByteArray();
    }

    QByteArray aesKey, hmacKey;
    splitKeyMaterial(material, aesKey, hmacKey);
    QByteArray result = encryptCommon(plain, aesKey, hmacKey, salt, kFlagPassphrase);

    secureZero(material);
    secureZero(aesKey);
    secureZero(hmacKey);
    return result;
}

std::optional<QByteArray> SecureSecretBox::decryptWithPassphrase(const QByteArray &cipher,
                                                                 const QString &passphrase)
{
    if (passphrase.isEmpty())
    {
        return std::nullopt;
    }

    uint8_t flags = 0;
    QByteArray salt, nonce, payload, mac;
    if (!parseHeader(cipher, flags, salt, nonce, payload, mac))
    {
        qWarning() << "[SecureSecretBox] decryptWithPassphrase: header parse failed";
        return std::nullopt;
    }
    if (flags != kFlagPassphrase)
    {
        qWarning() << "[SecureSecretBox] decryptWithPassphrase: flags mismatch (got"
                   << flags << "expected" << kFlagPassphrase << ")";
        return std::nullopt;
    }

    QByteArray pwBytes = passphrase.toUtf8();
    QByteArray material = pbkdf2Sha256(pwBytes, salt, kPbkdf2Iterations, kKdfOutLen);
    secureZero(pwBytes);

    if (material.size() != kKdfOutLen)
    {
        return std::nullopt;
    }

    QByteArray aesKey, hmacKey;
    splitKeyMaterial(material, aesKey, hmacKey);
    auto plain = decryptCommon(cipher, aesKey, hmacKey);

    secureZero(material);
    secureZero(aesKey);
    secureZero(hmacKey);
    return plain;
}





QByteArray SecureSecretBox::encryptLocalSecret(const QByteArray &plain)
{
    QByteArray material = loadOrCreateLocalKeyMaterial();
    if (material.size() != kKdfOutLen)
    {
        qWarning() << "[SecureSecretBox] encryptLocalSecret: local key unavailable";
        return QByteArray();
    }

    QByteArray aesKey, hmacKey;
    splitKeyMaterial(material, aesKey, hmacKey);

    
    QByteArray zeroSalt(kSaltLen, '\0');
    QByteArray result = encryptCommon(plain, aesKey, hmacKey, zeroSalt, kFlagLocal);

    secureZero(material);
    secureZero(aesKey);
    secureZero(hmacKey);
    return result;
}

std::optional<QByteArray> SecureSecretBox::decryptLocalSecret(const QByteArray &cipher)
{
    uint8_t flags = 0;
    QByteArray salt, nonce, payload, mac;
    if (!parseHeader(cipher, flags, salt, nonce, payload, mac))
    {
        qWarning() << "[SecureSecretBox] decryptLocalSecret: header parse failed";
        return std::nullopt;
    }
    if (flags != kFlagLocal)
    {
        qWarning() << "[SecureSecretBox] decryptLocalSecret: flags mismatch (got"
                   << flags << "expected" << kFlagLocal << ")";
        return std::nullopt;
    }

    QByteArray material = loadOrCreateLocalKeyMaterial();
    if (material.size() != kKdfOutLen)
    {
        qWarning() << "[SecureSecretBox] decryptLocalSecret: local key unavailable";
        return std::nullopt;
    }

    QByteArray aesKey, hmacKey;
    splitKeyMaterial(material, aesKey, hmacKey);
    auto plain = decryptCommon(cipher, aesKey, hmacKey);

    secureZero(material);
    secureZero(aesKey);
    secureZero(hmacKey);
    return plain;
}
