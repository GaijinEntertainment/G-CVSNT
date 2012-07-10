-- Postgres upgrade to v2

Create Table %SCHEMA%SchemaVersion (Version Integer);
Insert Into %SCHEMA%SchemaVersion (Version) Values (2);
Alter Table %SCHEMA%SessionLog Add Column EndTime Timestamp;
Alter Table %SCHEMA%SessionLog Rename Column Date To StartTime;

