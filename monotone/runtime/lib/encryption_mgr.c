
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
encryption_mgr_prepare(EncryptionMgr* self, Random* random, Str* type, Str* key)
{
	unused(self);

	// validate encryption type
	int id = encryption_mgr_of(type);
	if (id == -1)
		error("unknown encryption: %.*s", str_size(type),
			  str_of(type));

	if (id == ENCRYPTION_NONE)
		return;

	// validate or generate key
	if (! str_empty(key))
	{
		if (str_size(key) != 32)
			error("aes: 256bit key expected");
	} else
	{
		uint8_t data[32];
		random_generate_alnum(random, data, sizeof(data));
		str_strndup(key, data, sizeof(data));
	}
}
