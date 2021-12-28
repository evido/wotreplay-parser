#include <openssl/err.h>

#include "cipher_context.h"

#define SSL_SHOW_ERRORS(x) if ((x)) { ERR_print_errors_fp(stderr); }

CipherContext::CipherContext(const unsigned char* key_data, int key_size, const unsigned char* iv)
    : legacy_provider(nullptr), default_provider(nullptr), ossl_ctx(nullptr), cipher(nullptr), ctx(nullptr)
{
    OPENSSL_assert(ossl_ctx = OSSL_LIB_CTX_new());
    OPENSSL_assert(legacy_provider = OSSL_PROVIDER_load(ossl_ctx, "legacy"));
    OPENSSL_assert(default_provider = OSSL_PROVIDER_load(ossl_ctx, "default"));

    OPENSSL_assert(cipher = EVP_CIPHER_fetch(ossl_ctx, "BF-ECB", nullptr));

    OPENSSL_assert(ctx = EVP_CIPHER_CTX_new());
    OPENSSL_assert(EVP_CipherInit_ex(ctx, cipher, nullptr, nullptr, nullptr, 0) == 1);
    OPENSSL_assert(EVP_CIPHER_CTX_set_key_length(ctx, 16) == 1);
    OPENSSL_assert(EVP_CipherInit_ex(ctx, nullptr, nullptr, key_data, iv, 0) == 1);
    OPENSSL_assert(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1);
}

int CipherContext::update(unsigned char* out, int* out_len, const unsigned char* in, int in_len) {
    int result = EVP_CipherUpdate(ctx, out, out_len, in, in_len) == 1;
    OPENSSL_assert(result);
    return result;
}

int CipherContext::finalize(unsigned char *out, int* out_len) {
    int result = EVP_CipherFinal_ex(ctx, out, out_len) == 1;
    OPENSSL_assert(result);
    return result;
}

CipherContext::~CipherContext() {
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
    OSSL_PROVIDER_unload(legacy_provider);
    OSSL_PROVIDER_unload(default_provider);
    OSSL_LIB_CTX_free(ossl_ctx);
}

