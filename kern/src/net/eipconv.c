// INFERNO
#include <vfs.h>
#include <kfs.h>
#include <slab.h>
#include <kmalloc.h>
#include <kref.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <error.h>
#include <cpio.h>
#include <pmap.h>
#include <smp.h>
#include <ip.h>

enum {
	Isprefix = 16,
};

uint8_t prefixvals[256] = {
	[0x00] 0 | Isprefix,
	[0x80] 1 | Isprefix,
	[0xC0] 2 | Isprefix,
	[0xE0] 3 | Isprefix,
	[0xF0] 4 | Isprefix,
	[0xF8] 5 | Isprefix,
	[0xFC] 6 | Isprefix,
	[0xFE] 7 | Isprefix,
	[0xFF] 8 | Isprefix,
};

static char *efmt = "%02x:%02x:%02x:%02x:%02x:%02x";
static char *ifmt = "%d.%d.%d.%d";

void printemac(void (*putch) (int, void **), void **putdat, uint8_t * mac)
{
	printfmt(putch, putdat, efmt, mac[0], mac[1], mac[2], mac[3], mac[4],
			 mac[5]);
}

void printip(void (*putch) (int, void **), void **putdat, uint8_t * ip)
{
	int i, j, eln, eli;
	uint16_t s;
	if (memcmp(ip, v4prefix, 12) == 0)
		printfmt(putch, putdat, ifmt, ip[12], ip[13], ip[14], ip[15]);
	else {
		/* find longest elision */
		eln = eli = -1;
		for (i = 0; i < 16; i += 2) {
			for (j = i; j < 16; j += 2)
				if (ip[j] != 0 || ip[j + 1] != 0)
					break;
			if (j > i && j - i > eln) {
				eli = i;
				eln = j - i;
			}
		}

		/* print with possible elision */
		for (i = 0; i < 16; i += 2) {
			if (i == eli) {
				/* not sure what to do ... we don't get
				 * the number of bytes back from printing.
				 */
				printfmt(putch, putdat, "::");
				i += eln;
				if (i >= 16)
					break;
			} else if (i != 0)
				printfmt(putch, putdat, ":");

			s = (ip[i] << 8) + ip[i + 1];
			printfmt(putch, putdat, "0x%x", s);
		}
	}
}

void printipv4(void (*putch) (int, void **), void **putdat, uint8_t * p)
{
	printfmt(putch, putdat, ifmt, p[0], p[1], p[2], p[3]);
}

void printipmask(void (*putch) (int, void **), void **putdat, uint8_t * ip)
{
	int i, j, n;
	/* look for a prefix mask */
	for (i = 0; i < 16; i++)
		if (ip[i] != 0xff)
			break;
	if (i < 16) {
		if ((prefixvals[ip[i]] & Isprefix) == 0) {
			printip(putch, putdat, ip);
			return;
		}
		for (j = i + 1; j < 16; j++)
			if (ip[j] != 0) {
				printip(putch, putdat, ip);
				return;
			}
		n = 8 * i + (prefixvals[ip[i]] & ~Isprefix);
	} else
		n = 8 * 16;

	/* got one, use /xx format */
	printfmt(putch, putdat, "/%d", n);
}

void printqid(void (*putch) (int, void **), void **putdat, struct qid *q)
{
	printfmt(putch, putdat, "{path:%p,type:%02x,vers:%p}",
		 q->path, q->type, q->vers);

}

void printcname(void (*putch) (int, void **), void **putdat, struct cname *c)
{
	printfmt(putch, putdat, "{ref %d, alen %d, len %d, s %s}",
		 kref_refcnt(&c->ref), c->alen, c->len, c->s);
}

void printchan(void (*putch) (int, void **), void **putdat, struct chan *c)
{

	printfmt(putch, putdat, "%slocked ", spin_locked(&c->lock) ? "":"un");
	printfmt(putch, putdat, "refs %p ", kref_refcnt(&c->ref));
//	printfmt(putch, putdat, "%p ", struct chan *next,
//	printfmt(putch, putdat, "%p ", struct chan *link,
	printfmt(putch, putdat, "off %p ", c->offset);
	printfmt(putch, putdat, "type %p ", c->type);
	printfmt(putch, putdat, "dev %p ", c->dev);
	printfmt(putch, putdat, "mode %p ", c->mode);
	printfmt(putch, putdat, "flag %p ", c->flag);
	printfmt(putch, putdat, "qid");
	printqid(putch, putdat, &c->qid);
	printfmt(putch, putdat, " fid %p ", c->fid);
	printfmt(putch, putdat, "iounit %p ", c->iounit);
	printfmt(putch, putdat, "umh %p ", c->umh);
	printfmt(putch, putdat, "umc %p ", c->umc);
//      printfmt(putch, putdat, "%p ", qlock_t umqlock,
	printfmt(putch, putdat, "uri %p ", c->uri);
	printfmt(putch, putdat, "dri %p ", c->dri);
	printfmt(putch, putdat, "mountid %p ", c->mountid);
	printfmt(putch, putdat, "mntcache %p ", c->mcp);
	printfmt(putch, putdat, "mnt %p ", c->mux);
	printfmt(putch, putdat, "aux %p ", c->aux);
	printfmt(putch, putdat, "mchan %p ", c->mchan);
	printfmt(putch, putdat, "mqid %p ");
	printqid(putch, putdat, &c->mqid);
	printfmt(putch, putdat, " cname ");
	printcname(putch, putdat, c->name);
	printfmt(putch, putdat, " ateof %p ", c->ateof);
	printfmt(putch, putdat, "buf %p ", c->buf);
	printfmt(putch, putdat, "bufused %p ", c->bufused);
}

static uint8_t testvec[11][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 1, 3, 4, 5,},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	 0xff, 0xff, 0xff, 0xff,},
	{0xff, 0xff, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
	{0xff, 0xff, 0xff, 0xc0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
	{0xff, 0xff, 0xff, 0xff, 0xe0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	 0xff, 0xff, 0xff, 0xff,},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
	{0, 0, 0, 0, 0, 0x11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
	{0, 0, 0, 0x11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x12,},
};

/* handy dandy test function. When in doubt, you can call this from the monitor.
 * I doubt we want this long term.
 * Google 'remove before flight'.
 */
void testeip(void)
{
	int i;
	for (i = 0; i < 11; i++)
		printk("%I\n", &testvec[i]);

}
