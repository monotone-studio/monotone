#pragma once

//
// monotone
//
// time-series storage
//

void index_open(File*, Source*, Id*, int, Index*);
void index_read(File*, Source*, Index*, Buf*, bool);
