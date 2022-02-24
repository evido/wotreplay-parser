#pragma once

#ifndef cipher_context_h
#define cipher_context_h

#include <openssl/opensslv.h>

#if OPENSSL_VERSION_MAJOR >= 3
#include <openssl/provider.h>
#endif

#include <openssl/evp.h>

class CipherContext {
public:
    CipherContext(const char* cipher_name, const unsigned char* key_data, int key_size, const unsigned char* iv);
    int update(unsigned char* out, int* out_len, const unsigned char* in, int in_len);
    int finalize(unsigned char* out, int* out_len);
    ~CipherContext();
private:
#if OPENSSL_VERSION_MAJOR >= 3
    OSSL_PROVIDER *legacy_provider = nullptr;
    OSSL_PROVIDER *default_provider = nullptr;
    OSSL_LIB_CTX *ossl_ctx;
    EVP_CIPHER *cipher;
#else
    const EVP_CIPHER *cipher;
#endif
    EVP_CIPHER_CTX *ctx;
};

#endif

