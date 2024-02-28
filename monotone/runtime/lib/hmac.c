
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>

#include <openssl/hmac.h>
#include <openssl/evp.h>

static void
hmac_base64_encode(unsigned char digest_raw[20], unsigned char digest[28])
{
	static const char* const chars =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	unsigned long v = 0;
	int i = 0;
	int j = 0;
	for (; i < 20; ++i)
	{
		v = (v << 8) | digest_raw[i];
		if ((i % 3) == 2) {
			digest[j++] = chars[(v >> 18) & 0x3F];
			digest[j++] = chars[(v >> 12) & 0x3F];
			digest[j++] = chars[(v >> 6)  & 0x3F];
			digest[j++] = chars[v & 0x3F];
			v = 0;
		}
	}
	digest[j++] = chars[(v >> 10) & 0x3F];
	digest[j++] = chars[(v >> 4)  & 0x3F];
	digest[j++] = chars[(v << 2)  & 0x3F];
	digest[j++] = '=';
	digest[j++] = '\0';
}

void
hmac_base64(char* digest, int digest_size,
            void* key,
            int   key_size,
            void* data,
            int   data_size)
{
	assert(digest_size == HMAC_SZ);
	unused(digest_size);

    unsigned char digest_raw[20];
    HMAC(EVP_sha1(), key, key_size, data, data_size, digest_raw, NULL);

    hmac_base64_encode(digest_raw, (unsigned char*)digest);
}
