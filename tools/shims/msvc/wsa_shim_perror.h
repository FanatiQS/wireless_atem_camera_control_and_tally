// Include guard
#ifndef WSA_SHIM_PERROR_H
#define WSA_SHIM_PERROR_H

#if __cplusplus
extern "C" {
#endif // __cplusplus

void wsa_shim_perror(const char* msg);

#if __cplusplus
}
#endif // __cplusplus

#endif // WSA_SHIM_PERROR_H
