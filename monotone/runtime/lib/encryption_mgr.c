
//
// monotone.
//
// embeddable cloud-native storage for events
// and time-series data.
//
// Copyright (c) 2023-2024 Dmitry Simonenko
// Copyright (c) 2023-2024 Monotone
//
// Distributed under the terms of GNU LGPL v3 or
// Monotone Commercial License.
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
	if (str_compare_raw(name, "none", 4))
		return COMPRESSION_NONE;
	if (str_compare_raw(name, "aes", 3))
		return ENCRYPTION_AES;
	return -1;
}
