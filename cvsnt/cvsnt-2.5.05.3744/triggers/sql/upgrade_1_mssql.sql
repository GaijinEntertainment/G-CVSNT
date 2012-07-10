-- Mssql upgrade to v2

Create Table %PREFIX%SchemaVersion (Version Integer);
Insert Into %PREFIX%SchemaVersion (Version) Values (2);
Alter Table %PREFIX%SessionLog Add Column EndTime DateTime;
EXEC sp_rename '%PREFIX%SessionLog.Date', 'StartTime', 'COLUMN'
