#pragma once

//
// monotone
//
// time-series storage
//

// partition file
void part_file_open(Part*, bool);
void part_file_create(Part*);
void part_file_delete(Part*, bool);
void part_file_complete(Part*);

// cloud file
void part_file_cloud_open(Part*);
void part_file_cloud_create(Part*);
void part_file_cloud_delete(Part*, bool);
void part_file_cloud_complete(Part*);
