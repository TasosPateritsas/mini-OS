#ifndef __KERNEL_SOCKET_H
#define __KERNEL_SOCKET_H

#include "tinyos.h"
#include "kernel_streams.h"


typedef enum socket_type {
LISTENER,
PEER,
UNBOUND
}socket_type;

typedef struct socket_control_block SCB;


typedef struct Listener{
  rlnode request;
  CondVar listener_cond_var;
}listener;

typedef struct Peer{
  PPCB* writer;
  PPCB* reader;
  SCB* P2P;
} peer;
typedef struct request{
  Fid_t fid;
  FCB* fcb;
  rlnode request_node;
  SCB* scb;
  CondVar request_cond_var;
  int  admit_f;
}request;

typedef struct socket_control_block{
  int refcount;
FCB* fcb;
Fid_t fid;
socket_type stype;
port_t port;
listener* listener_node;
peer* peer_node;
}SCB;




SCB* Port_map[MAX_PORT+1];

int socket_read(void* read,char*buffer , unsigned int size);
int socket_write(void* write,const char*buffer , unsigned int size);
int socket_close(void* fid);

#endif /* __KERNEL_SOCKET_H */
