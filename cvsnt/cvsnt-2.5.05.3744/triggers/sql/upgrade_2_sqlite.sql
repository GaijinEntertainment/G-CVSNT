-- Sqlite upgrade to v2

Delete from %PREFIX%SchemaVersion; 
Insert Into %PREFIX%SchemaVersion (Version) Values (3);
Alter Table %PREFIX%SessionLog Add Column FinalReturnCode Integer;



