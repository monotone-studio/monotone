
![image description](.github/logo.png)

### Storage for Sequential data

Monotone is a modern embeddable data storage designed from the ground up precisely
for sequential workloads, such as append write and range scans.

Sequential workloads are at the foundation of the following use cases: `IoT, events, time series, crypto, blockchain, finance, monitoring,
logs collection, and Kafka-style workloads`.

Using Monotone, you can create high-performance serverless solutions for storage and processing data directly within your application.

Made to match the following requirements:

- Collect and process events in large volumes serially or by time (auto-increment by default)
- Write events as fast as possible, maintaining order and persistence
- Delete or Update events by primary key when necessary (but rarely or never needed)
- Read any range of events as fast as possible using the primary key
- Efficiently store and manage large volumes of data using partitions
- Efficiently compress and encrypt data
- Transparently update partitions and recompress, decrypt data without blocking readers and writers
- Extend disk space without downtime by plugging additional storage
- Seamlessly work on top of S3
- Make sense of Hot and Cold data patterns using Data Tiering

The storage architecture is inspired by a Log-Structured approach and implements a custom-made `memory-disk-cloud`
hybrid engine.

Learn more about its [Architecture](ARCHITECTURE.md).

----

### Features

- **Automatic Range Partitioning**

    Automatically partition data by range or time intervals (min/max).
	Transparently create partitions on write.
	Support partitions in the past and future.
	Automatically or manually refresh partitions on disk or cloud after being updated.
	Automatically exclude unrelated partitions during reading.

- **Transparent Compression**

    Compress and recompress partitions automatically on refresh or partition move.
	Allow different compression types and compression level settings.
	Everything is done transparently without blocking readers and writers.

- **Transparent Encryption**
	
	Encrypt and decrypt partitions automatically on refresh, partition move, or read.
	Compatible with compression and done transparently without blocking readers and writers.
  
- **Multiple Storages**

    Create storage to store data on different storage devices.
	Set different storage settings, such as compression, encryption, associated cloud, etc.
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
 	-- Store recent data on SSD for one day, then move to S3.
	CREATE CLOUD s3 (type 's3', access_key 'minioadmin', secret_key 'minioadmin', url 'localhost:9000')
	CREATE STORAGE hot (compression 'zstd')
	CREATE STORAGE cold (cloud 's3', compression 'zstd', encryption 'aes')

	ALTER PIPELINE hot (duration 1day), cold
	```

	```SQL
	-- Work on top of S3.
 	CREATE CLOUD s3 (type 's3', access_key 'minioadmin', secret_key 'minioadmin', url 'localhost:9000')
	ALTER STORAGE main (cloud 's3', compression 'zstd')
	```

----

### API

Monotone provides simple [C API](monotone/main/api/monotone.h).

Insert (and replace/delete) is done in batches using the event `id` associated with `raw data`.
Data are read using cursors.
Data management, administration, and monitoring are done by using `SQL-style` `DDL` commands.

----

#### Interactive Benchmarking

Monotone ships with the client application, which can do simple interactive benchmarking and
execute commands in runtime: `monotone bench.` You can use it to experiment with settings and data management commands and get a sense of performance.

#### Performance

Some arbitrary performance numbers for **single instance** using **single writer thread**:

**With WAL**

With standard settings (100 bytes per event):

```
monotone bench
write: 5847000 rps (5.85 million events/sec, 165.18 million metrics/sec), 630.10 MiB/sec
```

Maxing out metrics: (1000 bytes per event = 250 metrics per event):

```
monotone bench -s 1000
write: 1867200 rps (1.87 million events/sec, 472.87 million metrics/sec), 1803.85 MiB/sec
```
Writing 1.5+ GiB to WAL (uncompressed), performance depends on your storage device throughput.

Max event throughput with WAL (data size set to zero, 1000 events per write):

```
monotone bench -s 0 -b 1000
write: 8059000 rps (8.06 million events/sec, 0.00 million metrics/sec), 99.91 MiB/sec
```

Read all events:

```
> /select 0 432000000
read:         30513948 rps (30.51 million events/sec, 862.02 million metrics/sec), 3288.30 MiB/sec
read events:  432000000 (432.0 millions)
read metrics: 47412.0 millions
read size:    46554.6 MiB
read time:    14.2 secs
```

**Without WAL**

Disabling WAL allows us to get maximum out of the storage performance and not get bound by IO.
Write is in-memory. Partitions are compressed, flushed, and synced to disk ASAP by background workers.

With standard settings (100 bytes per event):

```
monotone bench -n
write: 10954400 rps (10.95 million events/sec, 309.46 million metrics/sec), 1180.50 MiB/sec
```

![image description](.github/bench.gif)

#### Five Seconds to Mars

Following results depend on your hardware (RAM / CPU) and can be scaled further by playing with the benchmark settings.

Maxing out metrics: (1000 bytes per event = 250 metrics per event):

```
monotone bench -n -s 1000
write: 6949800 rps (6.95 million events/sec, 1760.04 million metrics/sec), 6714.01 MiB/sec
```

The expected compression rate using `zstd` is `25-86x`, and write performance is more than `1.5 billion` metrics per second for a
single thread writer.

Parallel writing to two independent storage instances:

```
monotone bench -n -s 1000 -i2
write: 9007800 rps (9.01 million events/sec, 2281.23 million metrics/sec), 8702.18 MiB/sec
```

Please note that writing 5+ GiB into memory requires appropriate memory capacity to fit the updates until
partitions are flushed to disk and CPU/compaction workers to handle the compaction.

----

#### Build

#### OS

Currently only Linux environments are supported.

#### Dependencies

- cmake
- gcc (recommended) or clang
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
