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
