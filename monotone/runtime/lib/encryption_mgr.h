#pragma once

//
// monotone
//
// time-series storage
//

typedef struct EncryptionMgr EncryptionMgr;

struct EncryptionMgr
{
	Cache cache_aes;
};

void encryption_mgr_init(EncryptionMgr*);
void encryption_mgr_free(EncryptionMgr*);
Encryption*
encryption_mgr_pop(EncryptionMgr*, int);
void encryption_mgr_push(EncryptionMgr*, Encryption*);
int  encryption_mgr_of(Str*);
void encryption_mgr_prepare(EncryptionMgr*, EncryptionConfig*, UuidMgr*);
