
#include "tinyos.h"
#include "util.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_cc.h"

static file_ops sock_ops={
	.Open  = NULL,
	.Read  = socket_read,
	.Write = socket_write,
	.Close = socket_close
};

PPCB *init_ppcb(Fid_t fid_1, Fid_t fid_2)
{
	PPCB *ppcb = (PPCB *)malloc(sizeof(PPCB));
	ppcb->pit.read  =     fid_1;
	ppcb->pit.write =     fid_2;
	ppcb->reader_count  = 0;
	ppcb->write_count = 0;
	ppcb->bytes = 0;
	ppcb->reader_cond_var   = COND_INIT;
	ppcb->writer_cond_var   = COND_INIT;

	return ppcb;
}

int socket_read(void* this, char *buf, unsigned int size)
{
	SCB *socket;
	if ( this == NULL )
		return -1;
	socket = (SCB *)this;
	if (socket->type != PEER)  // gia na diavasei prepei an einai peer
		return -1;
	if ( !socket->peer_node.read ) // an einai shutdowned
		return -1;
	return reader( (void *)socket->peer_node.reader, buf, size ); //read from pipe
}


int socket_write(void* this, const char* buf, unsigned int size)
{

	SCB *socket;

	if ( this == NULL )
		return -1;
	socket = (SCB *)this;

	if (socket->type != PEER) // gia na grapsei prepei na einai peer
		return -1;

	if ( !socket->peer_node.write ) // an einai shutdowned
		return -1;

	return writer( (void *)socket->peer_node.writer, buf, size ); //write from pipe
}

int socket_close(void* this)
{

	SCB *socket;

	if ( this == NULL )
		return -1;
	socket = (SCB *)this;

	if (socket->type == LISTENER)
	{
		port_map[socket->port] = NULL;
		kernel_broadcast( &(socket->listener_node.req) ); //jupnaei ta listening sockets
	}
	if (socket->type == PEER)
	{	//kleinei ta pipes
		close_reader( (void *)socket->peer_node.reader );
		close_writer( (void *)socket->peer_node.writer );	
	}

	free(socket);

	return 0;
}

// arxikopoihsh tou socket
Fid_t sys_Socket(port_t port)
{

	if (port >= MAX_PORT+1 || port < 0)
		return NOFILE;

	Fid_t fid[1];
	FCB   *fcb[1];

	if ( !FCB_reserve(1, fid, fcb) )
		return NOFILE;

	SCB *scb = (SCB *)malloc(sizeof(SCB));

	scb->fid         = fid[0];
	scb->fcb         = fcb[0];
	scb->port        = port;
	scb->ref_counter = 0;
	scb->type        = UNBOUND;
	
	fcb[0]->streamfunc = &sock_ops;
	fcb[0]->streamobj  = scb;

	return fid[0];
}

int sys_Listen(Fid_t sock)
{

	FCB *sock_fcb = get_fcb(sock);

	if (sock_fcb == NULL)
		return -1;

	SCB *socket = (SCB *)sock_fcb->streamobj; // pairnei to socket

	if (socket == NULL)
		return -1;
	if (socket->port == NOPORT) // den einai unbound se kapoio port
		return -1;

	if (port_map[socket->port] != NULL) //exei hdh listener
		return -1;

	if (socket->type != UNBOUND) //hdh einai initialized
		return -1;

	//to kataxwreis sthn oura kai einai to kanies listeing 
	rlnode_init( &(socket->listener_node.queue), NULL);

	socket->listener_node.req = COND_INIT;
	socket->type = LISTENER; // to kaneis listener
	port_map[socket->port] = socket;
	
	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{

	FCB *fcb = get_fcb(lsock);

	
	if (fcb == NULL) //einai keno
		return NOFILE;
	SCB *lscb = (SCB *)fcb->streamobj; // to pairneis
	if (lscb == NULL)
		return NOFILE;
	if (lscb->type != LISTENER) // an de eina listener feugeis
		return NOFILE;

	while ( rlist_len( &(lscb->listener_node.queue) ) <= 0 )  // perimeneis mexri na erthei kapoio request
	{		
		kernel_wait( &(lscb->listener_node.req), SCHED_USER );
	}
	if (lscb->type == UNBOUND) // to socket einai kleisto
		return NOFILE;
	rlnode *req_node = rlist_pop_front( &(lscb->listener_node.queue) ); // pairneis to prwto request
	RQS *request = req_node->rqs;

	// create 2 pipes
	PPCB *pipe1 = init_ppcb(lscb->fid, request->scb->fid);
	PPCB *pipe2 = init_ppcb(lscb->fid, request->scb->fid);

	Fid_t first_connect = sys_Socket(lscb->port);

	if (first_connect == NOFILE)
		return NOFILE;
		
	//initialize to socket kai to request
	SCB *scb_server = (SCB *) (get_fcb(first_connect)->streamobj);

	scb_server->peer_node.other     = (void *)request->scb;
	scb_server->peer_node.writer = pipe1;
	scb_server->peer_node.reader = pipe2;
	scb_server->peer_node.read   = 1;
	scb_server->peer_node.write  = 1;
	scb_server->type = PEER;


	request->scb->peer_node.other     = (void *)scb_server;
	request->scb->peer_node.writer = pipe2;
	request->scb->peer_node.reader = pipe1;
	request->scb->peer_node.read   = 1;
	request->scb->peer_node.write  = 1;
	request->scb->type = PEER;

	request->admit_f = 1;
	// jupnas to socket tou request
	kernel_broadcast( &(request->request_cond_var) );
	
	return first_connect;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{

	FCB *sock_fcb = get_fcb(sock);
	if (sock_fcb == NULL) // einai keno
		return -1;
	SCB *socket = (SCB *)sock_fcb->streamobj; //pairneis to socket

	if (socket == NULL)
		return -1;
	if (port >= MAX_PORT+1 || port < 0) //to port einai ektos oriwn
		return -1;
	if (socket->type != UNBOUND) // einia hdh initialize
		return -1;

	if (port_map[port] == NULL)
		return -1;
	//dhmiourgeis ena neo request
	RQS *request = (RQS *)malloc(sizeof(RQS));

	
	request->admit_f = 0;
	request->request_cond_var  = COND_INIT;
	request->scb   = socket;

	rlnode_init( &(request->node), request );
	rlist_push_back( &(port_map[port]->listener_node.queue), &(request->node) );
	kernel_broadcast( &(port_map[port]->listener_node.req) );
	//perimeneis mexri na teleiwsei to timeout h na ginei wake up 
	if(!kernel_timedwait( &(request->request_cond_var), SCHED_IO, timeout)){
		return -1;
	}
	
	// ama einai accepted 
	if (request->admit_f)
		return 0;

	return -1;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	FCB *sock_fcb = get_fcb(sock);

	if (sock_fcb == NULL)
		return -1;

	SCB *socket = (SCB *)sock_fcb->streamobj;
	if (socket == NULL)
		return -1;
	
	if (socket->type != PEER) // ama de  einai peer return
		return 0;
	//kleineis ton writer kai ton reader tou socket
	if (how == SHUTDOWN_READ || how == SHUTDOWN_BOTH){
		SCB *s = (SCB *)socket->peer_node.other;
		s->peer_node.write = 0;
		socket->peer_node.read = 0;
	}

	if (how == SHUTDOWN_WRITE || how == SHUTDOWN_BOTH)
	{
		socket->peer_node.write = 0; // kleineis ton writer 
		socket->peer_node.writer->pit.write = -1; //sigoureueis oti o reader tou allou socket 8a diavasei mexri to telos 
	}

	return 0;
}
