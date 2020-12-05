#pragma once
enum {LOG_NOTIFY, LOG_WARNING, LOG_ERROR};
//these functions has to be resolved as link time dependency
void blob_logmessage(int log, const char *msg,...);
