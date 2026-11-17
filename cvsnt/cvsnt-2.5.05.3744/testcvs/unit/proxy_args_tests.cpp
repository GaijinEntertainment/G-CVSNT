// Unit tests for the cafs_proxy_server command line: master_url and the
// listen port.
//
// Build: see testcvs/README.md.
//
// Exit status is 0 if every check passed.

#include <cstdio>
#include <string>

#include "../../keyValueServer/proxy/cafs_proxy_args.h"

static int g_checks = 0;
static int g_failures = 0;

static void check(bool cond, const char *what)
{
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::printf("  FAIL  <%s>\n", what);
    }
}

static void check_master_url(const char *arg, const char *want_host, int want_port)
{
    std::string host;
    int port = -1;
    check(cafs_split_master_url(arg, host, port) && host == want_host && port == want_port, arg);
}

static void check_master_url_refused(const char *arg)
{
    std::string host;
    int port = -1;
    check(!cafs_split_master_url(arg, host, port), arg);
}

static void test_master_url()
{
    check_master_url("cafs.lan", "cafs.lan", 2403);
    check_master_url("cafs.lan:2404", "cafs.lan", 2404);
    check_master_url("cafs.lan@2404", "cafs.lan", 2404);
    check_master_url("10.0.0.1:1", "10.0.0.1", 1);
    check_master_url("10.0.0.1@65535", "10.0.0.1", 65535);
    check_master_url("::1", "::1", 2403);
    check_master_url("::1@2404", "::1", 2404);

    const char *refused[] = {"", ":2404", "@2404", "cafs.lan:", "cafs.lan@", "cafs.lan:0",
                             "cafs.lan:65536", "cafs.lan:24x", "cafs.lan:-1"};
    for (const char *s : refused) {
        check_master_url_refused(s);
    }
}

static void test_port()
{
    int port = -1;
    check(cafs_parse_port("2404", port) && port == 2404, "2404");
    check(cafs_parse_port("1", port) && port == 1, "1 is the lowest port");
    check(cafs_parse_port("65535", port) && port == 65535, "65535 is the highest port");

    const char *refused[] = {"", "0", "65536", "99999999999999999999", "abc", "24x", "-1"};
    for (const char *s : refused) {
        port = 7;
        check(!cafs_parse_port(s, port) && port == 7, s);
    }
}

int main()
{
    test_master_url();
    test_port();

    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
