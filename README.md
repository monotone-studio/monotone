
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

Automatically or manually update (refresh) partitions on disk or cloud after being updated:
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

Create and keep all new partitions on NVME storage for short duration of time for a faster write
and read. Automatically move partitions to colder storage when hot storage reaches its limit.
Automatically delete older partitions from cold storage, when it reaches its limit.
Use different compression settings.

```
CREATE STORAGE hot (path '/mnt/ssd_nvme', compression 'lz4')
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

## Architecture

Storage architecture is inspired by log-structured approach and implements custom made memory-disk hybrid engine.
Data stored in sorted partition files. Partitions never overlap. Whole database can be seen as a 64bit range sparse array of events.

Event is a pair of [u64 id, raw data]. This can be seen almost the same as in key-value approach, beside the addition of id field.
Event Id is used as a key and represents serial (or time) primary key. Additionally it is possible to specify custom comparator, which
can be used to implement compound key (use additional embedded key in your data together with id).

Each partition file contains a number of sorted regions. Regions represent a sorted list of events, variable in size and compressed (~150KiB).
Region size can affect compression efficiency and reduce number of requests required to fulfill a scan.

Each partition has ordered in-memory storage associated with partition file. Eventually in-memory storage
and partition file merged together. This is done manually or in background by a pool of workers.
Compaction is multi-threaaded and scalable. Each partition file has index of regions, which contains of min/max keys of each region
and its file position. This index is kept in memory and loaded on start. Index size is insignificant even for huge data sets.

Partition refresh does not block readers and writers. Partitions can be updated or readed while being refreshed at the same time without
blocking. This is implemented by switching partition to the second in-memory storage and done in a several short locking stages.
Storage designed to survive and automatically recover from crashes, which could potentially happen during
refresh (power outage). Refresh automation is designed to reduce write-amplification as much as possible. Ideally, partition should
be written once using only memory storage as a source. Partition refresh scheduled automatically, when partition reaches its
refresh watermark (or can be done manually at any time).

In-memory storage (memtable) implemented using T-tree style data structure to reduce pointers overhead and increase data locality.
Each partition is using custom made memory allocator context optimized for sequential write and atomic free of large memory regions. 
This solves problem of memory fragmentation of malloc, greately improves data locality and allows multi-threaded compaction to work
without affecting other threads. Futhermore, the allocator optimized for using Linux Huge Pages by design (but not mandatory).

Range scans optimized for reduced IO and the idea that it can work on top of cloud. Unlike typical B-tree or LSM trees, Monotone storage never
does more then 1 read at time to a underlying storage device or a cloud service when using cursors. This is possible to do, because region
index is kept in memory.
Each cursor is doing merge join between partition file region and in-memory storages
(one or two in case of the parallel refresh).

Write operations never require disk access and done in-memory (with optional use of WAL for persistency).
After successful write, data immediately available for further read without delay. Any written event can be deleted or updated in the future (same as in a typical key-value storage).
Write is done in batches and transactional (atomical). Batching is mostly necessary to increase performance when using WAL.
Write never succeded unless data written to WAL. It is also possible to disable WAL to have higher performance. If it is not critical to lose latest updates,
which were yet not refreshed and synced with disk. Write-ahead logs are automatically deleted when data are saved to partitions.

Storage does not implement any kind of MVCC or Snapshot Isolation. This is done consciously to avoid problems with unavoidable
necessity for garbage collection (VACUUM). Locking is done per partition instead, which is more inline with the common usage patterns.
It is possible to read and write to different partitions without blocking each other.
Storage designed to work in multi-threaded applications.

Overall design follows ideas from Sophia and PostgreSQL (Timescale), addopted for IoT and sequential data
storage/access patterns.
