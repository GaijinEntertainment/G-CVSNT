-- Oracle specific configuration script

Create Table %SCHEMA%SchemaVersion (Version Number) %TABLESPACE%;

Insert Into %SCHEMA%SchemaVersion (Version) Values (3);

Grant Select On %SCHEMA%SchemaVersion To Public;

Create Table %SCHEMA%SessionLog (Id Number Not Null,
	Command nvarchar2(32),
	StartTime Timestamp,
	EndTime Timestamp,
	Hostname nvarchar2(256),
	Username nvarchar2(256),
	SessionId varchar(32),
	VirtRepos nvarchar2(256),
	PhysRepos nvarchar2(256),
	Client varchar(64),
	FinalReturnCode Number,
	CONSTRAINT SessionLog PRIMARY KEY(Id)) %TABLESPACE%;

Grant Select, Insert On %SCHEMA%SessionLog To Public;

Create Sequence %SCHEMA%SessionLog_sequence
	Start with 1
	Increment by 1
	Nomaxvalue
	Nocache	
	Noorder;

Grant Select On %SCHEMA%SessionLog_sequence To Public;

Create or Replace Trigger %SCHEMA%SessionLog_trigger
	Before insert on %SCHEMA%SessionLog
	  For each row
	  Begin
	    Select %SCHEMA%SessionLog_sequence.nextval into :new.Id from dual; End;;

Create Table %SCHEMA%CommitLog (Id Number Not Null,
	SessionId Number,
	Directory nvarchar2(256),
	Message nclob,
	Type char(1),
	Filename nvarchar2(256),
	Tag varchar(64),
	BugId varchar(64),
	OldRev varchar(64),
	NewRev varchar(64),
	Added Number,
	Removed Number,
	Diff nclob,
	CONSTRAINT CommitLog PRIMARY KEY(Id)) %TABLESPACE%;

Grant Select, Insert On %SCHEMA%CommitLog To Public;

Create Sequence %SCHEMA%CommitLog_sequence
	Start with 1
	Increment by 1
	Nomaxvalue
	Nocache	
	Noorder;

Grant Select On %SCHEMA%CommitLog_sequence To Public;

Create Or Replace Trigger %SCHEMA%CommitLog_trigger
	Before insert on %SCHEMA%CommitLog
	  For each row
	  Begin
	    Select %SCHEMA%CommitLog_sequence.nextval into :new.Id from dual; End;;

Create Index %SCHEMA%Commit_SessionId On %SCHEMA%CommitLog(SessionId) %TABLESPACE%;

Create Table %SCHEMA%HistoryLog (Id Number  Not Null,
	SessionId Number,
	Type char(1),
	WorkDir nvarchar2(256),
	Revs varchar(64),
	Name nvarchar2(256),
	BugId varchar(64),
	Message nclob,
	CONSTRAINT HistoryLog PRIMARY KEY(Id)) %TABLESPACE%;

Grant Select, Insert On %SCHEMA%HistoryLog To Public;

Create Sequence %SCHEMA%HistoryLog_sequence
	Start with 1
	Increment by 1
	Nomaxvalue
	Nocache	
	Noorder;

Grant Select On %SCHEMA%HistoryLog_sequence To Public;

Create Or Replace Trigger %SCHEMA%HistoryLog_trigger
	Before insert on %SCHEMA%HistoryLog
	  For each row
	  Begin
	    Select %SCHEMA%HistoryLog_sequence.nextval into :new.Id from dual; End;;

Create Index %SCHEMA%History_SessionId on %SCHEMA%HistoryLog(SessionId) %TABLESPACE%;

Create Table %SCHEMA%TagLog (Id Number  Not Null,
	SessionId Number,
	Directory nvarchar2(256),
	Filename nvarchar2(256),
	Tag varchar(64),
	Revision varchar(64),
	Message nclob,
	Action varchar(32),
	Type char(1),
	CONSTRAINT TagLog PRIMARY KEY(Id)) %TABLESPACE%;

Grant Select, Insert On %SCHEMA%TagLog To Public;

Create Sequence %SCHEMA%TagLog_sequence
	Start with 1
	Increment by 1
	Nomaxvalue
	Nocache	
	Noorder;

Grant Select On %SCHEMA%TagLog_sequence To Public;

Create Trigger %SCHEMA%TagLog_trigger
	Before insert on %SCHEMA%TagLog
	  For each row
	  Begin
 	    Select %SCHEMA%TagLog_sequence.nextval into :new.Id from dual; End;;

Create Index %SCHEMA%Tag_SessionId on %SCHEMA%TagLog(SessionId) %TABLESPACE%;
