-- Sqlite upgrade to v2

Create Table %PREFIX%SchemaVersion (Version Integer);
Insert Into %PREFIX%SchemaVersion (Version) Values (2);
Alter Table %PREFIX%SessionLog Add Column StartTime datetime;
Alter Table %PREFIX%SessionLog Add Column EndTime datetime;
Update %PREFIX%SessionLog Set StartTime=Date;

-- It is impossible to drop or rename columns in sqlite so the date field is left in this
-- version. 


