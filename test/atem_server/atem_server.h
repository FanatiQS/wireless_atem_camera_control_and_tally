// Include guard
#ifndef ATEM_SERVER_H
#define ATEM_SERVER_H

void atem_server_open(void);
void atem_server_open_extended(void);
void atem_server_close(void);
void atem_server_data(void);
void atem_server_data_extended(void);
void atem_server_cc(void);
void atem_server_commands(void);

void atem_server(void);
void atem_server_extended(void);

#endif // ATEM_SERVER_H
