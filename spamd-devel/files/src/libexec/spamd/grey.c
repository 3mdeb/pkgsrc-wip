/*	$OpenBSD: grey.c,v 1.39 2007/03/18 18:38:57 beck Exp $	*/

/*
 * Copyright (c) 2004-2006 Bob Beck.  All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>

#include "grey.h"
#include "sync.h"

extern time_t passtime, greyexp, whiteexp, trapexp;
extern struct syslog_data sdata;
extern struct passwd *pw;
extern u_short cfg_port;
extern pid_t jail_pid;
extern FILE *trapcfg;
extern FILE *grey;
extern int debug;
extern int syncsend;

/* From netinet/in.h, but only _KERNEL_ gets them. */
#define satosin(sa)	((struct sockaddr_in *)(sa))
#define satosin6(sa)	((struct sockaddr_in6 *)(sa))
int server_lookup4(struct sockaddr_in *, struct sockaddr_in *,
    struct sockaddr_in *);
int server_lookup6(struct sockaddr_in6 *, struct sockaddr_in6 *,
    struct sockaddr_in6 *);

size_t whitecount, whitealloc;
size_t trapcount, trapalloc;
char **whitelist;
char **traplist;

char *traplist_name = "spamd-greytrap";
char *traplist_msg = "\"Your address %A has mailed to spamtraps here\\n\"";

pid_t db_pid = -1;
int pfdev;

struct db_change {
	SLIST_ENTRY(db_change)	entry;
	char *			key;
	void *			data;
	size_t			dsiz;
	int			act;
};

#define DBC_ADD 1
#define DBC_DEL 2

/* db pending changes list */
SLIST_HEAD(, db_change) db_changes = SLIST_HEAD_INITIALIZER(db_changes);

struct mail_addr {
	SLIST_ENTRY(mail_addr)	entry;
	char			addr[MAX_MAIL];
};

/* list of suffixes that must match TO: */
SLIST_HEAD(, mail_addr) match_suffix = SLIST_HEAD_INITIALIZER(match_suffix);
char *alloweddomains_file = PATH_SPAMD_ALLOWEDDOMAINS;

char *low_prio_mx_ip;
time_t startup;

static char *pargv[11]= {
	"pfctl", "-p", "/dev/pf", "-q", "-t",
	"spamd-white", "-T", "replace", "-f" "-", NULL
};

/* If the parent gets a signal, kill off the children and exit */
/* ARGSUSED */
static void
sig_term_chld(int sig)
{
	if (db_pid != -1)
		kill(db_pid, SIGTERM);
	if (jail_pid != -1)
		kill(jail_pid, SIGTERM);
	_exit(1);
}

/*
 * Greatly simplified version from spamd_setup.c  - only
 * sends one blacklist to an already open stream. Has no need
 * to collapse cidr ranges since these are only ever single
 * host hits.
 */
int
configure_spamd(char **addrs, int count, FILE *sdc)
{
	int i;

	fprintf(sdc, "%s;%s;", traplist_name, traplist_msg);
	for (i = 0; i < count; i++)
		fprintf(sdc, "%s/32;", addrs[i]);
	fprintf(sdc, "\n");
	if (fflush(sdc) == EOF)
		syslog_r(LOG_DEBUG, &sdata, "configure_spamd: fflush failed (%m)");
	return(0);
}


/* Stolen from ftp-proxy */
int
server_lookup(struct sockaddr *client, struct sockaddr *proxy,
    struct sockaddr *server)
{
	if (client->sa_family == AF_INET)
		return (server_lookup4(satosin(client), satosin(proxy),
		    satosin(server)));
	
	if (client->sa_family == AF_INET6)
		return (server_lookup6(satosin6(client), satosin6(proxy),
		    satosin6(server)));

	errno = EPROTONOSUPPORT;
	return (-1);
}

int
server_lookup4(struct sockaddr_in *client, struct sockaddr_in *proxy,
    struct sockaddr_in *server)
{
	struct pfioc_natlook pnl;

	memset(&pnl, 0, sizeof pnl);
	pnl.direction = PF_OUT;
	pnl.af = AF_INET;
	pnl.proto = IPPROTO_TCP;
	memcpy(&pnl.saddr.v4, &client->sin_addr.s_addr, sizeof pnl.saddr.v4);
	memcpy(&pnl.daddr.v4, &proxy->sin_addr.s_addr, sizeof pnl.daddr.v4);
	pnl.sport = client->sin_port;
	pnl.dport = proxy->sin_port;
	
	if (ioctl(pfdev, DIOCNATLOOK, &pnl) == -1)
		return (-1);

	memset(server, 0, sizeof(struct sockaddr_in));
	server->sin_len = sizeof(struct sockaddr_in);
	server->sin_family = AF_INET;
	memcpy(&server->sin_addr.s_addr, &pnl.rdaddr.v4,
	    sizeof server->sin_addr.s_addr);
	server->sin_port = pnl.rdport;
		
	return (0);
}

int
server_lookup6(struct sockaddr_in6 *client, struct sockaddr_in6 *proxy,
    struct sockaddr_in6 *server)
{
	struct pfioc_natlook pnl;

	memset(&pnl, 0, sizeof pnl);
	pnl.direction = PF_OUT;
	pnl.af = AF_INET6;
	pnl.proto = IPPROTO_TCP;
	memcpy(&pnl.saddr.v6, &client->sin6_addr.s6_addr, sizeof pnl.saddr.v6);
	memcpy(&pnl.daddr.v6, &proxy->sin6_addr.s6_addr, sizeof pnl.daddr.v6);
	pnl.sport = client->sin6_port;
	pnl.dport = proxy->sin6_port;
	
	if (ioctl(pfdev, DIOCNATLOOK, &pnl) == -1)
		return (-1);

	memset(server, 0, sizeof(struct sockaddr_in6));
	server->sin6_len = sizeof(struct sockaddr_in6);
	server->sin6_family = AF_INET6;
	memcpy(&server->sin6_addr.s6_addr, &pnl.rdaddr.v6,
	    sizeof server->sin6_addr);
	server->sin6_port = pnl.rdport;

	return (0);
}

int
configure_pf(char **addrs, int count)
{
	FILE *pf = NULL;
	int i, pdes[2], status;
	pid_t pid;
	char *fdpath;
	struct sigaction sa;

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sig_term_chld;

	if (debug)
		fprintf(stderr, "configure_pf - device on fd %d\n", pfdev);
	if (pfdev < 1 || pfdev > 63)
		return(-1);
	if (asprintf(&fdpath, "/dev/fd/%d", pfdev) == -1)
		return(-1);
	pargv[2] = fdpath;
	if (pipe(pdes) != 0) {
		syslog_r(LOG_INFO, &sdata, "pipe failed (%m)");
		free(fdpath);
		fdpath = NULL;
		return(-1);
	}
	signal(SIGCHLD, SIG_DFL);
	switch (pid = fork()) {
	case -1:
		syslog_r(LOG_INFO, &sdata, "fork failed (%m)");
		free(fdpath);
		fdpath = NULL;
		close(pdes[0]);
		close(pdes[1]);
		sigaction(SIGCHLD, &sa, NULL);
		return(-1);
	case 0:
		/* child */
		close(pdes[1]);
		if (pdes[0] != STDIN_FILENO) {
			dup2(pdes[0], STDIN_FILENO);
			close(pdes[0]);
		}
		execvp(PATH_PFCTL, pargv);
		syslog_r(LOG_ERR, &sdata, "can't exec %s:%m", PATH_PFCTL);
		_exit(1);
	}

	/* parent */
	free(fdpath);
	fdpath = NULL;
	close(pdes[0]);
	pf = fdopen(pdes[1], "w");
	if (pf == NULL) {
		syslog_r(LOG_INFO, &sdata, "fdopen failed (%m)");
		close(pdes[1]);
		sigaction(SIGCHLD, &sa, NULL);
		return(-1);
	}
	for (i = 0; i < count; i++)
		if (addrs[i] != NULL)
			fprintf(pf, "%s/32\n", addrs[i]);
	fclose(pf);

	waitpid(pid, &status, 0);
	if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
		syslog_r(LOG_ERR, &sdata, "%s returned status %d", PATH_PFCTL,
		    WEXITSTATUS(status));
	else if (WIFSIGNALED(status))
		syslog_r(LOG_ERR, &sdata, "%s died on signal %d", PATH_PFCTL,
		    WTERMSIG(status));

	sigaction(SIGCHLD, &sa, NULL);
	return(0);
}

char *
dequotetolower(const char *addr)
{
	static char buf[MAX_MAIL];
	char *cp;

	if (*addr == '<');
		addr++;
	(void) strlcpy(buf, addr, sizeof(buf));
	cp = strrchr(buf, '>');
	if (cp != NULL && cp[1] == '\0')
		*cp = '\0';
	cp = buf;
	while (*cp != '\0') {
		*cp = tolower(*cp);
		cp++;
	}
	return(buf);
}

void
readsuffixlists(void)
{
	FILE *fp;
	char *buf;
	size_t len;
	struct mail_addr *m;

	while (!SLIST_EMPTY(&match_suffix))
		SLIST_REMOVE_HEAD(&match_suffix, entry);
	if ((fp = fopen(alloweddomains_file, "r")) != NULL) {
		while ((buf = fgetln(fp, &len))) {
			if (buf[len-1] == '\n')
				len--;
			if ((m = malloc(sizeof(struct mail_addr))) == NULL)
				goto bad;
			if ((len + 1) > sizeof(m->addr)) {
				syslog_r(LOG_ERR, &sdata,
				    "line too long in %s - file ignored",
				    alloweddomains_file);
				goto bad;
			}
			memcpy(m->addr, buf, len);
			m->addr[len]='\0';
			syslog_r(LOG_ERR, &sdata, "got suffix %s", m->addr);
			SLIST_INSERT_HEAD(&match_suffix, m, entry);
		}
	}
	return;
bad:
	while (!SLIST_EMPTY(&match_suffix))
		SLIST_REMOVE_HEAD(&match_suffix, entry);
}

void
freeaddrlists(void)
{
	int i;

	if (whitelist != NULL)
		for (i = 0; i < whitecount; i++) {
			free(whitelist[i]);
			whitelist[i] = NULL;
		}
	whitecount = 0;
	if (traplist != NULL) {
		for (i = 0; i < trapcount; i++) {
			free(traplist[i]);
			traplist[i] = NULL;
		}
	}
	trapcount = 0;
}

/* validate, then add to list of addrs to whitelist */
int
addwhiteaddr(char *addr)
{
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;		/*for now*/
	hints.ai_socktype = SOCK_DGRAM;		/*dummy*/
	hints.ai_protocol = IPPROTO_UDP;	/*dummy*/
	hints.ai_flags = AI_NUMERICHOST;

	if (getaddrinfo(addr, NULL, &hints, &res) == 0) {
		if (whitecount == whitealloc) {
			char **tmp;

			tmp = realloc(whitelist,
			    (whitealloc + 1024) * sizeof(char *));
			if (tmp == NULL) {
				freeaddrinfo(res);
				return(-1);
			}
			whitelist = tmp;
			whitealloc += 1024;
		}
		whitelist[whitecount] = strdup(addr);
		if (whitelist[whitecount] == NULL) {
			freeaddrinfo(res);
			return(-1);
		}
		whitecount++;
		freeaddrinfo(res);
	} else
		return(-1);
	return(0);
}

/* validate, then add to list of addrs to traplist */
int
addtrapaddr(char *addr)
{
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;		/*for now*/
	hints.ai_socktype = SOCK_DGRAM;		/*dummy*/
	hints.ai_protocol = IPPROTO_UDP;	/*dummy*/
	hints.ai_flags = AI_NUMERICHOST;

	if (getaddrinfo(addr, NULL, &hints, &res) == 0) {
		if (trapcount == trapalloc) {
			char **tmp;

			tmp = realloc(traplist,
			    (trapalloc + 1024) * sizeof(char *));
			if (tmp == NULL) {
				freeaddrinfo(res);
				return(-1);
			}
			traplist = tmp;
			trapalloc += 1024;
		}
		traplist[trapcount] = strdup(addr);
		if (traplist[trapcount] == NULL) {
			freeaddrinfo(res);
			return(-1);
		}
		trapcount++;
		freeaddrinfo(res);
	} else
		return(-1);
	return(0);
}

static int
queue_change(char *key, char *data, size_t dsiz, int act)
{
	struct db_change *dbc;

	if ((dbc = malloc(sizeof(*dbc))) == NULL) {
		syslog_r(LOG_DEBUG, &sdata, "malloc failed (queue change)");
		return(-1);
	}
	if ((dbc->key = strdup(key)) == NULL) {
		syslog_r(LOG_DEBUG, &sdata, "malloc failed (queue change)");
		free(dbc);
		return(-1);
	}
	if ((dbc->data = malloc(dsiz)) == NULL) {
		syslog_r(LOG_DEBUG, &sdata, "malloc failed (queue change)");
		free(dbc->key);
		free(dbc);
		return(-1);
	}
	memcpy(dbc->data, data, dsiz);
	dbc->dsiz = dsiz;
	dbc->act = act;
	syslog_r(LOG_DEBUG, &sdata,
	    "queueing %s of %s", ((act == DBC_ADD) ? "add" : "deletion"),
	    dbc->key);
	SLIST_INSERT_HEAD(&db_changes, dbc, entry);
	return(0);
}

static int
do_changes(DB *db)
{
	DBT			dbk, dbd;
	struct db_change	*dbc;
	int ret = 0;

	while (!SLIST_EMPTY(&db_changes)) {
		dbc = SLIST_FIRST(&db_changes);
		switch (dbc->act) {
		case DBC_ADD:
			memset(&dbk, 0, sizeof(dbk));
			dbk.size = strlen(dbc->key);
			dbk.data = dbc->key;
			memset(&dbd, 0, sizeof(dbd));
			dbd.size = dbc->dsiz;
			dbd.data = dbc->data;
			if (db->put(db, &dbk, &dbd, 0)) {
				db->sync(db, 0);
				syslog_r(LOG_ERR, &sdata,
				    "can't add %s to spamd db (%m)", dbc->key);
				ret = -1;
			}
			db->sync(db, 0);
			break;
		case DBC_DEL:
			memset(&dbk, 0, sizeof(dbk));
			dbk.size = strlen(dbc->key);
			dbk.data = dbc->key;
			if (db->del(db, &dbk, 0)) {
				syslog_r(LOG_ERR, &sdata,
				    "can't delete %s from spamd db (%m)",
				    dbc->key);
				ret = -1;
			}
			break;
		default:
			syslog_r(LOG_ERR, &sdata, "Unrecognized db change");
			ret = -1;
		}
		free(dbc->key);
		dbc->key = NULL;
		free(dbc->data);
		dbc->data = NULL;
		dbc->act = 0;
		dbc->dsiz = 0;
		SLIST_REMOVE_HEAD(&db_changes, entry);

	}
	return(ret);
}

int
db_notin(DB *db, char *key)
{
	int			i;
	DBT			dbk, dbd;

	memset(&dbk, 0, sizeof(dbk));
	dbk.size = strlen(key);
	dbk.data = key;
	memset(&dbd, 0,
 sizeof(dbd));
	i = db->get(db, &dbk, &dbd, 0);
	if (i == -1)
		return (-1);
	if (i)
		/* not in the database */
		return (1);
	else
		/* it is in the database */
		return (0);
}


int
greyscan(char *dbname)
{
	HASHINFO	hashinfo;
	DBT		dbk, dbd;
	DB		*db;
	struct gdata	gd;
	int		r;
	char		*a = NULL;
	size_t		asiz = 0;
	time_t now = time(NULL);

	/* walk db, expire, and whitelist */
	memset(&hashinfo, 0, sizeof(hashinfo));
	db = dbopen(dbname, O_EXLOCK|O_RDWR, 0600, DB_HASH, &hashinfo);
	if (db == NULL) {
		syslog_r(LOG_INFO, &sdata, "dbopen failed (%m)");
		return(-1);
	}
	memset(&dbk, 0, sizeof(dbk));
	memset(&dbd, 0, sizeof(dbd));
	for (r = db->seq(db, &dbk, &dbd, R_FIRST); !r;
	    r = db->seq(db, &dbk, &dbd, R_NEXT)) {
		if ((dbk.size < 1) || dbd.size != sizeof(struct gdata)) {
			syslog_r(LOG_ERR, &sdata, "bogus entry in spamd database");
			goto bad;
		}
		if (asiz < dbk.size + 1) {
			char *tmp;

			tmp = realloc(a, dbk.size * 2);
			if (tmp == NULL)
				goto bad;
			a = tmp;
			asiz = dbk.size * 2;
		}
		memset(a, 0, asiz);
		memcpy(a, dbk.data, dbk.size);
		memcpy(&gd, dbd.data, sizeof(gd));
		if (gd.expire <= now && gd.pcount != -2) {
			/* get rid of entry */
			if (queue_change(a, NULL, 0, DBC_DEL) == -1)
				goto bad;
		} else if (gd.pcount == -1)  {
			/* this is a greytrap hit */
			if ((addtrapaddr(a) == -1) &&
			    (queue_change(a, NULL, 0, DBC_DEL) == -1))
				goto bad;
		} else if (gd.pcount >= 0 && gd.pass <= now) {
			int tuple = 0;
			char *cp;

			/*
			 * add address to whitelist
			 * add an address-keyed entry to db
			 */
			cp = strchr(a, '\n');
			if (cp != NULL) {
				tuple = 1;
				*cp = '\0';
			}

			if (addwhiteaddr(a) == -1) {
				if (cp != NULL)
					*cp = '\n';
				if (queue_change(a, NULL, 0, DBC_DEL) == -1)
					goto bad;
			}

			if (tuple && db_notin(db, a)) {
				if (cp != NULL)
					*cp = '\0';
				/* re-add entry, keyed only by ip */
				gd.expire = now + whiteexp;
				dbd.size = sizeof(gd);
				dbd.data = &gd;
				if (queue_change(a, (void *) &gd, sizeof(gd),
				    DBC_ADD) == -1)
					goto bad;
				syslog_r(LOG_DEBUG, &sdata,
				    "whitelisting %s in %s", a, dbname);
			}
			if (debug)
				fprintf(stderr, "whitelisted %s\n", a);
		}
	}
	(void) do_changes(db);
	db->close(db);
	db = NULL;
	configure_pf(whitelist, whitecount);
	if (configure_spamd(traplist, trapcount, trapcfg) == -1)
		syslog_r(LOG_DEBUG, &sdata, "configure_spamd failed");

	freeaddrlists();
	free(a);
	a = NULL;
	asiz = 0;
	return(0);
 bad:
	(void) do_changes(db);
	db->close(db);
	db = NULL;
	freeaddrlists();
	free(a);
	a = NULL;
	asiz = 0;
	return(-1);
}

int
trapcheck(DB *db, char *to)
{
	int			i, j, smatch = 0;
	DBT			dbk, dbd;
	struct mail_addr	*m;
	char *			trap;
	size_t			s;

	trap = dequotetolower(to);
	if (!SLIST_EMPTY(&match_suffix)) {
		s = strlen(trap);
		SLIST_FOREACH(m, &match_suffix, entry) {
			j = s - strlen(m->addr);
			if ((j >= 0) && (strcasecmp(trap+j, m->addr) == 0))
				smatch = 1;
		}
		if (!smatch)
			/* no suffixes match, so trap it */
			return (0);
	}
	memset(&dbk, 0, sizeof(dbk));
	dbk.size = strlen(trap);
	dbk.data = trap;
	memset(&dbd, 0, sizeof(dbd));
	i = db->get(db, &dbk, &dbd, 0);
	if (i == -1)
		return (-1);
	if (i)
		/* didn't exist - so this doesn't match a known spamtrap  */
		return (1);
	else
		/* To: address is a spamtrap, so add as a greytrap entry */
		return (0);
}

int
twupdate(char *dbname, char *what, char *ip, char *source, char *expires)
{
	/* we got a TRAP or WHITE update from someone else */
	HASHINFO	hashinfo;
	DBT		dbk, dbd;
	DB		*db;
	struct gdata	gd;
	time_t		now, expire;
	int		r, spamtrap;

	now = time(NULL);
	/* expiry times have to be in the future */
	expire = strtonum(expires, now, UINT_MAX, NULL);
	if (expire == 0)
		return(-1);

	if (strcmp(what, "TRAP") == 0)
		spamtrap = 1;
	else if (strcmp(what, "WHITE") == 0)
		spamtrap = 0;
	else
		return(-1);

	memset(&hashinfo, 0, sizeof(hashinfo));
	db = dbopen(dbname, O_EXLOCK|O_RDWR, 0600, DB_HASH, &hashinfo);
	if (db == NULL)
		return(-1);

	memset(&dbk, 0, sizeof(dbk));
	dbk.size = strlen(ip);
	dbk.data = ip;
	memset(&dbd, 0, sizeof(dbd));
	r = db->get(db, &dbk, &dbd, 0);
	if (r == -1)
		goto bad;
	if (r) {
		/* new entry */
		memset(&gd, 0, sizeof(gd));
		gd.first = now;
		gd.pcount = spamtrap ? -1 : 0;
		gd.expire = expire;
		memset(&dbk, 0, sizeof(dbk));
		dbk.size = strlen(ip);
		dbk.data = ip;
		memset(&dbd, 0, sizeof(dbd));
		dbd.size = sizeof(gd);
		dbd.data = &gd;
		r = db->put(db, &dbk, &dbd, 0);
		db->sync(db, 0);
		if (r)
			goto bad;
		if (debug)
			fprintf(stderr, "added %s %s\n",
			    spamtrap ? "trap entry for" : "", ip);
		syslog_r(LOG_DEBUG, &sdata,
		    "New %s from %s for %s, expires %s", what, source, ip,
		    expires);
	} else {
		/* existing entry */
		if (dbd.size != sizeof(gd)) {
			/* whatever this is, it doesn't belong */
			db->del(db, &dbk, 0);
			db->sync(db, 0);
			goto bad;
		}
		memcpy(&gd, dbd.data, sizeof(gd));
		if (spamtrap) {
			gd.pcount = -1;
			gd.bcount++;
		} else
			gd.pcount++;
		memset(&dbk, 0, sizeof(dbk));
		dbk.size = strlen(ip);
		dbk.data = ip;
		memset(&dbd, 0, sizeof(dbd));
		dbd.size = sizeof(gd);
		dbd.data = &gd;
		r = db->put(db, &dbk, &dbd, 0);
		db->sync(db, 0);
		if (r)
			goto bad;
		if (debug)
			fprintf(stderr, "updated %s\n", ip);
	}
	db->close(db);
	return(0);
 bad:
	db->close(db);
	return(-1);

}

int
greyupdate(char *dbname, char *helo, char *ip, char *from, char *to, int sync,
    char *cip)
{
	HASHINFO	hashinfo;
	DBT		dbk, dbd;
	DB		*db;
	char		*key = NULL;
	char		*lookup;
	struct gdata	gd;
	time_t		now, expire;
	int		r, spamtrap;

	now = time(NULL);

	/* open with lock, find record, update, close, unlock */
	memset(&hashinfo, 0, sizeof(hashinfo));
	db = dbopen(dbname, O_EXLOCK|O_RDWR, 0600, DB_HASH, &hashinfo);
	if (db == NULL)
		return(-1);
	if (asprintf(&key, "%s\n%s\n%s\n%s", ip, helo, from, to) == -1)
		goto bad;
	r = trapcheck(db, to);
	switch (r) {
	case 1:
		/* do not trap */
		spamtrap = 0;
		lookup = key;
		expire = greyexp;
		break;
	case 0:
		/* trap */
		spamtrap = 1;
		lookup = ip;
		expire = trapexp;
		syslog_r(LOG_DEBUG, &sdata, "Trapping %s for tuple %s", ip,
		    key);
		break;
	default:
		goto bad;
		break;
	}
	memset(&dbk, 0, sizeof(dbk));
	dbk.size = strlen(lookup);
	dbk.data = lookup;
	memset(&dbd, 0, sizeof(dbd));
	r = db->get(db, &dbk, &dbd, 0);
	if (r == -1)
		goto bad;
	if (r) {
		/* new entry */
		if (sync &&  low_prio_mx_ip &&
		    (strcmp(cip, low_prio_mx_ip) == 0) &&
		    ((startup + 60)  < now)) {
			/* we haven't seen a greylist entry for this tuple, 
			 * and yet the connection was to a low priority MX
			 * which we know can't be hit first if the client
			 * is adhering to the RFC's - soo.. kill it!
			 */
			spamtrap = 1;
			lookup = ip;
			expire = trapexp;
			syslog_r(LOG_DEBUG, &sdata,
			    "Trapping %s for trying %s first for tuple %s",
			    ip, low_prio_mx_ip, key);
		}
		memset(&gd, 0, sizeof(gd));
		gd.first = now;
		gd.bcount = 1;
		gd.pcount = spamtrap ? -1 : 0;
		gd.pass = now + expire;
		gd.expire = now + expire;
		memset(&dbk, 0, sizeof(dbk));
		dbk.size = strlen(lookup);
		dbk.data = lookup;
		memset(&dbd, 0, sizeof(dbd));
		dbd.size = sizeof(gd);
		dbd.data = &gd;
		r = db->put(db, &dbk, &dbd, 0);
		db->sync(db, 0);
		if (r)
			goto bad;
		if (debug)
			fprintf(stderr, "added %s %s\n",
			    spamtrap ? "greytrap entry for" : "", lookup);
	} else {
		/* existing entry */
		if (dbd.size != sizeof(gd)) {
			/* whatever this is, it doesn't belong */
			db->del(db, &dbk, 0);
			db->sync(db, 0);
			goto bad;
		}
		memcpy(&gd, dbd.data, sizeof(gd));
		gd.bcount++;
		gd.pcount = spamtrap ? -1 : 0;
		if (gd.first + passtime < now)
			gd.pass = now;
		memset(&dbk, 0, sizeof(dbk));
		dbk.size = strlen(lookup);
		dbk.data = lookup;
		memset(&dbd, 0, sizeof(dbd));
		dbd.size = sizeof(gd);
		dbd.data = &gd;
		r = db->put(db, &dbk, &dbd, 0);
		db->sync(db, 0);
		if (r)
			goto bad;
		if (debug)
			fprintf(stderr, "updated %s\n", lookup);
	}
	free(key);
	key = NULL;
	db->close(db);
	db = NULL;

	/* Entry successfully update, sent out sync message */
	if (syncsend && sync) {
		if (spamtrap) {
			syslog_r(LOG_DEBUG, &sdata,
			    "sync_trap %s", ip);
			sync_trapped(now, now + expire, ip);
		}
		else
			sync_update(now, helo, ip, from, to);
	}
	return(0);
 bad:
	free(key);
	key = NULL;
	db->close(db);
	db = NULL;
	return(-1);
}

int
twread(char *buf)
{
	if ((strncmp(buf, "WHITE:", 6) == 0) ||
	    (strncmp(buf, "TRAP:", 5) == 0)) {
		char **ap, *argv[5];
		int argc = 0;

		for (ap = argv;
		    ap < &argv[4] && (*ap = strsep(&buf, ":")) != NULL;) {
			if (**ap != '\0')
				ap++;
			argc++;
		}
		*ap = NULL;
		if (argc != 4)
			return (-1);
		twupdate(PATH_SPAMD_DB, argv[0], argv[1], argv[2], argv[3]);
		return (0);
	} else
		return (-1);
}

int
greyreader(void)
{
	char cip[32], ip[32], helo[MAX_MAIL], from[MAX_MAIL], to[MAX_MAIL];
	char *buf;
	size_t len;
	int state, sync;
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;		/*for now*/
	hints.ai_socktype = SOCK_DGRAM;		/*dummy*/
	hints.ai_protocol = IPPROTO_UDP;	/*dummy*/
	hints.ai_flags = AI_NUMERICHOST;

	state = 0;
	sync = 1;
	if (grey == NULL) {
		syslog_r(LOG_ERR, &sdata, "No greylist pipe stream!\n");
		exit(1);
	}

	/* grab trap suffixes */
	readsuffixlists();

	while ((buf = fgetln(grey, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else
			/* all valid lines end in \n */
			continue;
		if (strlen(buf) < 4)
			continue;

		if (strcmp(buf, "SYNC") == 0) {
			sync = 0;
			continue;
		}

		switch (state) {
		case 0:
			if (twread(buf) == 0) {
				state = 0;
				break;
			}
			if (strncmp(buf, "HE:", 3) != 0) {
				if (strncmp(buf, "CO:", 3) == 0)
					strlcpy(cip, buf+3, sizeof(cip));
				state = 0;
				break;
			}
			strlcpy(helo, buf+3, sizeof(helo));
			state = 1;
			break;
		case 1:
			if (strncmp(buf, "IP:", 3) != 0)
				break;
			strlcpy(ip, buf+3, sizeof(ip));
			if (getaddrinfo(ip, NULL, &hints, &res) == 0) {
				freeaddrinfo(res);
				state = 2;
			} else
				state = 0;
			break;
		case 2:
			if (strncmp(buf, "FR:", 3) != 0) {
				state = 0;
				break;
			}
			strlcpy(from, buf+3, sizeof(from));
			state = 3;
			break;
		case 3:
			if (strncmp(buf, "TO:", 3) != 0) {
				state = 0;
				break;
			}
			strlcpy(to, buf+3, sizeof(to));
			if (debug)
				fprintf(stderr,
				    "Got Grey HELO %s, IP %s from %s to %s\n",
				    helo, ip, from, to);
			greyupdate(PATH_SPAMD_DB, helo, ip, from, to, sync, cip);
			sync = 1;
			state = 0;
			break;
		}
	}
	return (0);
}

void
greyscanner(void)
{
	for (;;) {
		if (greyscan(PATH_SPAMD_DB) == -1)
			syslog_r(LOG_NOTICE, &sdata, "scan of %s failed",
			    PATH_SPAMD_DB);
		sleep(DB_SCAN_INTERVAL);
	}
	/* NOTREACHED */
}

static void
drop_privs(void)
{
	/*
	 * lose root, continue as non-root user
	 */
	if (pw) {
		setgroups(1, &pw->pw_gid);
		setegid(pw->pw_gid);
		setgid(pw->pw_gid);
		seteuid(pw->pw_uid);
		setuid(pw->pw_uid);
	}
}

static void
convert_spamd_db(void)
{
	char		sfn[] = "/var/db/spamd.XXXXXXXXX";
	int		r, fd = -1;
	DB		*db1, *db2;
	BTREEINFO	btreeinfo;
	HASHINFO	hashinfo;
	DBT		dbk, dbd;

	/* try to open the db as a BTREE */
	memset(&btreeinfo, 0, sizeof(btreeinfo));
	db1 = dbopen(PATH_SPAMD_DB, O_EXLOCK|O_RDWR, 0600, DB_BTREE,
	    &btreeinfo);
	if (db1 == NULL) {
		syslog_r(LOG_ERR, &sdata,
		    "corrupt db in %s, remove and restart", PATH_SPAMD_DB);
		exit(1);
	}

	if ((fd = mkstemp(sfn)) == -1) {
		syslog_r(LOG_ERR, &sdata,
		    "can't convert %s: mkstemp failed (%m)", PATH_SPAMD_DB);
		exit(1);
	}
	memset(&hashinfo, 0, sizeof(hashinfo));
	db2 = dbopen(sfn, O_EXLOCK|O_RDWR, 0600, DB_HASH, &hashinfo);
	if (db2 == NULL) {
		unlink(sfn);
		syslog_r(LOG_ERR, &sdata,
		    "can't convert %s:  can't dbopen %s (%m)", PATH_SPAMD_DB,
		sfn);
		db1->close(db1);
		exit(1);
	}

	memset(&dbk, 0, sizeof(dbk));
	memset(&dbd, 0, sizeof(dbd));
		for (r = db1->seq(db1, &dbk, &dbd, R_FIRST); !r;
		    r = db1->seq(db1, &dbk, &dbd, R_NEXT)) {
			if (db2->put(db2, &dbk, &dbd, 0)) {
				db2->sync(db2, 0);
				db2->close(db2);
				db1->close(db1);
				unlink(sfn);
				syslog_r(LOG_ERR, &sdata,
				    "can't convert %s - remove and restart",
				    PATH_SPAMD_DB);
				exit(1);
			}
		}
	db2->sync(db2, 0);
	db2->close(db2);
	db1->sync(db1, 0);
	db1->close(db1);
	rename(sfn, PATH_SPAMD_DB);
	close(fd);
	/* if we are dropping privs, chown to that user */
	if (pw && (chown(PATH_SPAMD_DB, pw->pw_uid, pw->pw_gid) == -1)) {
		syslog_r(LOG_ERR, &sdata,
		    "chown %s failed (%m)", PATH_SPAMD_DB);
		exit(1);
	}
}

static void
check_spamd_db(void)
{
	HASHINFO hashinfo;
	int i = -1;
	DB *db;

	/* check to see if /var/db/spamd exists, if not, create it */
	memset(&hashinfo, 0, sizeof(hashinfo));
	db = dbopen(PATH_SPAMD_DB, O_EXLOCK|O_RDWR, 0600, DB_HASH, &hashinfo);

	if (db == NULL) {
		switch (errno) {
		case ENOENT:
			i = open(PATH_SPAMD_DB, O_RDWR|O_CREAT, 0644);
			if (i == -1) {
				syslog_r(LOG_ERR, &sdata,
				    "create %s failed (%m)", PATH_SPAMD_DB);
				exit(1);
			}
			/* if we are dropping privs, chown to that user */
			if (pw && (fchown(i, pw->pw_uid, pw->pw_gid) == -1)) {
				syslog_r(LOG_ERR, &sdata,
				    "chown %s failed (%m)", PATH_SPAMD_DB);
				exit(1);
			}
			close(i);
			drop_privs();
			return;
			break;
		case EFTYPE:
			/*
			 * db may be old BTREE instead of HASH, attempt to
			 * convert.
			 */
			convert_spamd_db();
			drop_privs();
			return;
			break;
		default:
			syslog_r(LOG_ERR, &sdata, "open of %s failed (%m)",
			    PATH_SPAMD_DB);
			exit(1);
		}
	}
	db->sync(db, 0);
	db->close(db);
	drop_privs();
}


int
greywatcher(void)
{
	struct sigaction sa;

	check_spamd_db();

	startup = time(NULL);
	db_pid = fork();
	switch (db_pid) {
	case -1:
		syslog_r(LOG_ERR, &sdata, "fork failed (%m)");
		exit(1);
	case 0:
		/*
		 * child, talks to jailed spamd over greypipe,
		 * updates db. has no access to pf.
		 */
		close(pfdev);
		setproctitle("(%s update)", PATH_SPAMD_DB);
		greyreader();
		/* NOTREACHED */
		_exit(1);
	}


	fclose(grey);
	/*
	 * parent, scans db periodically for changes and updates
	 * pf whitelist table accordingly.
	 */

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sig_term_chld;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	setproctitle("(pf <spamd-white> update)");
	greyscanner();
	/* NOTREACHED */
	exit(1);
}
