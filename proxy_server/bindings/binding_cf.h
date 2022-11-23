// Include guard
#ifndef BINDING_CF_H
#define BINDING_CF_H

// Makes functions available in C++
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <arpa/inet.h> // in_addr_t, inet_addr

/**
 * Initializes the proxy server and binds it to a CoreFoundation run loop
 * @param rl The run loop to attach the event listeners to
 * @param addr The ip address of the ATEM switcher to connect to, or 0 to disable
 */
void atem_server_init(CFRunLoopRef rl, const in_addr_t addr);

/**
 * Enables the relay client and connects it to an ATEM switcher at the address of the argument addr
 * @param addr The ip address of the ATEM switcher to connect to
 */
void atem_relay_enable(const in_addr_t addr);

/**
 * Disables the relay client and disconnects from the ATEM switcher
 */
void atem_relay_disable(void);

// Exists extern C block
#ifdef __cplusplus
}
#endif // __cplusplus

#endif // BINDING_CF_H
