-- Oracle upgrade to v3

Delete from %SCHEMA%SchemaVersion; 
Insert Into %SCHEMA%SchemaVersion (Version) Values (3);
Alter Table %SCHEMA%SessionLog Add Column FinalReturnCode Number;

