-- Postgres upgrade to v3

Delete from %PREFIX%SchemaVersion; 
Insert Into %SCHEMA%SchemaVersion (Version) Values (3);
Alter Table %SCHEMA%SessionLog Add Column FinalReturnCode Integer;

