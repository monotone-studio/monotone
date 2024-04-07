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

typedef struct Engine Engine;

struct Engine
{
	// locking
	Mutex       lock;
	CondVar     cond_var;
	// partition mapping
	Mapping     mapping;
	// objects
	Pipeline    pipeline;
	StorageMgr  storage_mgr;
	Wal*        wal;
	Service*    service;
	Comparator* comparator;
};

void engine_init(Engine*, Comparator*, Wal*, Service*, CloudMgr*);
void engine_free(Engine*);
void engine_open(Engine*);
void engine_close(Engine*);
void engine_set_serial(Engine*);
Ref* engine_lock(Engine*, uint64_t, LockType, bool, bool);
void engine_unlock(Engine*, Ref*, LockType);
