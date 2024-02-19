#pragma once

//
// monotone
//
// time-series storage
//

void engine_write(Engine*, RowRef*, int);
void engine_replay(Engine*, LogWrite*);
