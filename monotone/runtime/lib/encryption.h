#pragma once

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

typedef struct EncryptionIf EncryptionIf;
typedef struct Encryption   Encryption;

enum
{
	ENCRYPTION_NONE = 0,
	ENCRYPTION_AES  = 1
};

struct EncryptionIf
{
	int          id;
	Encryption* (*create)(EncryptionIf*);
	void        (*free)(Encryption*);
	void        (*encrypt)(Encryption*, Random*, Str*, Buf*, int, Buf**);
	void        (*decrypt)(Encryption*, Str*, Buf*, uint8_t*, int);
};

struct Encryption
{
	EncryptionIf* iface;
	List          link;
};

static inline Encryption*
encryption_create(EncryptionIf* iface)
{
	return iface->create(iface);
}

static inline void
encryption_free(Encryption* self)
{
	return self->iface->free(self);
}

static inline Encryption*
encryption_of(List* link)
{
	return container_of(link, Encryption, link);
}

static inline void
encryption_encrypt(Encryption* self, Random* random,
                   Str*        key,
                   Buf*        buf,
                   int         argc,
                   Buf**       argv)
{
	self->iface->encrypt(self, random, key, buf, argc, argv);
}

static inline void
encryption_decrypt(Encryption* self, Str* key,
                   Buf*        buf,
                   uint8_t*    data,
                   int         data_size)
{
	self->iface->decrypt(self, key, buf, data, data_size);
}
