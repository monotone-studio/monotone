
//
// monotone
//
// time-series storage
//

#include <monotone_runtime.h>
#include <monotone_lib.h>
#include <monotone_io.h>
#include <monotone_catalog.h>
#include <monotone_engine.h>

void
engine_init(Engine* self, Comparator* comparator)
{
	self->comparator = comparator;
	service_init(&self->service);
	catalog_init(&self->catalog, comparator, &self->service);
}

void
engine_free(Engine* self)
{
	catalog_free(&self->catalog);
	service_free(&self->service);
}

void
engine_open(Engine* self)
{
	// recover system objects
	catalog_open(&self->catalog);

	// recover partitions
	engine_recover(self);
}

void
engine_close(Engine* self)
{
	catalog_close(&self->catalog);
}
