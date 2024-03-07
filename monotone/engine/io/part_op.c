
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_config.h>
#include <monotone_cloud.h>
#include <monotone_io.h>

void
part_download(Part* self)
{
	assert(part_has(self, ID_CLOUD));
	assert(self->cloud);

	// download partition file locally
	cloud_download(self->cloud, self->source, &self->id);

	// open
	part_open(self, ID, false);
}

void
part_upload(Part* self)
{
	assert(part_has(self, ID));
	assert(self->cloud);

	// in case previous attempt failed without crash
	part_delete(self, ID_CLOUD_INCOMPLETE);

	// create incomplete cloud file (index dump)
	part_create(self, ID_CLOUD_INCOMPLETE);

	// upload partition file to the cloud
	cloud_upload(self->cloud, self->source, &self->id);

	// rename partition cloud file as completed
	part_rename(self, ID_CLOUD_INCOMPLETE, ID_CLOUD);
}

void
part_offload(Part* self, bool from_storage)
{
	// remove from storage
	if (from_storage)
	{
		// remove data file
		file_close(&self->file);
		part_delete(self, ID);
		return;
	}

	// remove from cloud
	assert(self->cloud);

	// remove cloud file first
	part_delete(self, ID_CLOUD);

	// remove from cloud
	cloud_remove(self->cloud, self->source, &self->id);
}
