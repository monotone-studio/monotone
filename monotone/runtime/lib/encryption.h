#pragma once

//
// monotone
//
// time-series storage
//

typedef struct EncryptionIf     EncryptionIf;
typedef struct EncryptionConfig EncryptionConfig;
typedef struct Encryption       Encryption;

enum
{
	ENCRYPTION_NONE = 0,
	ENCRYPTION_AES  = 1
};

struct EncryptionConfig
{
	Str* type;
	Str* key;
	Str* iv;
};

struct EncryptionIf
{
	int          id;
	Encryption* (*create)(EncryptionIf*);
	void        (*free)(Encryption*);
	void        (*encrypt)(Encryption*, EncryptionConfig*, Buf*, int, Buf**);
	void        (*decrypt)(Encryption*, EncryptionConfig*, Buf*, uint8_t*, int, int);
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
encryption_encrypt(Encryption* self, EncryptionConfig* config,
                   Buf*        buf,
                   int         argc,
                   Buf**       argv)
{
	self->iface->encrypt(self, config, buf, argc, argv);
}

static inline void
encryption_decrypt(Encryption* self, EncryptionConfig* config,
                   Buf*        buf,
                   uint8_t*    data,
                   int         data_size,
                   int         data_size_unencrypted)
{
	self->iface->decrypt(self, config, buf,
	                     data,
	                     data_size,
	                     data_size_unencrypted);
}
