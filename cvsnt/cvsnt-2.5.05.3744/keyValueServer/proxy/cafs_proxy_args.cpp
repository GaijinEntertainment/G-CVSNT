#include "cafs_proxy_args.h"

bool cafs_parse_port(const char *s, int &port)
{
    long v = 0;
    for (const char *p = s; *p; ++p) {
        if (*p < '0' || *p > '9') {
            return false;
        }
        v = v * 10 + (*p - '0');
        if (v > 65535) {
            return false;
        }
    }
    if (v == 0) {
        return false;
    }
    port = (int)v;
    return true;
}

bool cafs_split_master_url(const char *arg, std::string &host, int &port)
{
    host = arg;
    port = cafs_default_port;
    size_t sep = host.rfind('@');
    if (sep == std::string::npos) {
        const size_t colon = host.find(':');
        if (colon != std::string::npos && host.find(':', colon + 1) == std::string::npos) {
            sep = colon;
        }
    }
    if (sep != std::string::npos) {
        if (!cafs_parse_port(host.c_str() + sep + 1, port)) {
            return false;
        }
        host.resize(sep);
    }
    return !host.empty();
}
