
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>

void
encryption_mgr_init(EncryptionMgr* self)
{
	cache_init(&self->cache_aes);
}

void
encryption_mgr_free(EncryptionMgr* self)
{
	list_foreach_safe(&self->cache_aes.list)
	{
		auto ref = list_at(Encryption, link);
		encryption_free(ref);
	}
	cache_reset(&self->cache_aes);
	cache_free(&self->cache_aes);
}

Encryption*
encryption_mgr_pop(EncryptionMgr* self, int id)
{
	if (id == ENCRYPTION_AES)
	{
		auto ref = cache_pop(&self->cache_aes);
		if (likely(ref))
			return encryption_of(ref);
		return encryption_create(&encryption_aes);
	}
	return NULL;
}

void
encryption_mgr_push(EncryptionMgr* self, Encryption* encryption)
{
	if (encryption->iface->id == ENCRYPTION_AES)
	{
		cache_push(&self->cache_aes, &encryption->link);
		return;
	}
	abort();
}

int
encryption_mgr_of(Str* name)
{
	if (!name || str_empty(name))
		return ENCRYPTION_NONE;
	if (str_compare_raw(name, "aes", 3))
		return ENCRYPTION_AES;
	return -1;
}

void
encryption_mgr_prepare(EncryptionMgr* self, EncryptionConfig* config,
                       Random* random)
{
	unused(self);

	// validate encryption type
	int id = encryption_mgr_of(config->type);
	if (id == -1)
		error("unknown encryption: %.*s", str_size(config->type),
			  str_of(config->type));

	if (id == ENCRYPTION_NONE)
		return;

	// validate or generate key
	if (! str_empty(config->key))
	{
		if (str_size(config->key) != 32)
			error("aes: 256bit key expected");
	} else
	{
		uint8_t key[32];
		random_generate_alnum(random, key, sizeof(key));
		str_strndup(config->key, key, sizeof(key));
	}

	// validate or generate iv
	if (! str_empty(config->iv))
	{
		if (str_size(config->iv) != 16)
			error("aes: 128bit iv expected");
	} else
	{
		uint8_t iv[16];
		random_generate_alnum(random, iv, sizeof(iv));
		str_strndup(config->iv, iv, sizeof(iv));
	}
}
