
/* advmsg.c - adventure interpreter message routines */
/*
	Copyright (c) 1986, by David Michael Betz
	All rights reserved
*/
#include "header.h"

/* cache size */
#define CSIZE	8


/* message block cache */
static char *mbuffer[CSIZE];	/* message text block cache buffers */
static int mblock[CSIZE];	/* message text block cache block numbers */
static int mnext[CSIZE];	/* next most recently used block */
static int mhead,mtail;		/* head and tail of lru list */

/* message file variables */
static int mbase;		/* message base block */
static strid_t mfd;			/* message file descriptor */

/* current message variables */
static int mblk;		/* current block */
static char *mbuf;		/* current buffer */
static int moff;		/* current buffer offset */


/* Headers */
void get_block(unsigned int blk);
int decode(int ch);



/* msg_init - initialize the message routines */
void msg_init(strid_t fd,int base)
{
    char *p;
    int i;

    /* remember the message file descriptor and base */
    mbase = base;
    mfd = fd;

    /* initialize the cache */
    if ((p = malloc(CSIZE * 512)) == NULL)
		error("insufficient memory");
    
	for (i = 0; i < CSIZE; i++) {
		mbuffer[i] = p; p += 512;
		mblock[i] = -1;
		mnext[i] = i+1;
    }
    mhead = 0; mtail = CSIZE-1; mnext[mtail] = -1;
}

/* msg_open - open a message */
void msg_open(unsigned int msg)
{
    /* save the current message block */
    mblk = msg >> 7;

    /* make sure the first block is in a buffer */
    get_block(mblk);

    /* setup the initial offset into the block */
    moff = (msg & 0x7F) << 2;
}

/* msg_byte - get a byte from a message */
int msg_byte()
{
    /* check for end of block and get next block */
    if (moff >= 512) {
	get_block(++mblk);
	moff = 0;
    }

    /* return the next message byte */
    return (decode(mbuf[moff++]));
}

/* decode - decode a character */
int decode(int ch)
{
    return ((ch + 30) & 0xFF);
}

/* get_block - get a block of message text */
void get_block(unsigned int blk)
{
    int last,n;
    long loff;

    /* first check the cache */
    for (n = mhead; n != -1; last = n, n = mnext[n])
	if (blk == mblock[n]) {
	    if (n != mhead) {
		if ((mnext[last] = mnext[n]) == -1)
		    mtail = last;
		mnext[n] = mhead;
		mhead = n;
	    }
	    mbuf = mbuffer[n];
	    return;
	}

    /* overwrite the least recently used buffer */
    mblock[mtail] = blk;
    loff = ((long) mbase + (long) blk) << 9;
    glk_stream_set_position(mfd,loff,seekmode_Start);
    if (glk_get_buffer_stream(mfd,mbuffer[mtail],512) != 512)
		error("error reading message text");

    /* get the block */
    get_block(blk);
}
