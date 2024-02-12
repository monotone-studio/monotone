
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_io.h>
#include <monotone_storage.h>
#include <monotone_wal.h>

void
wal_init(Wal* self)
{
	self->current = NULL;
	mutex_init(&self->lock);
	wal_id_init(&self->list);
}

void
wal_free(Wal* self)
{
	if (self->current)
	{
		auto file = self->current;
		wal_file_close(file);
		wal_file_free(file);
		self->current = NULL;
	}
	wal_id_free(&self->list);
	mutex_free(&self->lock);
}

void
wal_rotate(Wal* self)
{
	mutex_lock(&self->lock);
	uint64_t next_lsn = config_lsn() + 1;

	// create new wal file
	WalFile* file = NULL;
	Exception e;
	if (try(&e))
	{
		file = wal_file_allocate(next_lsn);
		wal_file_create(file);
	}
	if (catch(&e))
	{
		if (file)
		{
			wal_file_close(file);
			wal_file_free(file);
		}
		mutex_unlock(&self->lock);
		rethrow();
	}

	// add to the list and set as current
	auto file_prev = self->current;
	self->current = file;

	wal_id_add(&self->list, next_lsn);
	mutex_unlock(&self->lock);

	// sync and close prev file
	if (file_prev)
	{
		if (var_int_of(&config()->wal_sync_on_rotate))
			file_sync(&file_prev->file);
		wal_file_close(file_prev);
		wal_file_free(file_prev);
	}
}

void
wal_gc(Wal* self, uint64_t snapshot)
{
	uint64_t min = snapshot;

	Buf list;
	buf_init(&list);
	guard(list_guard, buf_free, &list);

	int list_count;
	list_count = wal_id_gc_between(&self->list, &list, min);

	// remove files by id
	if (list_count > 0)
	{
		char path[PATH_MAX];
		uint64_t *ids = (uint64_t*)list.start;
		int i = 0;
		for (; i < list_count; i++)
		{
			snprintf(path, sizeof(path), "%s/wal/%020" PRIu64 ".wal",
			         config_directory(),
			         ids[i]);
			log("wal: remove file %s", path);
			fs_unlink("%s", path);
		}
	}
}

static inline int64_t
wal_file_id_of(const char* path)
{
	uint64_t id = 0;
	while (*path && *path != '.')
	{
		if (unlikely(! isdigit(*path)))
			return -1;
		id = (id * 10) + *path - '0';
		path++;
	}
	return id;
}

static void
wal_recover(Wal* self, char *path)
{
	// open and read log directory
	DIR* dir = opendir(path);
	if (unlikely(dir == NULL))
		error("wal: directory '%s' open error", path);
	guard(dir_guard, closedir, dir);
	for (;;)
	{
		auto entry = readdir(dir);
		if (entry == NULL)
			break;
		if (entry->d_name[0] == '.')
			continue;
		int64_t id = wal_file_id_of(entry->d_name);
		if (unlikely(id == -1))
			continue;
		wal_id_add(&self->list, id);
	}
}

void
wal_open(Wal* self)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/wal", config_directory());

	// create log directory
	if (! fs_exists("%s", path))
	{
		log("wal: new directory '%s'", path);
		fs_mkdir(0755, "%s", path);
	}

	// read file list
	wal_recover(self, path);

	// open last log file and set it as current
	if (self->list.list_count > 0)
	{
		uint64_t last = wal_id_max(&self->list);
		self->current = wal_file_allocate(last);
		wal_file_open(self->current);
		file_seek_to_end(&self->current->file);
	} else
	{
		wal_rotate(self);
	}
}

hot bool
wal_write(Wal* self, Log* log)
{
	mutex_lock(&self->lock);
	guard(unlock, mutex_unlock, &self->lock);

	// assign next lsn
	log->write.lsn = config_lsn() + 1;

	// write wal file
	wal_file_write(self->current, iov_pointer(&log->iov),
	               log->iov.iov_count);

	// set lsn
	config_lsn_set(log->write.lsn);

	// return true if wal is ready to be rotated
	auto rotate_ready =
		self->current->file.size > var_int_of(&config()->wal_rotate_wm);
	return rotate_ready;
}

void
wal_show(Wal* self, Buf* buf)
{
	int      list_count;
	uint64_t list_min;
	wal_id_stats(&self->list, &list_count, &list_min);
	buf_printf(buf, "lsn     %" PRIu64 "\n", config_lsn());
	buf_printf(buf, "lsn_min %" PRIu64 "\n", list_min);
	buf_printf(buf, "files   %d\n", list_count);
}
