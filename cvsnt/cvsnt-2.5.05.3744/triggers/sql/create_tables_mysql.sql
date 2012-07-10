-- MySql specific configuration script

Create Table %PREFIX%SchemaVersion (Version Integer);

Insert Into %PREFIX%SchemaVersion (Version) Values (3);

Create Table %PREFIX%SessionLog (Id Integer Auto_Increment Primary Key Not Null,
	Command varchar(32),
	StartTime datetime,
	EndTime datetime,
	Hostname varchar(255),
	Username varchar(255),
	SessionId varchar(32),
	VirtRepos varchar(255),
	PhysRepos varchar(255),
	Client varchar(64),
	FinalReturnCode Integer);

Create Table %PREFIX%CommitLog (Id Integer Auto_Increment Primary Key Not Null,
	SessionId Integer,
	Directory varchar(255),
	Message text,
	Type char(1),
	Filename varchar(255),
	Tag varchar(64),
	BugId varchar(64),
	OldRev varchar(64),
	NewRev varchar(64),
	Added Integer,
	Removed Integer,
	Diff text);

Create Index %PREFIX%Commit_SessionId On %PREFIX%CommitLog(SessionId);

Create Table %PREFIX%HistoryLog (Id Integer Auto_Increment Primary Key Not Null,
	SessionId Integer,
	Type char(1),
	WorkDir varchar(255),
	Revs varchar(64),
	Name varchar(255),
	BugId varchar(64),
	Message text);

Create Index %PREFIX%History_SessionId on %PREFIX%HistoryLog(SessionId);

Create Table %PREFIX%TagLog (Id Integer Auto_Increment Primary Key Not Null,
	SessionId Integer,
	Directory varchar(255),
	Filename varchar(255),
	Tag varchar(64),
	Revision varchar(64),
	Message text,
	Action varchar(32),
	Type char(1));
	
Create Index %PREFIX%Tag_SessionId on %PREFIX%TagLog(SessionId);
