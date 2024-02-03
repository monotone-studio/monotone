
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_io.h>

void
part_download(Part* self)
{
	assert(part_has(self, PART_FILE_CLOUD));
	assert(self->cloud);

	// download partition file locally
	cloud_download(self->cloud, &self->id);

	// open
	part_file_open(self, false);
}

void
part_upload(Part* self)
{
	assert(part_has(self, PART_FILE));
	assert(self->cloud);

	Exception e;
	if (try(&e))
	{
		// create incomplete cloud file (index dump)
		part_file_cloud_create(self);

		// upload partition file to the cloud
		cloud_upload(self->cloud, &self->id);

		// rename partition cloud file as completed
		part_file_cloud_complete(self);
	}
	if (catch(&e))
	{
		part_file_cloud_delete(self, false);
		rethrow();
	}
}

void
part_offload(Part* self, bool from_storage)
{
	assert(self->cloud);

	// remove from storage
	if (from_storage)
	{
		// remove data file
		file_close(&self->file);
		part_file_delete(self, true);
		return;
	}

	// remove from cloud

	// remove cloud file first
	part_file_cloud_delete(self, true);

	// remove from cloud
	cloud_remove(self->cloud, &self->id);
}
