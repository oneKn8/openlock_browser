// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// SEB config decryption using RNCryptor v3 format.
// Reference: https://github.com/RNCryptor/RNCryptor-Spec/blob/master/RNCryptor-Spec-v3.md

#include <QByteArray>
#include <QDebug>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <cstring>

namespace openlock {

class SEBConfigParser {
public:
    // RNCryptor v3 binary layout:
    //   [0]    version byte = 0x03
    //   [1]    options byte = 0x01 (password-based)
    //   [2-9]  encryption salt (8 bytes)
    //   [10-17] HMAC salt (8 bytes)
    //   [18-33] IV (16 bytes)
    //   [34..n-33] AES-256-CBC ciphertext (PKCS7 padded)
    //   [n-32..n-1] HMAC-SHA256 tag (32 bytes)

    static QByteArray decryptRNCryptorV3(const QByteArray& data, const QString& password)
    {
        const int kHeaderSize = 2;
        const int kSaltSize = 8;
        const int kIVSize = 16;
        const int kHMACSize = 32;
        const int kMinSize = kHeaderSize + kSaltSize + kSaltSize + kIVSize + kHMACSize;

        if (data.size() < kMinSize) {
            qWarning() << "RNCryptor data too small:" << data.size();
            return {};
        }

        const unsigned char* raw = reinterpret_cast<const unsigned char*>(data.constData());
        int pos = 0;

        // Version and options
        unsigned char version = raw[pos++];
        unsigned char options = raw[pos++];

        if (version != 0x03 && version != 0x02) {
            qWarning() << "Unsupported RNCryptor version:" << (int)version;
            return {};
        }
        if (options != 0x01) {
            qWarning() << "RNCryptor: not password-based (options=" << (int)options << ")";
            return {};
        }

        // Encryption salt
        const unsigned char* encSalt = raw + pos;
        pos += kSaltSize;

        // HMAC salt
        const unsigned char* hmacSalt = raw + pos;
        pos += kSaltSize;

        // IV
        const unsigned char* iv = raw + pos;
        pos += kIVSize;

        // Ciphertext (everything except the last 32 bytes = HMAC)
        int ciphertextLen = data.size() - pos - kHMACSize;
        if (ciphertextLen <= 0) {
            qWarning() << "RNCryptor: no ciphertext";
            return {};
        }
        const unsigned char* ciphertext = raw + pos;
        pos += ciphertextLen;

        // HMAC tag
        const unsigned char* expectedHmac = raw + pos;

        // Derive keys using PBKDF2
        // SEB uses SHA-1 PRF with 10000 iterations
        QByteArray passwordBytes = password.toUtf8();
        unsigned char encKey[32];
        unsigned char hmacKey[32];

        int passLen = passwordBytes.size();
        // Note: RNCryptor v2 had a bug using character count instead of byte count.
        // v3 uses byte count correctly.
        if (version == 0x02) {
            passLen = password.length(); // character count (the v2 bug)
        }

        if (!PKCS5_PBKDF2_HMAC(
                passwordBytes.constData(), passLen,
                encSalt, kSaltSize,
                10000, EVP_sha1(),
                32, encKey)) {
            qWarning() << "PBKDF2 encryption key derivation failed";
            return {};
        }

        if (!PKCS5_PBKDF2_HMAC(
                passwordBytes.constData(), passLen,
                hmacSalt, kSaltSize,
                10000, EVP_sha1(),
                32, hmacKey)) {
            qWarning() << "PBKDF2 HMAC key derivation failed";
            return {};
        }

        // Verify HMAC
        // HMAC covers header + ciphertext (everything except the HMAC itself)
        unsigned int hmacLen = 0;
        unsigned char computedHmac[32];
        HMAC(EVP_sha256(),
             hmacKey, 32,
             raw, data.size() - kHMACSize,
             computedHmac, &hmacLen);

        if (memcmp(computedHmac, expectedHmac, kHMACSize) != 0) {
            qWarning() << "RNCryptor HMAC verification failed (wrong password?)";
            return {};
        }

        // Decrypt AES-256-CBC
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) return {};

        QByteArray decrypted(ciphertextLen + 16, '\0');
        int outLen = 0;
        int totalLen = 0;

        if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, encKey, iv)) {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }

        if (!EVP_DecryptUpdate(ctx,
                               reinterpret_cast<unsigned char*>(decrypted.data()), &outLen,
                               ciphertext, ciphertextLen)) {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }
        totalLen = outLen;

        if (!EVP_DecryptFinal_ex(ctx,
                                  reinterpret_cast<unsigned char*>(decrypted.data()) + totalLen,
                                  &outLen)) {
            EVP_CIPHER_CTX_free(ctx);
            qWarning() << "AES decryption final block failed";
            return {};
        }
        totalLen += outLen;

        EVP_CIPHER_CTX_free(ctx);
        decrypted.resize(totalLen);

        return decrypted;
    }

    // Full .seb file decryption flow:
    // 1. Outer gzip decompress
    // 2. Read 4-byte prefix
    // 3. Decrypt based on prefix type
    // 4. Inner gzip decompress
    // 5. Result is XML plist
    static QByteArray decryptSebFile(const QByteArray& data, const QString& password)
    {
        QByteArray working = data;

        // Try gzip decompression first (outer layer)
        QByteArray decompressed = tryGzipDecompress(working);
        if (!decompressed.isEmpty()) {
            working = decompressed;
        }

        if (working.size() < 4) return {};

        // Check prefix
        QByteArray prefix = working.left(4);

        if (prefix == "pswd" || prefix == "pwcc") {
            // Password-encrypted (RNCryptor v3)
            QByteArray encrypted = working.mid(4);
            QByteArray decrypted = decryptRNCryptorV3(encrypted, password);
            if (decrypted.isEmpty()) return {};

            // Inner gzip decompress
            QByteArray innerDecomp = tryGzipDecompress(decrypted);
            return innerDecomp.isEmpty() ? decrypted : innerDecomp;
        }

        if (prefix == "plnd") {
            // Plain compressed
            return tryGzipDecompress(working.mid(4));
        }

        if (working.startsWith("<?xm")) {
            // Raw XML plist, no encryption
            return working;
        }

        qWarning() << "Unknown .seb file prefix:" << prefix.toHex();
        return {};
    }

private:
    static QByteArray tryGzipDecompress(const QByteArray& data)
    {
        // Check gzip magic bytes (0x1f, 0x8b)
        if (data.size() < 2) return {};
        if (static_cast<unsigned char>(data[0]) != 0x1f ||
            static_cast<unsigned char>(data[1]) != 0x8b) {
            return {};
        }

        return qUncompress(data);
    }
};

} // namespace openlock
