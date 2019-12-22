
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_cc.h"

// ta duo file operation ena gia to diavasam kai ena gia to write
static file_ops reader_ops ={
.Open  = NULL,
.Read  = reader,
.Write = NULL,
.Close = close_reader
};
static file_ops writer_ops={
.Open  = NULL,
.Read  = NULL,
.Write = writer,
.Close = close_writer
};
//arxikopoihsh tou pipe
int sys_Pipe(pipe_t* pipe)
{

	Fid_t fid[2];
	FCB   *fcb[2];

	if ( !FCB_reserve(2, fid, fcb) )
		return -1;

	pipe->read  = fid[0];
	pipe->write = fid[1];
	PPCB *ppcb = (PPCB *)malloc(sizeof(PPCB));
	ppcb->pit.read  =  fid[0];
	ppcb->pit.write =  fid[1];
	ppcb->reader_count  =    0;
	ppcb->write_count = 	0;
	ppcb->bytes = 	0;
	ppcb->reader_cond_var     = COND_INIT;
	ppcb->writer_cond_var     = COND_INIT;

	fcb[0]->streamfunc = &reader_ops;
	fcb[1]->streamfunc = &writer_ops;

	fcb[0]->streamobj = ppcb;
	fcb[1]->streamobj = ppcb;

	return 0;
}
// h sunarthsh gia to operation tou diavasmatos 
int reader(void* this, char *buf, unsigned int size)
{
	PPCB *pipe;
	int count = 0;
	if (this == NULL) // an de uparxei
		return -1;

	pipe = (PPCB *)this;
	// otan o reader exei ftasei ton writer alla exei einai akomi alive o writer
	while ( pipe->bytes == 0 && pipe->pit.write != -1 )
		kernel_wait( &(pipe->reader_cond_var), SCHED_PIPE );

	if (pipe->bytes == 0 && pipe->pit.write == -1) // an einai kenos apo stoixeia 
		return 0;
	
	for (int i = 0; i < size; i++) // diavasma 
	{
		if (pipe->bytes == 0) // an einai kenos
			break;
		buf[i] = pipe->buffer[pipe->reader_count]; // read 
	
		pipe->reader_count++; //aujanw

		if (pipe->reader_count >= BUFFER_SIZE) // to kanw reset gia na mhn bgw ektos oriwn
			pipe->reader_count = 0;

		count++;
		pipe->bytes--;

	}
	kernel_broadcast( &(pipe->writer_cond_var) );
	return count;
}

int close_reader(void* this)
{
	PPCB *pipe;
	if (this == NULL) // an de uparxei
		return -1;
	pipe = (PPCB *)this;

	pipe->pit.read = -1; // to kleinw

	if (pipe->pit.write != -1) // ama akomi o writer xrisimopoiei to pipe de to kanw destroy
		return 0;

	
	return 0;
}
// omoiws me thn read aplws einai h antistrfh diadikasia
int writer(void* this, const char* buf, unsigned int size)
{

	PPCB *pipe;
	int count = 0;
	if (this == NULL)
		return -1;
	pipe = (PPCB *)this;
	while (pipe->bytes >= BUFFER_SIZE)
		kernel_wait( &(pipe->writer_cond_var), SCHED_PIPE );
	
	if (pipe->pit.read == -1)
		return -1;
	
	for (int i = 0; i < size; i++)
	{
		if (pipe->bytes >= BUFFER_SIZE)
			break;
		pipe->buffer[pipe->write_count] = buf[i];
		pipe->write_count++;
		if (pipe->write_count >= BUFFER_SIZE)
			pipe->write_count = 0;
		count++;
		pipe->bytes++;

	}
	kernel_broadcast( &(pipe->reader_cond_var) );
	return count;
}
//omoiws me thn close_readr
int close_writer(void* this)
{
	//Get the pipe object.
	PPCB *pipe;

	if (this == NULL)
		return -1;
	pipe = (PPCB *)this;
	pipe->pit.write = -1;
	kernel_broadcast( &(pipe->reader_cond_var) );
	if (pipe->pit.read != -1)
		return 0;
	
	return 0;
}

