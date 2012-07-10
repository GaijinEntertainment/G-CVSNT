-- MS SQL specific configuration script

Create Table %PREFIX%SchemaVersion (Version Integer);

Insert Into %PREFIX%SchemaVersion (Version) Values (3);

Create Table %PREFIX%SessionLog (Id Integer Identity Primary Key Not Null,
	Command nvarchar(32),
	StartTime Datetime,
	EndTime Datetime,
	Hostname nvarchar(256),
	Username nvarchar(256),
	SessionId nvarchar(32),
	VirtRepos nvarchar(256),
	PhysRepos nvarchar(256),
	Client nvarchar(64),
	FinalReturnCode Integer);

Create Table %PREFIX%CommitLog (Id Integer Identity Primary Key Not Null,
	SessionId Integer,
	Directory nvarchar(256),
	Message text,
	Type char(1),
	Filename nvarchar(256),
	Tag nvarchar(64),
	BugId nvarchar(64),
	OldRev nvarchar(64),
	NewRev nvarchar(64),
	Added Integer,
	Removed Integer,
	Diff text);

Create Index %PREFIX%Commit_SessionId On %PREFIX%CommitLog(SessionId);

Create Table %PREFIX%HistoryLog (Id Integer Identity Primary Key Not Null,
	SessionId Integer,
	Type char(1),
	WorkDir nvarchar(256),
	Revs nvarchar(64),
	Name nvarchar(256),
	BugId nvarchar(64),
	Message text);

Create Index %PREFIX%History_SessionId on %PREFIX%HistoryLog(SessionId);

Create Table %PREFIX%TagLog (Id Integer Identity Primary Key Not Null,
	SessionId Integer,
	Directory nvarchar(256),
	Filename nvarchar(256),
	Tag nvarchar(64),
	Revision nvarchar(64),
	Message text,
	Action nvarchar(32),
	Type char(1));
	
Create Index %PREFIX%Tag_SessionId on %PREFIX%TagLog(SessionId);
