#ifndef FEATURES_NET_LINK_TESTS_CONTRACT_H
#define FEATURES_NET_LINK_TESTS_CONTRACT_H

#include "features/net_link/net_link.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cases every transport must pass, written against net_link.h alone. An environment opens
   links and lets time pass; the cases never assume zero latency or a virtual clock, so a real
   transport runs them in real time with `pass` sleeping and pumping its platform. */

typedef struct net_link_contract_env {
    void *context;
    void (*begin)(void *context); /* a fresh network for one case */
    void (*end)(void *context);   /* releases it and every link still open */
    net_link_t *(*open_host)(void *context, uint64_t identity, uint32_t max_peers);
    net_link_t *(*open_guest)(void *context, uint64_t identity);
    void (*pass)(void *context, double seconds);
    double step_s;           /* one pass between polls */
    double settle_s;         /* a connect or a message lands within this long */
    /* Everything already sent has landed after this long with nothing new sent; zero: settle_s.
       A real-time transport sets it far below settle_s, which must also cover a first connect. */
    double quiet_s;
    /* A wait that proves nothing more comes. It must outlast what the transport does late by its
       own timers (a resend, a keepalive, a timeout), so it is longer than quiet_s; zero: settle_s. */
    double silence_s;
    double fail_s;           /* a refused or unreachable connect is reported within this long */
    bool exact_identity;     /* identities are the numbers asked for, not the platform's own */
    bool refused_names_host; /* REFUSED carries the host's identity; false: it carries zero */
    bool unreliable_arrives; /* clean conditions: no unreliable message is lost */
    uint32_t guest_links;    /* guest links one process holds at once; cases needing two skip at one */
    /* Optional, NULL skips the cut cases: stops everything both ways on the path of `guest`. */
    void (*cut)(void *context, net_link_t *guest, bool cut);
    double timeout_s; /* a cut this long disconnects both ends */
} net_link_contract_env_t;

/* Unity cases; each reads the env set by net_link_contract_use. */
void net_link_contract_use(const net_link_contract_env_t *env);
void net_link_contract_run_all(void);
/* Only the cases whose name contains `name_part`; NULL runs all. For a quick look at one rule. */
void net_link_contract_run_matching(const char *name_part);

#ifdef __cplusplus
}
#endif

#endif
