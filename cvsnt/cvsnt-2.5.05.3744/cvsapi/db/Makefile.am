## Process this file with automake to produce Makefile.in

libdir=$(cvs_library_dir)/database

if WITH_MYSQL
mysql_la_SOURCES = \
	mysql/MySqlConnection.cpp \
	mysql/MySqlConnection.h \
	mysql/MySqlConnectionInformation.cpp \
	mysql/MySqlConnectionInformation.h \
	mysql/MySqlRecordset.cpp \
	mysql/MySqlRecordset.h 
mysql_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir)
mysql_la_LIBADD = @MYSQL_LIBS@
mysql_database_lib = mysql.la
else
mysql_database_lib =
endif

if WITH_SQLITE
sqlite_la_SOURCES = \
	sqlite/SQLiteConnection.cpp \
	sqlite/SQLiteConnection.h \
	sqlite/SQLiteConnectionInformation.cpp \
	sqlite/SQLiteConnectionInformation.h \
	sqlite/SQLiteRecordset.cpp \
	sqlite/SQLiteRecordset.h 
sqlite_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir)
sqlite_la_LIBADD = @SQLITE_LIBS@
sqlite_database_lib = sqlite.la
else
sqlite_database_lib =
endif

if WITH_POSTGRES
postgres_la_SOURCES = \
	postgres/PostgresConnection.cpp \
	postgres/PostgresConnection.h \
	postgres/PostgresConnectionInformation.cpp \
	postgres/PostgresConnectionInformation.h \
	postgres/PostgresRecordset.cpp \
	postgres/PostgresRecordset.h 
postgres_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir)
postgres_la_LIBADD = @POSTGRES_LIBS@
postgres_database_lib = postgres.la
else
libcvsapi_postres_lib =
endif

if WITH_ODBC
odbc_la_SOURCES = \
	odbc/OdbcConnection.cpp \
	odbc/OdbcConnection.h \
	odbc/OdbcConnectionInformation.cpp \
	odbc/OdbcConnectionInformation.h \
	odbc/OdbcRecordset.cpp \
	odbc/OdbcRecordset.h 
odbc_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir)
odbc_la_LIBADD = @ODBC_LIBS@
odbc_database_lib = odbc.la
else
odbc_database_lib =
endif

#if WITH_ORACLE
#oracle_la_SOURCES = \
#	oracle/OracleConnection.cpp \
#	oracle/OracleConnection.h \
#	oracle/OracleConnectionInformation.cpp \
#	oracle/OracleConnectionInformation.h \
#	oracle/OracleRecordset.cpp \
#	oracle/OracleRecordset.h 
#oracle_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir)
#oracle_la_LIBADD = @ORACLE_LIBS@
#oracle_database_lib = oracle.la
#else
oracle_database_lib =
#endif

AM_CPPFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/cvsapi $(includeopt)

lib_LTLIBRARIES = $(mysql_database_lib) \
	$(sqlite_database_lib) $(postgres_database_lib) \
	$(odbc_database_lib) $(oracle_database_lib)



