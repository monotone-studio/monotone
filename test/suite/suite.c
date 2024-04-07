
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

#include <monotone_private.h>
#include <monotone.h>
#include <monotone_test.h>
#include <dlfcn.h>

static Test*
test_new(TestSuite*  self, TestGroup* group,
         const char* name,
         const char* description)
{
	Test* test = malloc(sizeof(*test));
	if (group == NULL)
		return NULL;
	test->name = strdup(name);
	if (test->name == NULL) {
		free(test);
		return NULL;
	}
	test->description = NULL;
	if (description) {
		test->description = strdup(description);
		if (test->description == NULL) {
			free(test->name);
			free(test);
			return NULL;
		}
	}
	test->group = group;
	list_init(&test->link_group);
	list_init(&test->link);
	list_append(&group->list_test, &test->link_group);
	list_append(&self->list_test, &test->link);
	return test;
}

static void
test_free(Test* test)
{
	free(test->name);
	if (test->description)
		free(test->description);
	free(test);
}

static Test*
test_find(TestSuite* self, const char* name)
{
	list_foreach(&self->list_test)
	{
		auto test = list_at(Test, link);
		if (strcasecmp(name, test->name) == 0)
			return test;
	}
	return NULL;
}

static TestGroup*
test_group_new(TestSuite*  self,
               const char* name,
               const char* directory,
               bool        optional)
{
	TestGroup* group = malloc(sizeof(*group));
	if (group == NULL)
		return NULL;
	group->optional = optional;
	group->name = strdup(name);
	if (group->name == NULL) {
		free(group);
		return NULL;
	}
	group->directory = strdup(directory);
	if (group->directory == NULL) {
		free(group->name);
		free(group);
		return NULL;
	}
	list_init(&group->list_test);
	list_init(&group->link);
	list_append(&self->list_group, &group->link);
	return group;
}

static void
test_group_free(TestGroup* group)
{
	list_foreach_safe(&group->list_test)
	{
		auto test = list_at(Test, link_group);
		test_free(test);
	}
	free(group->name);
	free(group->directory);
	free(group);
}

static TestGroup*
test_group_find(TestSuite* self, const char* name)
{
	list_foreach(&self->list_group)
	{
		auto group = list_at(TestGroup, link);
		if (strcasecmp(name, group->name) == 0)
			return group;
	}
	return NULL;
}

static int
test_suite_plan_group(TestSuite* self, char* arg, char* directory,
                      bool optional)
{
	char* name = test_arg(&arg);
	if (name == NULL || !strlen(name)) {
		test_error(self, "line %d: plan: bad group name",
		           self->current_plan_line);
		return -1;
	}
	auto group = test_group_find(self, name);
	if (group) {
		test_error(self, "line %d: plan: group '%s' redefined",
		           self->current_plan_line,
		           name);
		return -1;
	}
	group = test_group_new(self, name, directory, optional);
	self->current_group = group;
	return 0;
}

static int
test_suite_plan_test(TestSuite* self, char* arg)
{
	char* name = test_arg(&arg);
	char* description = test_chomp(arg);

	if (!strlen(name)) {
		test_error(self, "line %d: plan: bad test definition",
		           self->current_plan_line);
		return -1;
	}

	if (self->current_group == NULL) {
		test_error(self, "line %d: plan: test group is not defined for test",
		           self->current_plan_line);
		return -1;
	}

	auto test = test_find(self, name);
	if (test) {
		test_error(self, "line %d: plan: test '%s' redefined",
		           self->current_plan_line,
		           name);
		return -1;
	}

	test_new(self, self->current_group, name, description);
	return 0;
}

static int
test_suite_plan(TestSuite* self)
{
	FILE* file = fopen(self->option_plan_file, "r");
	if (! file) {
		test_error(self, "plan: failed to open plan file: %s",
		           self->option_plan_file);
		goto error;
	}
	self->current_plan_line = 1;

	char directory[PATH_MAX];
	memset(directory, 0, sizeof(directory));

	char query[PATH_MAX];
	int  rc;
	for (;; self->current_plan_line++)
	{
		if (! fgets(query, sizeof(query), file))
			break;

		if (*query == '\n' || *query == '#')
			continue;

		if (strncmp(query, "directory", 9) == 0)
		{
			// directory <name>
			char* arg = query + 9;
			char* name = test_arg(&arg);
			snprintf(directory, sizeof(directory), "%s", name);
		} else
		if (strncmp(query, "group_optional", 14) == 0)
		{
			if (strlen(directory) == 0)
			{
				test_error(self, "plan: %d: directory is not set",
				           self->current_plan_line);
				goto error;
			}
			// group_optional <name>
			rc = test_suite_plan_group(self, query + 14, directory, true);
			if (rc == -1)
				goto error;
		} else
		if (strncmp(query, "group", 5) == 0)
		{
			if (strlen(directory) == 0)
			{
				test_error(self, "plan: %d: directory is not set",
				           self->current_plan_line);
				goto error;
			}
			// group <name>
			rc = test_suite_plan_group(self, query + 5, directory, false);
			if (rc == -1)
				goto error;
		} else
		if (strncmp(query, "test", 4) == 0)
		{
			// test <name> <description>
			rc = test_suite_plan_test(self, query + 4);
			if (rc == -1)
				goto error;
		} else
		{
			test_error(self, "plan: %d: unknown plan file command",
			           self->current_plan_line);
			goto error;
		}
	}

	fclose(file);
	return 0;

error:
	if (file)
		fclose(file);
	return -1;
}

static int
test_suite_test_check(TestSuite* self)
{
	// new test
	if (! self->current_test_ok_exists)
	{
		test_info(self, "\033[1;32mnew\033[0m\n");
		return 0;
	}

	// diff
	struct stat result_stat;
	stat(self->current_test_result_file, &result_stat);
	int rc;
	rc = test_sh(self, "cmp -s -n %d %s %s", result_stat.st_size,
	             self->current_test_result_file,
	             self->current_test_ok_file);
	if (rc == 0)
	{
		test_info(self, "%s", "\n");
		return 0;
	}

	if (self->option_fix) {
		test_info(self, "%s", "\033[1;35mfixed\033[0m\n");
	} else {
		test_info(self, "%s", "\033[1;31mfailed\033[0m\n");
	}
	return -1;
}

static int
test_suite_test(TestSuite* self, char* description)
{
	// finish previous test
	int rc = 0;
	if (self->current_test_started)
		rc = test_suite_test_check(self);
	else
		self->current_test_started = 1;

	// start new test
	test_chomp(description);
	test_info(self, "    - %s ", description);
	return rc;
}

static int
test_suite_execute(TestSuite* self, Test* test, char* options)
{
	test_sh(self, "mkdir %s", self->option_result_dir, test->name);

	char test_file[PATH_MAX];
	snprintf(test_file, sizeof(test_file), "%s/%s.test",
	         test->group->directory, test->name);

	if (test->description && options)
		test_info(self, "  \033[0;33m%s\033[0m %s: %s\n", test->name, test->description, options);
	else
	if (test->description)
		test_info(self, "  \033[0;33m%s\033[0m %s\n", test->name, test->description);
	else
	if (options)
		test_info(self, "  \033[0;33m%s\033[0m: %s\n", test->name, options);
	else
		test_info(self, "  \033[0;33m%s\033[0m\n", test->name);

	FILE *file;
	file = fopen(test_file, "r");
	if (! file) {
		test_error(self, "test: failed to open test file: %s", test_file);
		return -1;
	}

	char test_result_file[PATH_MAX];
	snprintf(test_result_file, sizeof(test_result_file), "%s/%s.test.result",
	         test->group->directory, test->name);

	char test_ok_file[PATH_MAX];
	snprintf(test_ok_file, sizeof(test_ok_file), "%s/%s.test.ok",
	         test->group->directory, test->name);

	struct stat ok_stat;
	int  rc;
	rc = stat(test_ok_file, &ok_stat);
	self->current_test_ok_exists = rc != -1;
	self->current_test_ok_file = test_ok_file;
	self->current_test_result_file = test_result_file;
	self->current_test = test_file;
	self->current = test;
	self->current_test_options = options;
	self->current_test_started = 0;
	self->current_line = 1;
	self->current_test_result = fopen(test_result_file, "w");
	if (self->current_test_result == NULL) {
		test_error(self, "test: failed to open test result file: %s",
		           test_result_file);
		fclose(file);
		return -1;
	}

	int test_failed = 0;
	char query[16384];
	for (;; self->current_line++)
	{
		if (! fgets(query, sizeof(query), file))
			break;

		if (*query == '\n')
			continue;

		if (*query == '#')
		{
			// test case
			if (strncmp(query, "# test: ", 8) == 0)
			{
				test_log(self, "%s", query);
				rc = test_suite_test(self, query + 8);
				test_failed += (rc == -1);
			}
			continue;
		}

		test_log(self, "%s", query);

		rc = test_suite_cmd(self, query);
		if (rc == -1)
			return -1;
	}

	if (self->current_test_started)
	{
		rc = test_suite_test_check(self);
		test_failed += rc == -1;
	}

	fclose(file);
	fclose(self->current_test_result);
	self->current_test_result = NULL;;

	// create new test
	if (! self->current_test_ok_exists)
		test_sh(self, "cp %s %s", test_result_file, test_ok_file);

	// fix the test
	if (test_failed && self->option_fix) {
		test_sh(self, "cp %s %s", test_result_file, test_ok_file);
		test_failed = 0;
		// one-time effect
		self->option_fix = 0;
	}

	// check env
	if (self->list_cursor_count > 0)
	{
		test_error(self, "%s", "test: cursors left open");
		return -1;
	}
	if (self->env)
	{
		test_error(self, "%s", "test: env left open");
		return -1;
	}

	// show diff and stop
	if (test_failed) {
		test_sh(self, "git diff --no-index %s %s", test_ok_file, test_result_file);
		return -1;
	}

	// cleanup
	unlink(test_result_file);

	test_suite_cleanup(self);
	return 0;
}

static int
test_suite_execute_with_options(TestSuite* self, Test* test)
{
	char options_file[PATH_MAX];
	snprintf(options_file, sizeof(options_file), "%s/%s.test.options",
	         test->group->directory, test->name);
	int rc;
	FILE* file = fopen(options_file, "r");
	if (file) {
		char options[512];
		while ((fgets(options, sizeof(options), file)))
		{
			if (*options == '\n' || *options == '#')
				continue;
			char* p = options;
			while (*p != '\n')
				p++;
			*p = 0;
			rc = test_suite_execute(self, test, options);
			if (rc == -1) {
				fclose(file);
				return -1;
			}
		}
		fclose(file);
		return 0;
	}
	return test_suite_execute(self, test, NULL);
}

static int
test_suite_execute_group(TestSuite* self, TestGroup* group)
{
	test_info(self, "%s\n", group->name);

	self->current_group = group;
	list_foreach(&group->list_test)
	{
		auto test = list_at(Test, link_group);
		int rc;
		rc = test_suite_execute_with_options(self, test);
		if (rc == -1)
			return -1;
	}
	return 0;
}

void
test_suite_init(TestSuite* self)
{
	self->dlhandle                 = NULL;
	self->current_test_ok_exists   = 0;
	self->current_test_ok_file     = 0;
	self->current_test_result_file = 0;
	self->current_test_started     = 0;
	self->current_test_options     = NULL;
	self->current_test             = NULL;
	self->current_test_result      = NULL;
	self->current_line             = 0;
	self->current_plan_line        = 0;
	self->current                  = NULL;
	self->current_group            = NULL;
	self->env                      = NULL;
	self->list_cursor_count        = 0;

	self->option_result_dir        = NULL;
	self->option_test              = NULL;
	self->option_group             = NULL;
	self->option_fix               = 0;

	list_init(&self->list_cursor);
	list_init(&self->list_test);
	list_init(&self->list_group);
}

void
test_suite_free(TestSuite* self)
{
	list_foreach_safe(&self->list_group)
	{
		auto group = list_at(TestGroup, link);
		test_group_free(group);
	}
	if (self->dlhandle)
	{
		dlclose(self->dlhandle);
		self->dlhandle = NULL;
	}
}

void
test_suite_cleanup(TestSuite* self)
{
	if (self->current_test_result)
	{
		fclose(self->current_test_result);
		self->current_test_result = NULL;
	}
	self->current = NULL;
	self->current_test_options = NULL;
	self->current_test = NULL;
	self->current_test_result = NULL;
	self->current_line = 0;

	test_sh(self, "rm -rf %s", self->option_result_dir);
}

int
test_suite_run(TestSuite* self)
{
	test_suite_cleanup(self);

	self->dlhandle = dlopen(NULL, RTLD_NOW);

	// read plan file
	int rc;
	rc = test_suite_plan(self);
	if (rc == -1)
		return -1;
	self->current_group = NULL;

	// run selected group
	if (self->option_group)
	{
		auto group = test_group_find(self, self->option_group);
		if (group == NULL) {
			test_error(self, "suite: group is not '%s' defined",
			           self->option_group);
			return -1;
		}
		return test_suite_execute_group(self, group);
	}

	// run selected test
	if (self->option_test)
	{
		auto test = test_find(self, self->option_test);
		if (test == NULL) {
			test_error(self, "suite: test '%s' is not defined",
			           self->option_test);
			return -1;
		}
		return test_suite_execute_with_options(self, test);
	}

	// run all tests groups
	list_foreach(&self->list_group)
	{
		auto group = list_at(TestGroup, link);

		// skip optional groups
		if (group->optional)
			continue;

		rc = test_suite_execute_group(self, group);
		if (rc == -1)
			return -1;
	}
	return 0;
}
