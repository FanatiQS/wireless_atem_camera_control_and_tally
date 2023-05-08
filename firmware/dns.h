#ifndef DNS_H
#define DNS_H

#include <lwip/udp.h> // struct udp_pcb

struct udp_pcb* captive_portal_init(void);

#endif // DNS_H
