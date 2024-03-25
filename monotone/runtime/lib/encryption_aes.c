
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>

#include <openssl/evp.h>

typedef struct EncryptionAes EncryptionAes;

struct EncryptionAes
{
	Encryption      id;
	EVP_CIPHER_CTX* ctx;
};

static Encryption*
encryption_aes_create(EncryptionIf* iface)
{
	EncryptionAes* self = mn_malloc(sizeof(EncryptionAes));
	self->id.iface = iface;
	self->ctx      = EVP_CIPHER_CTX_new();
	if (self->ctx == NULL)
	{
		mn_free(self);
		error("aes: EVP_CIPHER_CTX_new() failed");
	}
	if (! EVP_CipherInit_ex2(self->ctx, EVP_aes_256_cbc(), NULL, NULL, true, NULL))
	{
		EVP_CIPHER_CTX_free(self->ctx);
		mn_free(self);
		error("aes: EVP_CipherInit_ex2() failed");
	}
	list_init(&self->id.link);
	return &self->id;
}

static void
encryption_aes_free(Encryption* ptr)
{
	auto self = (EncryptionAes*)ptr;
	if (self->ctx)
		EVP_CIPHER_CTX_free(self->ctx);
	mn_free(self);
}

hot static void
encryption_aes_encrypt(Encryption*       ptr,
                       EncryptionConfig* config,
                       Buf*              buf,
                       int               argc,
                       Buf**             argv)
{
	auto self = (EncryptionAes*)ptr;

	// calculate total size
	size_t size = EVP_MAX_BLOCK_LENGTH;
	for (int i = 0; i < argc; i++)
		size += buf_size(argv[i]);
	buf_reserve(buf, size);

	// prepare cipher
	if (! EVP_CipherInit_ex2(self->ctx, NULL,
	                         str_u8(config->key),
	                         str_u8(config->iv), true, NULL))
		error("aes: EVP_CipherInit_ex2() failed");

	// encrypt
	for (int i = 0; i < argc; i++)
	{
		auto ref = argv[i];
		int outlen = 0;
		if (! EVP_CipherUpdate(self->ctx, buf->position, &outlen, ref->start, buf_size(ref)))
			error("aes: EVP_CipherUpdate() failed");
		buf_advance(buf, outlen);
	}

	// finilize
	int outlen = 0;
	if (! EVP_CipherFinal_ex(self->ctx, buf->position, &outlen))
		error("aes: EVP_CipherFinal_ex() failed");
	buf_advance(buf, outlen);
}

hot static void
encryption_aes_decrypt(Encryption*       ptr,
                       EncryptionConfig* config,
                       Buf*              buf,
                       uint8_t*          data,
                       int               data_size,
                       int               data_size_unencrypted)
{
	auto self = (EncryptionAes*)ptr;
	buf_reserve(buf, data_size_unencrypted);

	// prepare cipher
	if (! EVP_CipherInit_ex2(self->ctx, NULL,
	                         str_u8(config->key),
	                         str_u8(config->iv), false, NULL))
		error("aes: EVP_CipherInit_ex2() failed");

	// decrypt
	int outlen = 0;
	if (! EVP_CipherUpdate(self->ctx, buf->position, &outlen, data, data_size))
		error("aes: EVP_CipherUpdate() failed");
	buf_advance(buf, outlen);

	// finilize
	if (! EVP_CipherFinal_ex(self->ctx, buf->position, &outlen))
		error("aes: EVP_CipherFinal_ex() failed");
	buf_advance(buf, outlen);
}

EncryptionIf encryption_aes =
{
	.id      = ENCRYPTION_AES,
	.create  = encryption_aes_create,
	.free    = encryption_aes_free,
	.encrypt = encryption_aes_encrypt,
	.decrypt = encryption_aes_decrypt
};
