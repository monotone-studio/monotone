#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Execute    Execute;
typedef struct Executable Executable;

typedef void (*ExecuteFunction)(Executable*);

enum
{
	EXECUTE_LOCK_NONE,
	EXECUTE_LOCK_SHARED,
	EXECUTE_LOCK_EXCLUSIVE,
};

struct Execute
{
	ExecuteFunction function;
	int             lock;
};

struct Executable
{
	Main* main;
	Cmd*  cmd;
	Buf*  output;
};

void main_execute(Main*, const char*, char**);
