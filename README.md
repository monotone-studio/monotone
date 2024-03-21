
![image description](.github/logo.png)

## Storage for Sequential data and IoT

Monotone is a modern embeddable data storage designed from the ground up precisely
for sequential workloads, such as append write and range scans. The storage architecture
is inspired by a Log-Structured approach and implements a custom-made `memory-disk-cloud`
hybrid engine.

Made to match the following requirements:

- Collect and process events in large volumes serially or by time (auto-increment by default)
- Write events as fast as possible, maintaining order and persistence
- Delete or Update events by primary key when necessary (but rarely or never needed)
- Read events serially or by time as fast as possible
- Efficiently store and manage large volumes of data using partitions
- Efficiently compress data
- Transparently update partitions or recompress data without blocking readers and writers
- Extend disk space without downtime by plugging additional storage
- Transparently work on top of S3
- Understand Hot and Cold data patterns

Monotone provides simple [C API](monotone/main/api/monotone.h).

Insert (and replace/delete) is done in batches using the event id associated with raw data.
Data are read using cursors.
Data management, administration and monitoring are done by using `SQL-style` DDL commands.

Learn more about its [Architecture](ARCHITECTURE.md).

## Features

- **Automatic Range Partitioning**

    Automatically partition data by range or time intervals (min/max).
	Transparently create partitions on write.
	Support partitions in the past and future.
	Automatically or manually refresh partitions on disk or cloud after being updated.

- **Transparent Compression**

    Compress or recompress partitions automatically on refresh or partition move.
	Allow different compression types and compression level settings.
	Everything is done transparently without blocking readers and writers.

- **Storages**

    Create storage to store data on different storage devices.
	Set different storage settings, such as compression, associated cloud, etc.
	Manually or automatically move partitions between storages.
	Automatic compaction is made when moving to change settings.
	Create or drop storage online.
	Extend disk space by adding new storage without downtime.

- **Data Tiering**

    Understand Hot and Cold data by creating data policies involving several storages.
	Define Pipeline to specify where partitions are created and when they need to be moved or dropped.
	All are done automatically or manually.

	```SQL
	--
	-- Example:
	--
	-- Create and keep all new partitions on SSD storage for a short duration
	-- of time for a faster refresh and read. Automatically move partitions
	-- to cold HDD storage when hot storage reaches its limit. Automatically
	-- Delete older partitions from cold storage when they reach their limit.
	--
	CREATE STORAGE hot (path '/mnt/ssd', compression 'zstd')
	CREATE STORAGE cold (path '/mnt/hdd', compression 'zstd')

	ALTER PIPELINE hot (size 10G), cold (size 100G)
	```

- **Bottomless Storage**

    Associate storage with the cloud for extensive and cheaper storage.
	Transparently access partitions on the cloud.
	Automate partitions lifecycle for the cloud using Pipeline.

    Automatically or manually handle updates by reuploading partitions to the cloud.
	Download or upload partitions.
	Move partitions between local storage and the cloud.
	Move partitions between different cloud services.

	```SQL
	--
	-- Example:
	--
	-- Store recent data on SSD for one day, then move to S3.
	--
	CREATE CLOUD s3 (type 's3', access_key 'minioadmin', secret_key 'minioadmin', url 'localhost:9000')
	CREATE STORAGE hot (compression 'zstd')
	CREATE STORAGE cold (cloud 's3', compression 'zstd')

	ALTER PIPELINE hot (interval 1day), cold
	```

	```SQL
	--
	-- Example:
	--
	-- Work on top of S3.
	--
	CREATE CLOUD s3 (type 's3', access_key 'minioadmin', secret_key 'minioadmin', url 'localhost:9000')
	ALTER STORAGE main (cloud 's3', compression 'zstd')
	```
  
## Interactive Benchmarking

Monotone ships with the client application, which can do simple interactive benchmarking and
execute commands in runtime: `monotone bench.` One can use it to play around with settings and get a sense of performance.

## Performance

Performance numbers to expect (**single instance, single thread writer**):

**Without WAL**

Disabling WAL allows us to get maximum out of the storage and not get bound by IO.
Write is in-memory with eventual persistency per partition. Partitions are compressed and flushed ASAP by background workers.

The expected compression rate is more than `x25`, and write performance is more than `1 billion` metrics per second.

```
monotone bench -n -s 1000
write: 5522800 rps (5.52 million events/sec, 1398.65 million metrics/sec), 5335.42 MiB/sec
```

Those results depend on your hardware and can be scaled further by parallel writing
to the independent storage instances. Please note that writing 5GiB into memory requires appropriate memory capacity
to fit the updates until it is flushed to disk.

With recommended standard settings (100 bytes per event):

```
monotone bench -n
write: 7191600 rps (7.19 million events/sec, 203.16 million metrics/sec), 775.00 MiB/sec
```

**With WAL**

With enabled WAL (1000 bytes per event = 250 metrics per event):

```
monotone bench -s 1000
write: 1599600 rps (1.60 million events/sec, 405.10 million metrics/sec), 1545.33 MiB/sec
```

Writing 1.5GiB to WAL (uncompressed), performance depends on your storage throughput.


Max event throughput, with data size set to zero:

```
monotone bench -s 0 -b 800
write: 6012800 rps (6.01 million events/sec, 0.00 million metrics/sec), 74.55 MiB/sec
```

With recommended standard settings (100 bytes per event):

```
monotone bench
write: 4763000 rps (4.76 million events/sec, 134.55 million metrics/sec), 513.29 MiB/sec
```

## Build

#### OS

Currently only Linux environments are supported.

#### Dependencies

- cmake
- clang/gcc
- libcurl
- openssl
- zstd
- lz4

#### Build Release

```sh
make release
```

#### Build Release (pass cmake options directly)

```sh
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=<install_path> .
make
```

#### Build Debug

```sh
make debug
```

#### Running tests

```sh
make
cd test
./monotone-test
```
