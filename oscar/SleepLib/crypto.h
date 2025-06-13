/* SleepLib cryptography abstraction
*
* Copyright (c) 2021-2025 The OSCAR Team
*
* This file is subject to the terms and conditions of the GNU General Public
* License. See the file COPYING in the main directory of the source code
* for more details. */

#ifndef CRYPTO_H
#define CRYPTO_H

#include <QByteArray>

enum CryptoResult
{
    OK = 0,
    UnknownError = -1,
    InvalidTag = 1,
};

CryptoResult decrypt_aes256(const QByteArray & key, const QByteArray & ciphertext, QByteArray & plaintext);
CryptoResult decrypt_aes256_gcm(const QByteArray & key,
                                const QByteArray & iv, const QByteArray & ciphertext, const QByteArray & tag,
                                QByteArray & plaintext);
CryptoResult encrypt_aes256_gcm(const QByteArray & key,
                                const QByteArray & iv, const QByteArray & plaintext,
                                QByteArray & ciphertext, QByteArray & tag);
CryptoResult pbkdf2_sha256(const QByteArray & passphrase, const QByteArray & salt, int iterations, QByteArray & key);

#endif // CRYPTO_H
