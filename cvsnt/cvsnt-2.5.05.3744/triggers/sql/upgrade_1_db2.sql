-- Db2 upgrade to v2

Create Table %PREFIX%SchemaVersion (Version Integer);
Insert Into %PREFIX%SchemaVersion (Version) Values (2);
Alter Table %PREFIX%SessionLog Rename Column Date To StartTime;
Alter Table %PREFIX%SessionLog Add Column EndTime Timestamp;
