/* SleepLib cryptography abstraction
*
* Copyright (c) 2021-2025 The OSCAR Team
*
* This file is subject to the terms and conditions of the GNU General Public
* License. See the file COPYING in the main directory of the source code
* for more details. */

#include <QDebug>

#include "SleepLib/crypto.h"
#include "SleepLib/thirdparty/botan_all.h"

CryptoResult decrypt_aes256(const QByteArray & key, const QByteArray & ciphertext, QByteArray & plaintext)
{
    CryptoResult result = OK;
    plaintext.clear();
    try {
        const std::vector<uint8_t> botan_key(key.begin(), key.end());
        Botan::secure_vector<uint8_t> botan_message(ciphertext.begin(), ciphertext.end());

        std::unique_ptr<Botan::BlockCipher> dec = Botan::BlockCipher::create("AES-256");
        dec->set_key(botan_key);
        dec->decrypt(botan_message);
        QByteArray message((char*) botan_message.data(), botan_message.size());
        plaintext = message;
    }
    catch (std::exception& e) {
        // Make sure no Botan exceptions leak out and terminate the application.
        qWarning() << "Unexpected exception in decrypt_aes256:" << e.what();
        result = UnknownError;
    }
    return result;
}

CryptoResult decrypt_aes256_gcm(const QByteArray & key,
                                const QByteArray & iv, const QByteArray & ciphertext, const QByteArray & tag,
                                QByteArray & plaintext)
{
    CryptoResult result = OK;
    plaintext.clear();
    try {
        const std::vector<uint8_t> botan_key(key.begin(), key.end());
        const std::vector<uint8_t> botan_iv(iv.begin(), iv.end());
        const std::vector<uint8_t> botan_tag(tag.begin(), tag.end());

        Botan::secure_vector<uint8_t> botan_message(ciphertext.begin(), ciphertext.end());
        botan_message += botan_tag;

        std::unique_ptr<Botan::Cipher_Mode> dec = Botan::Cipher_Mode::create("AES-256/GCM", Botan::DECRYPTION);
        dec->set_key(botan_key);
        dec->start(botan_iv);
        try {
            dec->finish(botan_message);
            //qDebug() << QString::fromStdString(Botan::hex_encode(message.data(), message.size()));
            QByteArray message((char*) botan_message.data(), botan_message.size());
            plaintext = message;
        }
        catch (const Botan::Invalid_Authentication_Tag& e) {
            result = InvalidTag;
        }
    }
    catch (std::exception& e) {
        // Make sure no Botan exceptions leak out and terminate the application.
        qWarning() << "Unexpected exception in decrypt_aes256_gcm:" << e.what();
        result = UnknownError;
    }
    return result;
}

CryptoResult encrypt_aes256_gcm(const QByteArray & key,
                                const QByteArray & iv, const QByteArray & plaintext,
                                QByteArray & ciphertext, QByteArray & tag)
{
    CryptoResult result = OK;
    ciphertext.clear();
    try {
        const std::vector<uint8_t> botan_key(key.begin(), key.end());
        const std::vector<uint8_t> botan_iv(iv.begin(), iv.end());

        Botan::secure_vector<uint8_t> botan_message(plaintext.begin(), plaintext.end());

        std::unique_ptr<Botan::Cipher_Mode> enc = Botan::Cipher_Mode::create("AES-256/GCM", Botan::ENCRYPTION);
        enc->set_key(botan_key);
        enc->start(botan_iv);
        enc->finish(botan_message);
        //qDebug() << QString::fromStdString(Botan::hex_encode(botan_message.data(), botan_message.size()));

        size_t tag_size = enc->tag_size();
        QByteArray message((char*) botan_message.data(), botan_message.size());
        tag = message.right(tag_size);
        ciphertext = message.left(message.size() - tag_size);
    }
    catch (std::exception& e) {
        // Make sure no Botan exceptions leak out and terminate the application.
        qWarning() << "Unexpected exception in encrypt_aes256_gcm:" << e.what();
        result = UnknownError;
    }
    return result;

}

CryptoResult pbkdf2_sha256(const QByteArray & passphrase, const QByteArray & salt, int iterations, QByteArray & key)
{
    CryptoResult result = OK;
    try {
        std::unique_ptr<Botan::PasswordHashFamily> family = Botan::PasswordHashFamily::create("PBKDF2(SHA-256)");
        std::unique_ptr<Botan::PasswordHash> kdf = family->from_params(iterations);
        Botan::secure_vector<uint8_t> botan_key(key.size());
        kdf->derive_key(botan_key.data(), botan_key.size(),
                        (const char*) passphrase.data(), passphrase.size(),
                        (const uint8_t*) salt.data(), salt.size());
        QByteArray output((char*) botan_key.data(), botan_key.size());
        key = output;
    }
    catch (std::exception& e) {
        // Make sure no Botan exceptions leak out and terminate the application.
        qWarning() << "Unexpected exception in pbkdf2_sha256:" << e.what();
        result = UnknownError;
        key.clear();
    }
    return result;
}
