#pragma once

//
// monotone
//
// time-series storage
//

typedef struct Execute    Execute;
typedef struct Executable Executable;

typedef void (*ExecuteFunction)(Executable*);

struct Execute
{
	ExecuteFunction function;
	bool            lock;
};

struct Executable
{
	Main* main;
	Cmd*  cmd;
	Buf*  output;
};

void main_execute(Main*, const char*, char**);
