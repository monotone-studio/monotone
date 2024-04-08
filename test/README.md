
![image description](../.github/logo.png)

### Monotone QA and Testing

Monotone is using custom made test suite.

#### Testing

- functional

  Basic CRUD and Partition mapping.

- storage

  Storage specific DDL commands.

- cloud

  Storage specific DDL commands using cloud mock.

- recovery

  Storage behaviour after restart, crash recovery and error injections.

- s3

  Actual S3 testing using `minio` (which is required to be run locally)
