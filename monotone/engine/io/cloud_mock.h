#pragma once

//
// monotone
//
// time-series storage
//

typedef struct CloudMock CloudMock;

struct CloudMock
{
	CloudIf iface;
};

void cloud_mock_init(CloudMock*);
