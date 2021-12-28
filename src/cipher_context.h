#pragma once

#ifndef cipher_context_h
#define cipher_context_h

#include <openssl/provider.h>
#include <openssl/evp.h>

class CipherContext {
public:
    CipherContext(const unsigned char* key_data, int key_size, const unsigned char* iv);
    int update(unsigned char* out, int* out_len, const unsigned char* in, int in_len);
    int finalize(unsigned char* out, int* out_len);
    ~CipherContext();
private:
    OSSL_PROVIDER *legacy_provider = nullptr;
    OSSL_PROVIDER *default_provider = nullptr;
    OSSL_LIB_CTX *ossl_ctx;
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
};

#endif

