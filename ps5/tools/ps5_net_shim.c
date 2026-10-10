/* Name lookup for PS5 titles: the console's libc exports getaddrinfo & co as
 * null imports (libScePosixForWebKit is not loaded in a native title), which
 * crashes libmod_net. IPv4 only, through sceNetResolver; taken from the
 * native-app-boilerplate's console_curl.c. */
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>

extern int sceNetPoolCreate(const char *name, int size, int flags);
extern int sceNetPoolDestroy(int pool);
extern int sceNetResolverCreate(const char *name, int pool, int flags);
extern int sceNetResolverStartNtoa(int resolver, const char *hostname, struct in_addr *address,
                                   int timeout_us, int retries, int flags);
extern int sceNetResolverDestroy(int resolver);

static int ps5_lookup(const char *name, struct in_addr *address)
{
    if (inet_pton(AF_INET, name, address) == 1)
        return 0;
    const int pool = sceNetPoolCreate("bgd_dns", 16 * 1024, 0);
    if (pool < 0)
        return EAI_MEMORY;
    int result = EAI_FAIL;
    const int resolver = sceNetResolverCreate("bgd_dns", pool, 0);
    if (resolver >= 0)
    {
        result = sceNetResolverStartNtoa(resolver, name, address, 5000000, 2, 0) < 0 ? EAI_NONAME : 0;
        (void)sceNetResolverDestroy(resolver);
    }
    (void)sceNetPoolDestroy(pool);
    return result;
}

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints,
                struct addrinfo **result)
{
    *result = NULL;
    const int family = hints != NULL ? hints->ai_family : AF_UNSPEC;
    if (family != AF_UNSPEC && family != AF_INET)
        return EAI_FAMILY;

    struct in_addr address;
    address.s_addr = htonl(INADDR_LOOPBACK);
    if (node != NULL)
    {
        if (hints != NULL && (hints->ai_flags & AI_NUMERICHOST) != 0)
        {
            if (inet_pton(AF_INET, node, &address) != 1)
                return EAI_NONAME;
        }
        else
        {
            const int failed = ps5_lookup(node, &address);
            if (failed != 0)
                return failed;
        }
    }
    else if (hints != NULL && (hints->ai_flags & AI_PASSIVE) != 0)
    {
        address.s_addr = htonl(INADDR_ANY);
    }

    struct addrinfo *entry = calloc(1, sizeof(struct addrinfo) + sizeof(struct sockaddr_in));
    if (entry == NULL)
        return EAI_MEMORY;
    struct sockaddr_in *in = (struct sockaddr_in *)(entry + 1);
    in->sin_len = sizeof(*in);
    in->sin_family = AF_INET;
    in->sin_port = htons((uint16_t)(service != NULL ? atoi(service) : 0));
    in->sin_addr = address;
    entry->ai_family = AF_INET;
    entry->ai_socktype = hints != NULL && hints->ai_socktype != 0 ? hints->ai_socktype : SOCK_STREAM;
    entry->ai_protocol = hints != NULL ? hints->ai_protocol : 0;
    entry->ai_addrlen = sizeof(*in);
    entry->ai_addr = (struct sockaddr *)in;
    *result = entry;
    return 0;
}

void freeaddrinfo(struct addrinfo *entry)
{
    while (entry != NULL)
    {
        struct addrinfo *next = entry->ai_next;
        free(entry);
        entry = next;
    }
}

const char *gai_strerror(int code)
{
    return code == 0 ? "no error" : "the name lookup failed";
}
