#pragma once

//
// monotone
//
// time-series storage
//

void part_open(Part*, int, bool);
void part_create(Part*, int);
void part_delete(Part*, int);
void part_rename(Part*, int, int);
