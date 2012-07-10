/*
 * Linking module for PuTTY proper: list the available backends
 * including ssh.
 *
 * cvsnt - only need ssh
 */

#include "putty/putty.h"

struct backend_list backends[] = {
    {PROT_SSH, "ssh", &ssh_backend},
//    {PROT_TELNET, "telnet", NULL},
//    {PROT_RLOGIN, "rlogin", NULL},
//    {PROT_RAW, "raw", NULL},
    {0, NULL}
};
