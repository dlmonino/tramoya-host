#include <string.h>
#include "wk_connectivity.h"

bool wk_connectivity_host(const char *name)
{
    static const char *const hosts[] = {
        "ena.net.playstation.net", "get.net.playstation.net",
        "post.net.playstation.net", "www.msftconnecttest.com",
        "ipv6.msftconnecttest.com",
    };
    for (size_t i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++)
        if (strcmp(name, hosts[i]) == 0) return true;
    return false;
}

static bool path_is(const char *uri, const char *path)
{
    size_t length = strlen(path);
    return strncmp(uri, path, length) == 0 &&
           (uri[length] == '\0' || uri[length] == '?');
}

bool wk_connectivity_probe(const char *uri, bool upload,
                           const char **body, uint32_t *length)
{
    if (!uri) return false;
    if (upload) {
        if (!path_is(uri, "/networktest/post_128")) return false;
        *body = "";
        *length = 0;
    } else if (path_is(uri, "/netstart/icst") ||
               path_is(uri, "/netstart/ps4") || path_is(uri, "/netstart/ps5")) {
        *body = "Success";
        *length = 7;
    } else if (path_is(uri, "/connecttest.txt")) {
        *body = "Microsoft Connect Test";
        *length = 22;
    } else if (path_is(uri, "/networktest/get_2m") ||
               path_is(uri, "/networktest/get_6m")) {
        *body = NULL; /* Generate zeros from a small shared read-only buffer. */
        *length = path_is(uri, "/networktest/get_2m") ? 2097152u : 6291456u;
    } else {
        return false;
    }
    return true;
}
