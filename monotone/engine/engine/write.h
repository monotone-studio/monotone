#pragma once

//
// monotone
//
// time-series storage
//

void engine_write(Engine*, EventArg*, int);
void engine_replay(Engine*, LogWrite*);
