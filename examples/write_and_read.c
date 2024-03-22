
//
// monotone
//
// time-series storage
//

#include <monotone_private.h>
#include <monotone.h>

int
main(int argc, char* argv[])
{
	// create environment
	monotone_t* env = monotone_init(NULL, NULL);
	if (env == NULL)
	{
		fprintf(stderr, "monotone_init() failed\n");
		return EXIT_FAILURE;
	}

	// repeat log messages to the console
	monotone_execute(env, "set log_to_stdout to true", NULL);

	// create or open repository
	int rc = monotone_open(env, "./example_repo");
	if (rc == -1)
	{
		monotone_free(env);
		fprintf(stderr, "monotone_open() failed: %s\n", monotone_error(env));
		return EXIT_FAILURE;
	}

	// write one million events using batches
	const int batch_size = 200;
	monotone_event_t batch[batch_size];

	for (int i = 0; i < 5000; i++)
	{
		for (int j = 0; j < batch_size; j++)
		{
			// set id to UINT64_MAX for the serial mode, id will be assigned
			// automatically
			monotone_event_t* event = &batch[j];
			event->id        = UINT64_MAX;
			event->data      = NULL;
			event->data_size = 0;
			event->remove    = false;
		}
		rc = monotone_write(env, batch, batch_size);
		if (rc == -1)
		{
			monotone_free(env);
			fprintf(stderr, "monotone_write() failed: %s\n", monotone_error(env));
			return EXIT_FAILURE;
		}
	}

	// read all events, starting from zero
	monotone_event_t key =
	{
		.id        = 0,
		.data      = NULL,
		.data_size = 0,
		.remove    = false
	};
	monotone_cursor_t* cursor = monotone_cursor(env, NULL, &key);
	if (cursor == NULL)
	{
		monotone_free(env);
		fprintf(stderr, "monotone_cursor() failed: %s\n", monotone_error(env));
		return EXIT_FAILURE;
	}

	int total = 0;
	for (;;)
	{
		// read current cursor event
		monotone_event_t event;
		rc = monotone_read(cursor, &event);
		if (rc == -1)
		{
			fprintf(stderr, "monotone_read() failed: %s\n", monotone_error(env));
			break;
		}
		if (rc == 0)
			break;

		total++;

		// iterate forward
		rc = monotone_next(cursor);
		if (rc == -1)
		{
			fprintf(stderr, "monotone_next() failed: %s\n", monotone_error(env));
			break;
		}
	}

	printf("total: %d\n\n", total);

	// show statistics
	char* result = NULL;
	rc = monotone_execute(env, "show storages", &result);
	if (rc == -1)
		fprintf(stderr, "monotone_execute() failed: %s\n", monotone_error(env));
	if (result)
	{
		printf("%s\n", result);
		free(result);
	}

	monotone_free(cursor);
	monotone_free(env);

	// unused
	(void)argc;
	(void)argv;

	return EXIT_SUCCESS;
}
