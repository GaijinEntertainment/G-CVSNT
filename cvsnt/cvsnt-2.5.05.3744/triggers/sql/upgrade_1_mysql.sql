-- Mysql upgrade to v2

Create Table %PREFIX%SchemaVersion (Version Integer);
Insert Into %PREFIX%SchemaVersion (Version) Values (2);
Alter Table %PREFIX%SessionLog Add Column EndTime datetime;
Alter Table %PREFIX%SessionLog Change Date StartTime datetime;

