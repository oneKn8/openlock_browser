// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// SEB config decryption using RNCryptor v3 format.
// Reference: https://github.com/RNCryptor/RNCryptor-Spec/blob/master/RNCryptor-Spec-v3.md

#include "protocol/SEBConfigParser.h"

#include <QDebug>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <cstring>
#include <zlib.h>

namespace openlock {

// RNCryptor v3 binary layout:
//   [0]    version byte = 0x03
//   [1]    options byte = 0x01 (password-based)
//   [2-9]  encryption salt (8 bytes)
//   [10-17] HMAC salt (8 bytes)
//   [18-33] IV (16 bytes)
//   [34..n-33] AES-256-CBC ciphertext (PKCS7 padded)
//   [n-32..n-1] HMAC-SHA256 tag (32 bytes)

QByteArray SEBConfigParser::decryptRNCryptorV3(const QByteArray& data, const QString& password)
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

    // Version and options (RNCryptor v2: SEB for Windows, v3: SEB for macOS)
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

    if (CRYPTO_memcmp(computedHmac, expectedHmac, kHMACSize) != 0) {
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
QByteArray SEBConfigParser::decryptSebFile(const QByteArray& data, const QString& password)
{
    QByteArray working = data;

    // Try gzip decompression first (outer layer)
    QByteArray decompressed = tryOuterGzip(working);
    if (!decompressed.isEmpty()) {
        working = decompressed;
    }

    if (working.size() < 4) return {};

    // Check prefix
    QByteArray prefix = working.left(4);

    if (prefix == "pwcc") {
        qWarning() << "SEBConfigParser: pwcc (configure-client) files are not supported";
        return {};
    }

    if (prefix == "pswd") {
        // Password-encrypted (RNCryptor v2/v3)
        QByteArray encrypted = working.mid(4);
        QByteArray decrypted = decryptRNCryptorV3(encrypted, password);
        if (decrypted.isEmpty()) return {};

        // Inner gzip decompress
        QByteArray innerDecomp = tryOuterGzip(decrypted);
        return innerDecomp.isEmpty() ? decrypted : innerDecomp;
    }

    if (prefix == "plnd") {
        // Plain compressed
        return tryOuterGzip(working.mid(4));
    }

    if (working.startsWith("<?xm")) {
        // Raw XML plist, no encryption
        return working;
    }

    qWarning() << "Unknown .seb file prefix:" << prefix.toHex();
    return {};
}

QByteArray SEBConfigParser::tryOuterGzip(const QByteArray& data)
{
    // Check gzip magic bytes (0x1f, 0x8b)
    if (data.size() < 2) return {};
    if (static_cast<unsigned char>(data[0]) != 0x1f ||
        static_cast<unsigned char>(data[1]) != 0x8b) {
        return {};
    }

    z_stream zs{};
    // 15 + 16 tells zlib to expect gzip format (not raw zlib)
    if (inflateInit2(&zs, 15 + 16) != Z_OK)
        return {};

    zs.next_in = reinterpret_cast<Bytef*>(
        const_cast<char*>(data.constData()));
    zs.avail_in = static_cast<uInt>(data.size());

    QByteArray out;
    char buf[65536];
    int ret;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(buf);
        zs.avail_out = sizeof(buf);
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR ||
            ret == Z_MEM_ERROR || ret == Z_BUF_ERROR) {
            inflateEnd(&zs);
            return {};
        }
        out.append(buf, sizeof(buf) - zs.avail_out);
        if (out.size() > 16 * 1024 * 1024) {
            inflateEnd(&zs);
            qWarning() << "SEBConfigParser: decompressed size exceeds 16 MiB limit";
            return {};
        }
    } while (ret != Z_STREAM_END);

    inflateEnd(&zs);
    return out;
}

} // namespace openlock