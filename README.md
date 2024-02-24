
![image description](.github/logo.png)

## Storage for Sequential data and IoT

We designed modern embeddable data storage from groundup specifically for sequential workloads, such as append write and range scans.
Storage architecture is inspired by log-structured approach and implements custom made memory-disk hybrid engine.

Made to match following requirements:

  - Collect and process events in large volumes serially or by time (auto-increment by default)
  - Write events as fast as possible maintaining order and persistency
  - Delete or Update events by primary key when necessary (but rarely or never needed)
  - Read events serially or by time as fast as possible
  - Efficiently store and manage large volumes of data using partitions
  - Efficiently compress data
  - Transparently update partitions or recompress data without blocking writers and readers
  - Extend disk space without downtime by plugging additional storages
  - Transparently work on top of S3
  - Understand Hot and Cold data patterns

Learn more about [Architecture](ARCHITECTURE.md).

## API

Monotone provides simple API, which we tried to make intuitive and future-proof.

Insert (and replace/delete) is done in batches using key-value style approach using raw data.
Where data management, administration and monitoring is done by using SQL-style DDL commands.

*API ⇨*  [monotone.h](monotone/main/api/monotone.h)

## Automatic Range Partitioning

Automatically partition data by range or time intervals (min/max). Transparently create partitions on write.
Support partitions in past and future.

Each partition has in-memory storage associated with partition file. Eventually in-memory storage
and partition file merged together (refresh).

Automatically or manually refresh partitions on disk or cloud after being updated:
```
REFRESH PARTITION [IF EXISTS] <min>
REFRESH PARTITIONS FROM <min> TO <max>
```

Move partitions between storages:
```
MOVE PARTITION [IF EXISTS] <min> INTO <storage>
MOVE PARTITIONS FROM <min> to <max> INTO <storage>
```

Drop one or more partitions:
```
DROP PARTITION [IF EXISTS] <min> [ON STORAGE | ON CLOUD]
DROP PARTITIONS FROM <min> TO <max> [ON STORAGE | ON CLOUD]
```

## Transparent Compression

Compress or recompress partitions automatically on refresh. Everything is done transparently
without blocking readers or writers of the original partition.

## Storages

Create logical storages to store data on different storage devices and apply different settings,
such as compression.

```
CREATE [IF NOT EXISTS] STORAGE <name> (<options>)
DROP STORAGE [IF EXISTS] <name>
ALTER STORAGE <name> SET <name> TO <value>
ALTER STORAGE <name> RENAME TO <name>
```

Extend storage space by adding additional storages online. Set which storage will become
primary and store newly created partitions.

## Data Tiering

Understand Hot and Cold data by creating data policies involving several storages.
Define pipeline to specify where partitions are created first and when they needed to be
moved or dropped. All done automatically or manually.

*Example.*

Create and keep all new partitions on SSD storage for short duration of time for a faster refresh and read.
Automatically move partitions to cold HDD storage when hot storage reaches its limit.
Automatically delete older partitions from cold storage, when it reaches its limit.
Use different compression settings.

```
CREATE STORAGE hot (path '/mnt/ssd', compression 'lz4')
CREATE STORAGE cold (path '/mnt/hdd', compression 'zstd')

ALTER PIPELINE hot (size 10G), cold (size 100G)
```

## Bottomless Storage

Associate storages with cloud for extensive storage. Transparently access partitions on cloud.
Automate partitions lifecycle for cloud using Pipeline.

```
CREATE STORAGE hot (path '/mnt/ssd_nvme', compression 'lz4')
CREATE STORAGE cold (cloud 's3', compression 'zstd')

ALTER PIPELINE hot (interval 1day), cold
```

Automatically or manually handle updates by reuploading partitions to cloud.

Download partitions:
```
DOWNLOAD PARTITION [IF EXISTS] <min>
DOWNLOAD PARTITIONS FROM <min> TO <max>
```

Upload partitions:
```
UPLOAD PARTITION [IF EXISTS] <min>
UPLOAD PARTITIONS FROM <min> TO <max>
```

Move partitions between local storage and cloud. Move partitions between different cloud services.

## Performance

By keeping storage embedded within your application it is possible to achieve lowest latency and get much higher performance
comparing to a dedicated server solutions.

Performance numbers we achieved so far (single instance, single thread operations):

    150+ million metrics write / second (~ 600+ MiB per second, 4 bytes per metric)
    6-10 million events write / second (~ 100 bytes per event)
    20-30 million events read / second (up to ~ 2GiB per second)
