#pragma once

//
// monotone
//
// time-series storage
//

// storage operations
void engine_refresh(Engine*, Refresh*, uint64_t, Str*, bool);
void engine_refresh_range(Engine*, Refresh*, uint64_t, uint64_t, Str*);
void engine_drop(Engine*, uint64_t, bool, int);
void engine_drop_range(Engine*, uint64_t, uint64_t, int);
void engine_rebalance(Engine*, Refresh*);
void engine_checkpoint(Engine*);

// cloud operations
void engine_download(Engine*, uint64_t, bool, bool);
void engine_download_range(Engine*, uint64_t, uint64_t, bool);
void engine_upload(Engine*, uint64_t, bool, bool);
void engine_upload_range(Engine*, uint64_t, uint64_t, bool);
