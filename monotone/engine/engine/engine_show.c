
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
#include <monotone_config.h>
#include <monotone_cloud.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_wal.h>
#include <monotone_engine.h>

static void
engine_show_storages(Engine* self, Storage* storage,
                     Buf*    buf,
                     bool    debug)
{
	if (storage)
	{
		storage_show(storage, buf, debug);
		return;
	}
	storage_mgr_show(&self->storage_mgr, buf, debug);
}

static void
engine_show_partition(Engine* self, uint64_t min, Buf* buf, bool debug)
{
	auto slice = mapping_match(&self->mapping, min);
	if (! slice)
		error("partition '%" PRIu64 "': not exists", min);
	auto ref = ref_of(slice);
	stats_show_part(ref->part, buf, debug);
}

static void
engine_show_partitions(Engine* self, Storage* storage,
                       Buf*    buf,
                       bool    debug)
{
	// []
	int array_count = 0;
	int array_offset = buf_size(buf);
	encode_array32(buf, 0);

	auto slice = mapping_min(&self->mapping);
	for (; slice; slice = mapping_next(&self->mapping, slice))
	{
		auto ref  = ref_of(slice);
		auto part = ref->part;
		if (storage)
		{
			if (part->source != storage->source)
				continue;
		}
		stats_show_part(part, buf, debug);
		array_count++;
	}

	// set count
	uint8_t* pos = buf->start + array_offset;
	data_write_array32(&pos, array_count);
}

void
engine_show(Engine*  self,
            int      op,
            Str*     storage_name,
            uint64_t min,
            Buf*     buf,
            bool     debug)
{
	control_lock_shared();
	guard(control_unlock_guard, NULL);

	mutex_lock(&self->lock);
	guard(mutex_unlock, &self->lock);

	// find storage, if specified
	Storage* storage = NULL;
	if (storage_name)
	{
		storage = storage_mgr_find(&self->storage_mgr, storage_name);
		if (! storage)
		{
			error("storage '%.*s': not exists", str_size(storage_name),
			      str_of(storage_name));
			return;
		}
	}

	switch (op) {
	case ENGINE_SHOW_STORAGES:
		engine_show_storages(self, storage, buf, debug);
		break;
	case ENGINE_SHOW_PARTITION:
		engine_show_partition(self, min, buf, debug);
		break;
	case ENGINE_SHOW_PARTITIONS:
		engine_show_partitions(self, storage, buf, debug);
		break;
	}
}
