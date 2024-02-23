# monotone.

Store sequential data locally or in a cloud with seamless access directly within your application.

*API ⇨*  [monotone.h](monotone/main/api/monotone.h)

---

### Embeddable Cloud-Native Storage for Events and time-series data

We designed modern embeddable key-value storage from groundup specifically for sequential workloads, such as append write and range scans. Storage architecture is inspired by log-structured design and implements custom made memory-disk hybrid engine.

Made to match typical IoT requirements:

  - Collect and process events in large volumes serially or by time (auto-increment by default)
  - Write events as fast as possible
  - Delete or Replace events by primary key when necessary (but rarely or never needed)
  - Read events in time or serially as fast as possible
  - Efficiently manage large volumes of data using partitions
  - Efficiently compress data
  - Extend disk space without downtime by plugging additional storages or by using S3
  - Understand Hot and Cold data patterns

### Automatic Range Partitioning

Automatically partition data by range or time interval.

Automatically or manually update (refresh) partitions on disk or cloud:

```
REFRESH PARTITION [IF EXISTS] <min>
REFRESH PARTITIONS FROM <min> TO <max>
```

Move partitions between storages:
```
MOVE PARTITION [IF EXISTS] <min> INTO <storage>
MOVE PARTITIONS FROM <min> to <max> INTO <storage>
```

Drop one or more partition:
```
DROP PARTITION [IF EXISTS] <min> [ON STORAGE | ON CLOUD]
DROP PARTITIONS FROM <min> TO <max> [ON STORAGE | ON CLOUD]
```

### Transparent Compression and Recompression

### Scalable Compaction

### Understand Hot and Cold data

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

---
