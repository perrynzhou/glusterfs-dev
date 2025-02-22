/*
  Copyright (c) 2008-2012 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

#ifndef __RPC_TRANSPORT_H__
#define __RPC_TRANSPORT_H__

#include <inttypes.h>
#ifdef GF_SOLARIS_HOST_OS
#include <rpc/auth.h>
#else
#include <rpc/rpc.h>
#endif

#include <rpc/rpc_msg.h>

#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0
#endif /* AI_ADDRCONFIG */

/* Given the 4-byte fragment header, returns non-zero if this fragment
 * is the last fragment for the RPC record being assembled.
 * RPC Record marking standard defines a 32 bit value as the fragment
 * header with the MSB signifying whether the fragment is the last
 * fragment for the record being assembled.
 */
#define RPC_LASTFRAG(fraghdr) ((uint32_t)(fraghdr & 0x80000000U))

/* Given the 4-byte fragment header, extracts the bits that contain
 * the fragment size.
 */
#define RPC_FRAGSIZE(fraghdr) ((uint32_t)(fraghdr & 0x7fffffffU))

#define RPC_FRAGHDR_SIZE 4
#define RPC_MSGTYPE_SIZE 8

/* size of the msg from the start of call-body till and including credlen */
#define RPC_CALL_BODY_SIZE 24

#define RPC_REPLY_STATUS_SIZE 4

#define RPC_AUTH_FLAVOUR_N_LENGTH_SIZE 8

#define RPC_ACCEPT_STATUS_LEN 4

struct rpc_transport_ops;
typedef struct rpc_transport rpc_transport_t;

#include <glusterfs/dict.h>
#include <glusterfs/compat.h>
#include <glusterfs/async.h>
#include "rpcsvc-common.h"

struct peer_info {
    // OP-VERSION of clients
    uint32_t max_op_version;
    uint32_t min_op_version;
    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len;
    char identifier[UNIX_PATH_MAX];
    // Volume mounted by client
    char volname[NAME_MAX];
};
typedef struct peer_info peer_info_t;

typedef enum msg_type msg_type_t;

typedef enum {
    RPC_TRANSPORT_ACCEPT,          /* New client has been accepted */
    RPC_TRANSPORT_DISCONNECT,      /* Connection is disconnected */
    RPC_TRANSPORT_CLEANUP,         /* connection is about to be freed */
    /*RPC_TRANSPORT_READ,*/        /* An event used to enable rpcsvc to instruct
                                    * transport the number of bytes to read.
                                    * This helps in reading large msgs, wherein
                                    * the rpc actors might decide to place the
                                    * actor's payload in new iobufs separate
                                    * from the rpc header, proghdr and
                                    * authentication information. glusterfs/nfs
                                    * read and write actors are few examples
                                    * that might beniefit from this. While
                                    * reading a single msg, this event may be
                                    * delivered more than once.
                                    */
    RPC_TRANSPORT_MAP_XID_REQUEST, /* receiver of this event should send
                                    * the prognum and procnum corresponding
                                    * to xid.
                                    */
    RPC_TRANSPORT_MSG_RECEIVED,    /* Complete rpc msg has been read */
    RPC_TRANSPORT_CONNECT,         /* client is connected to server */
    RPC_TRANSPORT_MSG_SENT,
    RPC_TRANSPORT_EVENT_THREAD_DIED /* event-thread has died */
} rpc_transport_event_t;

struct rpc_transport_msg {
    struct iovec *rpchdr;
    struct iovec *proghdr;
    int rpchdrcount;
    int proghdrcount;
    struct iovec *progpayload;
    struct iobref *iobref;
    int progpayloadcount;
};
typedef struct rpc_transport_msg rpc_transport_msg_t;

struct rpc_transport_rsp {
    struct iovec *rsphdr;
    struct iovec *rsp_payload;
    int rsphdr_count;
    int rsp_payload_count;
    struct iobref *rsp_iobref;
};
typedef struct rpc_transport_rsp rpc_transport_rsp_t;

struct rpc_transport_req {
    rpc_transport_msg_t msg;
    rpc_transport_rsp_t rsp;
};
typedef struct rpc_transport_req rpc_transport_req_t;

struct rpc_transport_reply {
    void *private;
    rpc_transport_msg_t msg;
};
typedef struct rpc_transport_reply rpc_transport_reply_t;

/* FIXME: prognum, procnum and progver are already present in
 * rpc_request, hence these should be removed from request_info
 */
struct rpc_request_info {
    int prognum;
    int progver;
    void *rpc_req; /* struct rpc_req */
    rpc_transport_rsp_t rsp;
    int procnum;
    uint32_t xid;
};
typedef struct rpc_request_info rpc_request_info_t;

typedef int (*rpc_transport_notify_t)(rpc_transport_t *, void *mydata,
                                      rpc_transport_event_t, void *data, ...);

struct rpc_transport {
    struct rpc_transport_ops *ops;
    rpc_transport_t *listener; /* listener transport to which
                                * request for creation of this
                                * transport came from. valid only
                                * on server process.
                                */

    void *private;
    struct _client *xl_private;
    void *xl; /* Used for THIS */
    void *mydata;
    pthread_mutex_t lock;
    gf_atomic_t refcount;
    glusterfs_ctx_t *ctx;
    dict_t *options;
    char *name;
    void *dnscache;
    void *drc_client;
    data_t *buf;
    int32_t (*init)(rpc_transport_t *this);
    void (*fini)(rpc_transport_t *this);
    int (*reconfigure)(rpc_transport_t *this, dict_t *options);
    rpc_transport_notify_t notify;
    void *notify_data;
    peer_info_t peerinfo;
    peer_info_t myinfo;

    uint64_t total_bytes_read;
    uint64_t total_bytes_write;
    uint32_t xid; /* RPC/XID used for callbacks */
    int32_t outstanding_rpc_count;

    struct list_head list;
    void *dl_handle; /* handle of dlopen() */
    char *ssl_name;
    dict_t *clnt_options; /* store options received from
                           * client */
    gf_atomic_t disconnect_progress;
    int bind_insecure;
    int notify_poller_death;
    /* connect_failed: saves the connect() syscall status as socket_t
     * member holding connect() status can't be accessed by higher gfapi
     * layer or in client management notification handler functions
     */
    gf_boolean_t connect_failed;
    char poller_death_accept;
};

struct rpc_transport_pollin {
    struct rpc_transport *trans;
    void *private;
    struct iobref *iobref;
    struct iovec vector[MAX_IOVEC];
    gf_async_t async;
    int count;
    char is_reply;
    char vectored;
};
typedef struct rpc_transport_pollin rpc_transport_pollin_t;

struct rpc_transport_ops {
    /* no need of receive op, msg will be delivered through an event
     * notification
     */
    int32_t (*submit_request)(rpc_transport_t *this, rpc_transport_req_t *req);
    int32_t (*submit_reply)(rpc_transport_t *this,
                            rpc_transport_reply_t *reply);
    int32_t (*connect)(rpc_transport_t *this, int port);
    int32_t (*listen)(rpc_transport_t *this);
    int32_t (*disconnect)(rpc_transport_t *this, gf_boolean_t wait);
    int32_t (*get_peername)(rpc_transport_t *this, char *hostname, int hostlen);
    int32_t (*get_peeraddr)(rpc_transport_t *this, char *peeraddr, int addrlen,
                            struct sockaddr_storage *sa, socklen_t sasize);
    int32_t (*get_myname)(rpc_transport_t *this, char *hostname, int hostlen);
    int32_t (*get_myaddr)(rpc_transport_t *this, char *peeraddr, int addrlen,
                          struct sockaddr_storage *sa, socklen_t sasize);
    int32_t (*throttle)(rpc_transport_t *this, gf_boolean_t onoff);
};

int32_t
rpc_transport_count(const char *transport_type);

int32_t
rpc_transport_listen(rpc_transport_t *this);

int32_t
rpc_transport_connect(rpc_transport_t *this, int port);

int32_t
rpc_transport_disconnect(rpc_transport_t *this, gf_boolean_t wait);

int32_t
rpc_transport_notify(rpc_transport_t *this, rpc_transport_event_t event,
                     void *data, ...);

int32_t
rpc_transport_submit_request(rpc_transport_t *this, rpc_transport_req_t *req);

int32_t
rpc_transport_submit_reply(rpc_transport_t *this, rpc_transport_reply_t *reply);

rpc_transport_t *
rpc_transport_load(glusterfs_ctx_t *ctx, dict_t *options, char *name);

rpc_transport_t *
rpc_transport_ref(rpc_transport_t *trans);

int32_t
rpc_transport_unref(rpc_transport_t *trans);

int
rpc_transport_register_notify(rpc_transport_t *trans, rpc_transport_notify_t,
                              void *mydata);

int32_t
rpc_transport_get_peername(rpc_transport_t *this, char *hostname, int hostlen);

int32_t
rpc_transport_get_peeraddr(rpc_transport_t *this, char *peeraddr, int addrlen,
                           struct sockaddr_storage *sa, size_t salen);

int
rpc_transport_throttle(rpc_transport_t *this, gf_boolean_t onoff);

rpc_transport_pollin_t *
rpc_transport_pollin_alloc(rpc_transport_t *this, struct iovec *vector,
                           int count, struct iobuf *hdr_iobuf,
                           struct iobref *iobref, void *private);
void
rpc_transport_pollin_destroy(rpc_transport_pollin_t *pollin);

int
rpc_transport_keepalive_options_set(dict_t *options, int32_t interval,
                                    int32_t time, int32_t timeout);

int
rpc_transport_unix_options_build(dict_t *options, char *filepath,
                                 int frame_timeout);

int
rpc_transport_inet_options_build(dict_t *options, const char *hostname,
                                 int port, char *af);

void
rpc_transport_cleanup(rpc_transport_t *);
#endif /* __RPC_TRANSPORT_H__ */
