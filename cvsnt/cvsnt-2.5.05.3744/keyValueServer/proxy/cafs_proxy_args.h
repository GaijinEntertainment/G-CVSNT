#pragma once
#include <string>

// cafs_proxy_server command line: the master address and the listen port.

static constexpr int cafs_default_port = 2403;

// decimal digits only, 1..65535
bool cafs_parse_port(const char *s, int &port);
// host, host:port or host@port; without an @, a string with several colons is a bare host.
bool cafs_split_master_url(const char *arg, std::string &host, int &port);
