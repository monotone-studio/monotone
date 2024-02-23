
![image description](.github/logo.png)

## Storage for Sequential data and IoT

We designed modern embeddable data storage from groundup specifically for sequential workloads, such as append write and range scans.
Storage architecture is inspired by log-structured approach and implements custom made memory-disk hybrid engine.

Made to match following requirements:

  - Collect and process events in large volumes serially or by time (auto-increment by default)
  - Write events as fast as possible maintaining order and persistency
  - Delete or Update events by primary key when necessary (but rarely or never needed)
  - Read events by time or serially as fast as possible
  - Efficiently store and manage large volumes of data using partitions
  - Efficiently compress data
  - Transparently update partitions or recompress data without blocking writers and readers
  - Extend disk space without downtime by plugging additional storages
  - Store data on top of S3
  - Understand Hot and Cold data patterns

Event is a pair of a serial id (or time) + raw data (plus additional key, if necessary).

*API ⇨*  [monotone.h](monotone/main/api/monotone.h)

## Automatic Range Partitioning

Automatically partition data by range or time interval on write.
Support partitions in past and future.

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

## Scalable Compaction

## Storages

Use logical storages to store data on different storage devices or apply different settings,
such as compression.

```
CREATE [IF NOT EXISTS] STORAGE <name> (<options>)
DROP STORAGE [IF EXISTS] <name>
ALTER STORAGE <name> SET <name> TO <value>
ALTER STORAGE <name> RENAME TO <name>
```

Extend storage space by adding additional storages online.

### Understand Hot and Cold data

Example: apply different storage settings for each storage individually:

```
CREATE STORAGE hot (path '/mnt/ssd_nvme', compression 'lz4')
CREATE STORAGE cold (path '/mnt/hdd', compression 'zstd')
```

### Bottomless Storage

### Performance

By keeping storage embeddable within your application it is possible to achieve lowest latency and get much higher performance
comparing to a dedicated server solutions.

Typical performance numbers to expect:

    150M+ metrics / second (~ 600+ MiB per second, 4 bytes per metric)
    10M+ events write / second
    30M+ events read / second (up to ~ 2GiB per second)

After successful write data immediately available for further
read without delay.

Range scans optimized for reduced IO and never do more then 1 request at time to a underlying storage device or a cloud service.
