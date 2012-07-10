-- Postgres specific configuration script

Create Table %SCHEMA%SchemaVersion (Version Integer);

Insert Into %SCHEMA%SchemaVersion (Version) Values (3);

Create Table %SCHEMA%SessionLog (Id Serial Primary Key Not Null,
	Command varchar(32),
	StartTime Timestamp,
	EndTime Timestamp,
	Hostname varchar(256),
	Username varchar(256),
	SessionId varchar(32),
	VirtRepos varchar(256),
	PhysRepos varchar(256),
	Client varchar(64),
	FinalReturnCode Integer);

Create Table %SCHEMA%CommitLog (Id Serial Primary Key Not Null,
	SessionId Integer,
	Directory varchar(256),
	Message text,
	Type char(1),
	Filename varchar(256),
	Tag varchar(64),
	BugId varchar(64),
	OldRev varchar(64),
	NewRev varchar(64),
	Added Integer,
	Removed Integer,
	Diff text);

Create Index %SCHEMA%Commit_SessionId On %SCHEMA%CommitLog(SessionId);

Create Table %SCHEMA%HistoryLog (Id Serial Primary Key Not Null,
	SessionId Integer,
	Type char(1),
	WorkDir varchar(256),
	Revs varchar(64),
	Name varchar(256),
	BugId varchar(64),
	Message text);

Create Index %SCHEMA%History_SessionId on %SCHEMA%HistoryLog(SessionId);

Create Table %SCHEMA%TagLog (Id Serial Primary Key Not Null,
	SessionId Integer,
	Directory varchar(256),
	Filename varchar(256),
	Tag varchar(64),
	Revision varchar(64),
	Message text,
	Action varchar(32),
	Type char(1));
	
Create Index %SCHEMA%Tag_SessionId on %SCHEMA%TagLog(SessionId);
