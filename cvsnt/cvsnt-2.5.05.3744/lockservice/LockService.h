#ifndef __LOCKSERVICE__H
#define __LOCKSERVICE__H

#include "../version_no.h"
#include "../version_fu.h"

bool OpenLockClient(CSocketIOPtr s);
bool CloseLockClient(CSocketIOPtr s);
bool ParseLockCommand(CSocketIOPtr s, const char *command);

void run_server(int port, int seq, int local_only);

void InitServer();
void CloseServer();

#endif

