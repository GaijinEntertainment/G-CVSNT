-- DB2 specific configuration script

Create Table %PREFIX%SchemaVersion (Version Integer);

Insert Into %PREFIX%SchemaVersion (Version) Values (3);

Create Table %PREFIX%SessionLog (Id Integer Generated Always As Identity Primary Key,
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

Create Table %PREFIX%CommitLog (Id Integer Generated Always As Identity Primary Key,
	SessionId Integer,
	Directory varchar(256),
	Message clob,
	Type char(1),
	Filename varchar(256),
	Tag varchar(64),
	BugId varchar(64),
	OldRev varchar(64),
	NewRev varchar(64),
	Added Integer,
	Removed Integer,
	Diff clob);

Create Index %PREFIX%Commit_SessionId On %PREFIX%CommitLog(SessionId);

Create Table %PREFIX%HistoryLog (Id Integer Generated Always As Identity Primary Key,
	SessionId Integer,
	Type char(1),
	WorkDir varchar(256),
	Revs varchar(64),
	Name varchar(256),
	BugId varchar(64),
	Message clob);

Create Index %PREFIX%History_SessionId on %PREFIX%HistoryLog(SessionId);

Create Table %PREFIX%TagLog (Id Integer Generated Always As Identity Primary Key,
	SessionId Integer,
	Directory varchar(256),
	Filename varchar(256),
	Tag varchar(64),
	Revision varchar(64),
	Message clob,
	Action varchar(32),
	Type char(1));
	
Create Index %PREFIX%Tag_SessionId on %PREFIX%TagLog(SessionId);
