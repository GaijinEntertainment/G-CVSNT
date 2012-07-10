/*	cvsnt sql script runner
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
// cvsscript.cpp : Defines the entry point for the console application.
//

#include <ctype.h>
#include <config.h>
#include <cvsapi.h>
#include <cvstools.h>

CSqlConnection *g_pDb;
CSqlConnectionInformation *g_pCI;
char g_engine[64];

// Old->New mapping
static const char *const dbList[] =
{
        "mysql",
        "sqlite",
        "postgres",
        "odbc",
        "mssql",
        "db2"
};
#define dbList_Max (sizeof(dbList)/sizeof(dbList[0]))

int main(int argc, char* argv[])
{
        int nType;
        char value[1024];
 
	if(argc!=2)
	{
		printf("Runs a script against the audit database\n\n");
	  	printf("Usage: %s <script>\n",argv[0]);
		return -1;
	}
        if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseEngine",g_engine,sizeof(g_engine)))
        {
                if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseType",value,sizeof(value)))
                        nType = atoi(value);
                else
                {
                        fprintf(stderr,"Audit Database engine not set\n");
                        return -1;
                }
                if(nType<0 || nType>=dbList_Max)
                {
                        fprintf(stderr,"Audit Database engine type invalid\n");
                        return -1;
                }
                strcpy(g_engine,dbList[nType]);
        }

        if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseName",value,sizeof(value)))
        {
                fprintf(stderr,"Audit  Database name not set\n");
                return -1;
        }

        g_pDb = CSqlConnection::CreateConnection(g_engine,CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDDatabase));

        if(!g_pDb)
        {
                fprintf(stderr,"Couldn't initialise %s database engine.\n",g_engine);
                return -1;
        }

        g_pCI = g_pDb->GetConnectionInformation();

        if(!g_pCI)
        {
                fprintf(stderr,"Could not get database connection information");
                return -1;
        }

        if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseHost",value,sizeof(value)))
                g_pCI->setVariable("hostname",value);
        if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseName",value,sizeof(value)))
                g_pCI->setVariable("database",value);
        if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseUsername",value,sizeof(value)))
                g_pCI->setVariable("username",value);
        if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabasePassword",value,sizeof(value)))
                g_pCI->setVariable("password",value);
        // Legacy support - this is not part of the base
        if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabasePrefix",value,sizeof(value)))
                g_pCI->setVariable("prefix",value);

        const char *v;
        for(size_t n=CSqlConnectionInformation::firstDriverVariable; (v = g_pCI->enumVariableNames(n))!=NULL; n++)
        {
                cvs::string tmp;
                cvs::sprintf(tmp,80,"AuditDatabase_%s_%s",g_engine,v);
                if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer",tmp.c_str(),value,sizeof(value)))
                        g_pCI->setVariable(v,value);
        }

        if(!g_pDb->Open())
        {
                fprintf(stderr,"Connection to database failed\n");
                return -1;
        }

        CFileAccess acc;
        cvs::string line,comp_line;
        if(!acc.open(argv[1],"r"))
        {
                g_pDb->Close();
		fprintf(stderr,"Couldn't open %s\n",argv[1]);
		return -1;
        }

        g_pDb->BeginTrans();

        comp_line="";
        while(acc.getline(line))
        {
        	if(line.size()<2 || !strncmp(line.c_str(),"--",2))
                	continue;
                comp_line+=" "+line;
                if(line[line.size()-1]!=';')
                	continue;

                comp_line.resize(comp_line.size()-1); // Some DBs don't like the semicolon...
                int state=0,start;
                for(size_t n=0; state>=0 && n<comp_line.size(); n++)
                {
                	char c = comp_line[n];
                        switch(state)
                        {
                        case 0:
                        	if(c=='%') {state=1; start=n; }
                                break;
                        case 1:
                                if(c=='%')
                                {
                                        // possible keyword %FOO%
                                        const char *var = g_pCI->getVariable(comp_line.substr(start+1,n-(start+1)).c_str());
                                        if(var)
                                        {
                                        	comp_line.replace(start,(n-start)+1,var);
                                                n+=(strlen(var)-((n-start)+1));
                                        }
                                        state=0;
                                        break;
                                }

                                if(!isupper(c)) state=0;
                                break;
                        }
                }
                g_pDb->Execute("%s",comp_line.c_str());
                if(g_pDb->Error())
                {
			fprintf(stderr,"%s\n",g_pDb->ErrorString());
			return -1;
                }
                comp_line="";
        }
        g_pDb->CommitTrans();
        g_pDb->Close();

	printf("%s executed successfuly\n",argv[1]);
  	return 0;
}

