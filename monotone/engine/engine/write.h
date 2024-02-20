#pragma once

//
// monotone
//
// time-series storage
//

void engine_write(Engine*, EventArg*, int);
void engine_write_replay(Engine*, LogWrite*);
