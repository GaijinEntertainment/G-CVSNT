#ifndef __CVSCONTROL__H
#define __CVSCONTROL__H

bool OpenControlClient(CSocketIOPtr s);
bool CloseControlClient(CSocketIOPtr s);
bool ParseControlCommand(CSocketIOPtr s, const char *command);

void run_server(int port, int seq, int local_only);

#endif

