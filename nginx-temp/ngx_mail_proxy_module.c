
/*
 * Copyright (C) Igor Sysoev
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_event_connect.h>
#include <ngx_mail.h>


typedef struct {
    ngx_flag_t  enable;
    ngx_flag_t  pass_error_message;
    ngx_flag_t  xclient;
    size_t      buffer_size;
    ngx_msec_t  timeout;
} ngx_mail_proxy_conf_t;

static void ngx_mail_proxy_block_read(ngx_event_t *rev);
static void ngx_mail_proxy_pop3_handler(ngx_event_t *rev);
static void ngx_mail_proxy_imap_handler(ngx_event_t *rev);
static void ngx_mail_proxy_smtp_handler(ngx_event_t *rev);
static void ngx_mail_proxy_dummy_handler(ngx_event_t *ev);
static ngx_int_t ngx_mail_proxy_read_response(ngx_mail_session_t *s,
    ngx_uint_t state);
static void ngx_mail_proxy_handler(ngx_event_t *ev);
static void ngx_mail_proxy_upstream_error(ngx_mail_session_t *s);
static void ngx_mail_proxy_close_session(ngx_mail_session_t *s);
static void *ngx_mail_proxy_create_conf(ngx_conf_t *cf);
static char *ngx_mail_proxy_merge_conf(ngx_conf_t *cf, void *parent,
    void *child);


static ngx_command_t  ngx_mail_proxy_commands[] = {

    { ngx_string("proxy"),
      NGX_MAIL_MAIN_CONF|NGX_MAIL_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_MAIL_SRV_CONF_OFFSET,
      offsetof(ngx_mail_proxy_conf_t, enable),
      NULL },

    { ngx_string("proxy_buffer"),
      NGX_MAIL_MAIN_CONF|NGX_MAIL_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_MAIL_SRV_CONF_OFFSET,
      offsetof(ngx_mail_proxy_conf_t, buffer_size),
      NULL },

    { ngx_string("proxy_timeout"),
      NGX_MAIL_MAIN_CONF|NGX_MAIL_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_MAIL_SRV_CONF_OFFSET,
      offsetof(ngx_mail_proxy_conf_t, timeout),
      NULL },

    { ngx_string("proxy_pass_error_message"),
      NGX_MAIL_MAIN_CONF|NGX_MAIL_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_MAIL_SRV_CONF_OFFSET,
      offsetof(ngx_mail_proxy_conf_t, pass_error_message),
      NULL },

    { ngx_string("xclient"),
      NGX_MAIL_MAIN_CONF|NGX_MAIL_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_MAIL_SRV_CONF_OFFSET,
      offsetof(ngx_mail_proxy_conf_t, xclient),
      NULL },
      
      ngx_null_command
};


static ngx_mail_module_t  ngx_mail_proxy_module_ctx = {
    NULL,                                  /* protocol */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_mail_proxy_create_conf,            /* create server configuration */
    ngx_mail_proxy_merge_conf              /* merge server configuration */
};


ngx_module_t  ngx_mail_proxy_module = {
    NGX_MODULE_V1,
    &ngx_mail_proxy_module_ctx,            /* module context */
    ngx_mail_proxy_commands,               /* module directives */
    NGX_MAIL_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static u_char  smtp_auth_ok[] = "235 2.0.0 OK" CRLF;

void
ngx_mail_proxy_init(ngx_mail_session_t *s, ngx_addr_t *peer)
{
    int                        keepalive;
    ngx_mail_proxy_ctx_t      *p;
    ngx_mail_core_srv_conf_t  *cscf;

    s->connection->log->action = "initializing before connecting to upstream";

    cscf = ngx_mail_get_module_srv_conf(s, ngx_mail_core_module);

    if (cscf->so_keepalive) {
        keepalive = 1;

        if (setsockopt(s->connection->fd, SOL_SOCKET, SO_KEEPALIVE,
                       (const void *) &keepalive, sizeof(int))
                == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, s->connection->log, ngx_socket_errno,
                          "setsockopt(SO_KEEPALIVE) failed");
        }
    }
#if (NGX_MAIL_SSL)
    if (s->con_proto) {
        ngx_mail_ssl_conf_t *sslcf;

        sslcf = ngx_mail_get_module_srv_conf(s, ngx_mail_ssl_module);

        if (sslcf->ssl.ctx == NULL)
            ngx_log_error(NGX_LOG_WARN, s->connection->log, 0,
                "no \"ssl_certificate\" is defined in mail section " \
                "of config, while auth_http asked to use secure" \
                "connection to backend;");
    }
#endif

    p = ngx_pcalloc(s->connection->pool, sizeof(ngx_mail_proxy_ctx_t));
    if (p == NULL) {
        ngx_mail_session_internal_server_error(s);
        return;
    }
 
    s->proxy = p;

    p->upstream.socklen = peer->socklen;
    p->upstream.name = &peer->name;
    p->upstream.get = ngx_event_get_peer;
    p->upstream.log = s->connection->log;
    p->upstream.log_error = NGX_ERROR_ERR;
}

int
ngx_mail_proxy_postinit(ngx_mail_session_t *s, ngx_addr_t *peer, ngx_resolver_ctx_t *ctx)
{
    ngx_int_t rc;
    ngx_mail_proxy_ctx_t *p = s->proxy;
    ngx_mail_proxy_conf_t *pcf;
    ngx_mail_core_srv_conf_t *cscf;

    s->connection->log->action = "connecting to upstream";
    
    cscf = ngx_mail_get_module_srv_conf(s, ngx_mail_core_module);

    p->upstream.sockaddr = peer->sockaddr;
   
    if (!ctx)
	rc = ngx_event_connect_peer(&p->upstream, NULL);
    else {
	struct timeval *timeout;
	timeout = ngx_pcalloc(s->connection->pool, sizeof (struct timeval));
	if (timeout == NULL) {
	    ngx_mail_proxy_internal_server_error(s);
	    return 0;
	}
	//ngx_memzero(&timeout->tv_sec, sizeof(timeout->tv_sec));
	timeout->tv_sec = cscf->timeout / ctx->naddrs / 1000;
	
	rc = ngx_event_connect_peer(&p->upstream, timeout);
	if (rc == NGX_ETIMEDOUT)
	    return -1;
    }
	
    if (rc == NGX_ERROR || rc == NGX_BUSY || rc == NGX_DECLINED) {
    	ngx_mail_proxy_internal_server_error(s);
	return 0;
    }

    ngx_add_timer(p->upstream.connection->read, cscf->timeout);

    p->upstream.connection->pool = s->connection->pool;
    p->upstream.connection->data = s;

    s->connection->read->handler = ngx_mail_proxy_block_read;
    p->upstream.connection->write->handler = ngx_mail_proxy_dummy_handler;

    ngx_int_t secured = 0;
#if (NGX_MAIL_SSL)
    /* early explicit ssl */
    if (s->con_proto == NGX_MAIL_AUTH_PROTO_SSL ||
	    s->con_proto == NGX_MAIL_AUTH_PROTO_SECURE) {
	s->connection->log->action = "SSL handshaking";
    
	ngx_mail_ssl_conf_t *sslcf = ngx_mail_get_module_srv_conf(s, ngx_mail_ssl_module);
	if (sslcf->ssl.ctx) {
    	    ngx_mail_ssl_init_connection(&sslcf->ssl,
		    p->upstream.connection, NGX_MAIL_SECURE_DIR_OUT);
	    secured = 1;
	}
	else {
	    ngx_mail_proxy_internal_server_error(s);
	    return 0;
	}
    }
#endif
    pcf = ngx_mail_get_module_srv_conf(s, ngx_mail_proxy_module);

    s->proxy->buffer = ngx_create_temp_buf(s->connection->pool,
                                           pcf->buffer_size);
    if (s->proxy->buffer == NULL) {
        ngx_mail_proxy_internal_server_error(s);
        return 0;
    }

    s->out.len = 0;

    /* stls = set handler & state
     * secure = handler will be set via ssl init routine */
    if (s->con_proto == NGX_MAIL_AUTH_PROTO_STLS)
	switch (s->protocol) {
	    case NGX_MAIL_POP3_PROTOCOL:
		s->mail_state = ngx_pop3_stls;
		break;
	    case NGX_MAIL_IMAP_PROTOCOL:
		s->mail_state = ngx_imap_stls;
		break;
	}
    if (!secured)
	ngx_mail_proxy_start(p->upstream.connection);

    return 0;
}
void ngx_mail_proxy_start(ngx_connection_t *p)
{
    ngx_mail_session_t *s = p->data;

switch (s->protocol) {

    case NGX_MAIL_POP3_PROTOCOL:
        p->read->handler = ngx_mail_proxy_pop3_handler;
	if (s->mail_state != ngx_pop3_stls)
    	    s->mail_state = ngx_pop3_start;
        break;

    case NGX_MAIL_IMAP_PROTOCOL:
        p->read->handler = ngx_mail_proxy_imap_handler;
	if (s->mail_state != ngx_imap_stls)
	    s->mail_state = ngx_imap_start;
        break;

    default: /* NGX_MAIL_SMTP_PROTOCOL */
        p->read->handler = ngx_mail_proxy_smtp_handler;
	if (s->mail_state != ngx_smtp_helo_xclient &&
	    s->mail_state != ngx_smtp_helo_from &&
	    s->mail_state != ngx_smtp_helo)
    		    s->mail_state = ngx_smtp_start;
        break;
}
}

static void
ngx_mail_proxy_block_read(ngx_event_t *rev)
{
    ngx_connection_t    *c;
    ngx_mail_session_t  *s;

    ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0, "mail proxy block read");

    if (ngx_handle_read_event(rev, 0) != NGX_OK) {
        c = rev->data;
        s = c->data;

        ngx_mail_proxy_close_session(s);
    }
}

#if (NGX_MAIL_SSL)
static ngx_int_t
ngx_mail_proxy_handle_junk(ngx_connection_t *c)
{
    ngx_mail_session_t *s = c->data;
    ngx_mail_proxy_ctx_t *p = s->proxy;
    p->upstream.connection->read->pending_eof = 1;
    
    ssize_t len = p->buffer->end - p->buffer->last;
    ssize_t rc;
    ngx_int_t have_kqueue = 0;
#if (NGX_HAVE_KQUEUE)
    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT)
	have_kqueue = 1;
#endif

    if (!s->junk_flag) {
	p->upstream.connection->read->pending_eof = 0;
    
	do {
	    if (have_kqueue)
    		len = p->upstream.connection->read->available;
	    rc = p->upstream.connection->recv
	      (p->upstream.connection, p->buffer->start, len);
	    /* i can also modify ngx_unix_recv to return
	     * explicit "not ready", but that will be
	     * overengineering for such crappy task */
	    if (rc == NGX_ERROR)
		return rc;
	    /* take what is here now */
	    if (rc == NGX_AGAIN)
		break;
	} while(1);

	/* designate our ssl junk by CR LF */
	if (c->send(c, CRLF, 2) != 2)
	    return NGX_ERROR;
	
	s->junk_flag = 1;
	return ngx_mail_proxy_handle_junk(c);
    } else {
        ngx_log_debug0(NGX_LOG_DEBUG_MAIL, s->connection->log, 0,
    		"blocking recv junk");
	do {
	    if (have_kqueue)
		len = p->upstream.connection->read->available;
	    rc = p->upstream.connection->recv
	    (p->upstream.connection, p->buffer->start, len);
	    ngx_log_debug0(NGX_LOG_DEBUG_MAIL, s->connection->log,
		    0, "recv ret: %d", rc);
	    
	    if (rc == NGX_ERROR || rc == 0) 
		return NGX_ERROR;
	    if (rc == NGX_AGAIN) {
		/* must block */
		return NGX_AGAIN;
	    }
	    break;
	} while(1);

	s->con_proto = NGX_MAIL_AUTH_PROTO_STLS;
	p->upstream.connection->read->eof = 0;
    }
		    
    return NGX_OK;
}
#endif

static void
ngx_mail_proxy_pop3_handler(ngx_event_t *rev)
{
    u_char                 *p;
    ngx_int_t               rc = NGX_OK;
    ngx_str_t               line;
    ngx_connection_t       *c;
    ngx_mail_session_t     *s;
    ngx_mail_proxy_conf_t  *pcf;

    ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0,
                   "mail proxy pop3 auth handler");

    c = rev->data;
    s = c->data;

    if (rev->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT,
                      "upstream timed out");
        c->timedout = 1;
        ngx_mail_proxy_internal_server_error(s);
        return;
    }

#if (NGX_MAIL_SSL)
    ngx_int_t handshaked = 0;
    if (c->ssl) if (c->ssl->handshaked) handshaked = 1;
    if (s->con_proto == NGX_MAIL_AUTH_PROTO_SECURE && !handshaked) {
	    /* take off response stayed after failed pop3s on pop3 port */
	    if ( (rc = ngx_mail_proxy_handle_junk(c)) == NGX_OK)
		    s->mail_state = ngx_imap_stls;
    		    //s->mail_state = ngx_pop3_noop;
    }
    /* nothing to receive */
    else if (s->con_proto == NGX_MAIL_AUTH_PROTO_STLS && handshaked &&
		    s->mail_state == ngx_pop3_start)
	    ;
    else
#endif
    rc = ngx_mail_proxy_read_response(s, 0);

//#if (NGX_MAIL_SSL)
    /* Not all of servers are RFC 1939 compliant, and even compliant may
     * most probably 'll not allow NOOP in A-state, so give on then */
    //if (s->mail_state == ngx_pop3_stls && rc == NGX_ERROR)
	//rc = NGX_OK;
//#endif
	
    if (rc == NGX_AGAIN) {
        return;
    }

    if (rc == NGX_ERROR) {
#if (NGX_MAIL_SSL)
	/* not every server doing clean ssl shutdown */
	if (c->ssl) {
	    if (!handshaked)
		c->ssl->no_wait_shutdown = c->ssl->no_send_shutdown = 1;
	    ngx_ssl_shutdown(c);
	    if (!handshaked) {
		ngx_post_event(c->read, &ngx_posted_events);
	    }
	}
#endif
        ngx_mail_proxy_upstream_error(s);
        return;
    }

    switch (s->mail_state) {
#if (NGX_MAIL_SSL)
    /*case ngx_pop3_noop:
        ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0, "mail proxy send no op");

        line.len = sizeof("NOOP") - 1 + 2;
        line.data = ngx_pnalloc(c->pool, line.len);
        if (line.data == NULL) {
            ngx_mail_proxy_internal_server_error(s);
            return;
        }
        p = ngx_cpymem(line.data, "NOOP", sizeof("NOOP") - 1);
        *p++ = CR; *p = LF;
        s->mail_state = ngx_pop3_stls;
        break;*/
    case ngx_pop3_stls:
        ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0, "mail proxy send stls");
	
        line.len = sizeof("STLS") - 1 + 2;
        line.data = ngx_pnalloc(c->pool, line.len);
        if (line.data == NULL) {
            ngx_mail_proxy_internal_server_error(s);
            return;
        }
        p = ngx_cpymem(line.data, "STLS", sizeof("STLS") - 1);
        *p++ = CR; *p = LF;
	s->mail_state = ngx_pop3_stls_init;
	break;
   case ngx_pop3_stls_init:
	ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0, "mail proxy init stls");
   
        s->connection->log->action = "starttls handshaking";
        ngx_mail_ssl_conf_t *sslcf = ngx_mail_get_module_srv_conf(s,
	    ngx_mail_ssl_module);
        if (sslcf->ssl.ctx)
                ngx_mail_ssl_init_connection(&sslcf->ssl,
                    s->proxy->upstream.connection, NGX_MAIL_SECURE_DIR_OUT);
	return;
	/* c->read->handler != ngx_mail_proxy_pop3_handler */
#endif

    case ngx_pop3_start:
        ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0, "mail proxy send user");

        s->connection->log->action = "sending user name to upstream";

        line.len = sizeof("USER ")  - 1 + s->login.len + 2;
        line.data = ngx_pnalloc(c->pool, line.len);
        if (line.data == NULL) {
            ngx_mail_proxy_internal_server_error(s);
            return;
        }

        p = ngx_cpymem(line.data, "USER ", sizeof("USER ") - 1);
        p = ngx_cpymem(p, s->login.data, s->login.len);
        *p++ = CR; *p = LF;

        s->mail_state = ngx_pop3_user;
        break;

    case ngx_pop3_user:
        ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0, "mail proxy send pass");

        s->connection->log->action = "sending password to upstream";

        line.len = sizeof("PASS ")  - 1 + s->passwd.len + 2;
        line.data = ngx_pnalloc(c->pool, line.len);
        if (line.data == NULL) {
            ngx_mail_proxy_internal_server_error(s);
            return;
        }

        p = ngx_cpymem(line.data, "PASS ", sizeof("PASS ") - 1);
        p = ngx_cpymem(p, s->passwd.data, s->passwd.len);
        *p++ = CR; *p = LF;

        s->mail_state = ngx_pop3_passwd;
        break;

    case ngx_pop3_passwd:
        s->connection->read->handler = ngx_mail_proxy_handler;
        s->connection->write->handler = ngx_mail_proxy_handler;
        rev->handler = ngx_mail_proxy_handler;
        c->write->handler = ngx_mail_proxy_handler;

        pcf = ngx_mail_get_module_srv_conf(s, ngx_mail_proxy_module);
        ngx_add_timer(s->connection->read, pcf->timeout);
        ngx_del_timer(c->read);

        c->log->action = NULL;
        ngx_log_error(NGX_LOG_INFO, c->log, 0, "client logged in");

        ngx_mail_proxy_handler(s->connection->write);

        return;

    default:
#if (NGX_SUPPRESS_WARN)
        ngx_str_null(&line);
#endif
        break;
    }
    
    if (c->send(c, line.data, line.len) < (ssize_t) line.len) {
        /*
         * we treat the incomplete sending as NGX_ERROR
         * because it is very strange here
         */
        ngx_mail_proxy_internal_server_error(s);
        return;
    }

    s->proxy->buffer->pos = s->proxy->buffer->start;
    s->proxy->buffer->last = s->proxy->buffer->start;
}


static void
ngx_mail_proxy_imap_handler(ngx_event_t *rev)
{
    u_char                 *p;
    ngx_int_t               rc = NGX_OK;
    ngx_str_t               line;
    ngx_connection_t       *c;
    ngx_mail_session_t     *s;
    ngx_mail_proxy_conf_t  *pcf;

    ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0,
                   "mail proxy imap auth handler");

    c = rev->data;
    s = c->data;

    if (rev->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT,
                      "upstream timed out");
        c->timedout = 1;
        ngx_mail_proxy_internal_server_error(s);
        return;
    }
#if (NGX_MAIL_SSL)
    ngx_int_t handshaked = 0;
    if (c->ssl) if (c->ssl->handshaked) handshaked = 1;
    if (s->con_proto == NGX_MAIL_AUTH_PROTO_SECURE && !handshaked) {
	if ( (rc = ngx_mail_proxy_handle_junk(c)) == NGX_OK)
	    s->mail_state = ngx_imap_stls;
    }
    else if (s->con_proto == NGX_MAIL_AUTH_PROTO_STLS && handshaked &&
		    s->mail_state == ngx_imap_start)
	    ;
    else
#endif
    rc = ngx_mail_proxy_read_response(s, s->mail_state);

    if (rc == NGX_AGAIN) {
        return;
    }

    if (rc == NGX_ERROR) {
#if (NGX_MAIL_SSL)
	if (c->ssl) {
	    if (!handshaked)
		c->ssl->no_wait_shutdown = c->ssl->no_send_shutdown = 1;
	    ngx_ssl_shutdown(c);
	    if (!handshaked) {
		ngx_post_event(c->read, &ngx_posted_events);
	    }
	}
#endif
        ngx_mail_proxy_upstream_error(s);
        return;
    }

    switch (s->mail_state) {
#if (NGX_MAIL_SSL)
    case ngx_imap_stls:
	ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0, "mail proxy send stls");
	
	line.len = s->tag.len + sizeof("STARTTLS") - 1 + 2;
	line.data = ngx_pnalloc(c->pool, line.len);
	if (line.data == NULL) {
	    ngx_mail_proxy_internal_server_error(s);
	    return;
	}
	ngx_sprintf(line.data, "%VSTARTTLS" CRLF, &s->tag);
	s->mail_state = ngx_imap_stls_init;
	break;
    case ngx_imap_stls_init:
	ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0, "mail proxy init stls");
	
	s->connection->log->action = "starttls handshaking";
	ngx_mail_ssl_conf_t *sslcf = ngx_mail_get_module_srv_conf(s,
	    ngx_mail_ssl_module);
	if (sslcf->ssl.ctx)
		ngx_mail_ssl_init_connection(&sslcf->ssl,
		    s->proxy->upstream.connection, NGX_MAIL_SECURE_DIR_OUT);
	return;
#endif

    case ngx_imap_start:
        ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0,
                       "mail proxy send login");

        s->connection->log->action = "sending LOGIN command to upstream";

        line.len = s->tag.len + sizeof("LOGIN ") - 1
                   + 1 + NGX_SIZE_T_LEN + 1 + 2;
        line.data = ngx_pnalloc(c->pool, line.len);
        if (line.data == NULL) {
            ngx_mail_proxy_internal_server_error(s);
            return;
        }

        line.len = ngx_sprintf(line.data, "%VLOGIN {%uz}" CRLF,
                               &s->tag, s->login.len)
                   - line.data;

        s->mail_state = ngx_imap_login;
        break;

    case ngx_imap_login:
        ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0, "mail proxy send user");

        s->connection->log->action = "sending user name to upstream";

        line.len = s->login.len + 1 + 1 + NGX_SIZE_T_LEN + 1 + 2;
        line.data = ngx_pnalloc(c->pool, line.len);
        if (line.data == NULL) {
            ngx_mail_proxy_internal_server_error(s);
            return;
        }

        line.len = ngx_sprintf(line.data, "%V {%uz}" CRLF,
                               &s->login, s->passwd.len)
                   - line.data;

        s->mail_state = ngx_imap_user;
        break;

    case ngx_imap_user:
        ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0,
                       "mail proxy send passwd");

        s->connection->log->action = "sending password to upstream";

        line.len = s->passwd.len + 2;
        line.data = ngx_pnalloc(c->pool, line.len);
        if (line.data == NULL) {
            ngx_mail_proxy_internal_server_error(s);
            return;
        }

        p = ngx_cpymem(line.data, s->passwd.data, s->passwd.len);
        *p++ = CR; *p = LF;

        s->mail_state = ngx_imap_passwd;
        break;

    case ngx_imap_passwd:
        s->connection->read->handler = ngx_mail_proxy_handler;
        s->connection->write->handler = ngx_mail_proxy_handler;
        rev->handler = ngx_mail_proxy_handler;
        c->write->handler = ngx_mail_proxy_handler;

        pcf = ngx_mail_get_module_srv_conf(s, ngx_mail_proxy_module);
        ngx_add_timer(s->connection->read, pcf->timeout);
        ngx_del_timer(c->read);

        c->log->action = NULL;
        ngx_log_error(NGX_LOG_INFO, c->log, 0, "client logged in");

        ngx_mail_proxy_handler(s->connection->write);

        return;

    default:
#if (NGX_SUPPRESS_WARN)
        ngx_str_null(&line);
#endif
        break;
    }

    if (c->send(c, line.data, line.len) < (ssize_t) line.len) {
        /*
         * we treat the incomplete sending as NGX_ERROR
         * because it is very strange here
         */
        ngx_mail_proxy_internal_server_error(s);
        return;
    }

    s->proxy->buffer->pos = s->proxy->buffer->start;
    s->proxy->buffer->last = s->proxy->buffer->start;
}


static void
ngx_mail_proxy_smtp_handler(ngx_event_t *rev)
{
    u_char                    *p;
    ngx_int_t                  rc = NGX_OK;
    ngx_str_t                  line;
    ngx_buf_t                 *b;
    ngx_connection_t          *c;
    ngx_mail_session_t        *s;
    ngx_mail_proxy_conf_t     *pcf = NULL;
    ngx_mail_core_srv_conf_t  *cscf;

    ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0,
                   "mail proxy smtp auth handler");

    c = rev->data;
    s = c->data;

    if (rev->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT,
                      "upstream timed out");
        c->timedout = 1;
        ngx_mail_proxy_internal_server_error(s);
        return;
    }
#if (NGX_MAIL_SSL)
    ngx_int_t handshaked = 0;
    if (c->ssl) if (c->ssl->handshaked) handshaked = 1;
    if (s->con_proto == NGX_MAIL_AUTH_PROTO_SECURE && !handshaked)
	    rc = ngx_mail_proxy_handle_junk(c);
    else if (s->con_proto == NGX_MAIL_AUTH_PROTO_STLS && handshaked &&
		    (s->mail_state == ngx_smtp_xclient ||
		     s->mail_state == ngx_smtp_helo_from ||
		     s->mail_state == ngx_smtp_helo) 
	    )
	    ;
    else
#endif
    rc = ngx_mail_proxy_read_response(s, s->mail_state);

    if (rc == NGX_AGAIN) {
        return;
    }

    if (rc == NGX_ERROR) {
#if (NGX_MAIL_SSL)
	if (c->ssl) {
	    if (!handshaked)
		c->ssl->no_wait_shutdown = c->ssl->no_send_shutdown = 1;
	    ngx_ssl_shutdown(c);
	    if (!handshaked) {
		ngx_post_event(c->read, &ngx_posted_events);
	    }
	}
#endif
        ngx_mail_proxy_upstream_error(s);
        return;
    }

    switch (s->mail_state) {

    case ngx_smtp_start:
        ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0, "mail proxy send ehlo");

        s->connection->log->action = "sending HELO/EHLO to upstream";

        cscf = ngx_mail_get_module_srv_conf(s, ngx_mail_core_module);

        line.len = sizeof("HELO ")  - 1 + cscf->server_name.len + 2;
        line.data = ngx_pnalloc(c->pool, line.len);
        if (line.data == NULL) {
            ngx_mail_proxy_internal_server_error(s);
            return;
        }

        pcf = ngx_mail_get_module_srv_conf(s, ngx_mail_proxy_module);

        p = ngx_cpymem(line.data,
                       ((s->esmtp || pcf->xclient) ? "EHLO " : "HELO "),
                       sizeof("HELO ") - 1);

        p = ngx_cpymem(p, cscf->server_name.data, cscf->server_name.len);
        *p++ = CR; *p = LF;

#if (NGX_MAIL_SSL)
	if (s->con_proto == NGX_MAIL_AUTH_PROTO_STLS)
	    s->mail_state = ngx_smtp_stls;
	else
#endif
        if (pcf->xclient) {
            s->mail_state = ngx_smtp_helo_xclient;

        } else if (s->auth_method == NGX_MAIL_AUTH_NONE) {
            s->mail_state = ngx_smtp_helo_from;

        } else {
            s->mail_state = ngx_smtp_helo;
        }

        break;

#if (NGX_MAIL_SSL)	
    case ngx_smtp_stls:
	ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0, "mail proxy send stls");
	
	line.len = sizeof("STARTTLS") - 1 + 2;
	line.data = ngx_pnalloc(c->pool, line.len);
	if (line.data == NULL) {
	    ngx_mail_proxy_internal_server_error(s);
	    return;
	}
	p = ngx_cpymem(line.data, "STARTTLS", sizeof("STARTTLS") - 1);
	*p++ = CR; *p = LF;
	s->mail_state = ngx_smtp_stls_init;
	break;
    case ngx_smtp_stls_init:
	ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0, "mail proxy init stls");

        pcf = ngx_mail_get_module_srv_conf(s, ngx_mail_proxy_module);
        if (pcf->xclient)
            s->mail_state = ngx_smtp_helo_xclient;
        else if (s->auth_method == NGX_MAIL_AUTH_NONE)
            s->mail_state = ngx_smtp_helo_from;
        else s->mail_state = ngx_smtp_helo;
	
	s->connection->log->action = "starttls handshaking";
	ngx_mail_ssl_conf_t *sslcf = ngx_mail_get_module_srv_conf(s,
	    ngx_mail_ssl_module);
	if (sslcf->ssl.ctx)
		ngx_mail_ssl_init_connection(&sslcf->ssl,
		    s->proxy->upstream.connection, NGX_MAIL_SECURE_DIR_OUT);
	return;
#endif	

    case ngx_smtp_helo_xclient:
        ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0,
                       "mail proxy send xclient");

        s->connection->log->action = "sending XCLIENT to upstream";

        line.len = sizeof("XCLIENT ADDR= LOGIN= NAME="
                          CRLF) - 1
                   + s->connection->addr_text.len + s->login.len + s->host.len;

        line.data = ngx_pnalloc(c->pool, line.len);
        if (line.data == NULL) {
            ngx_mail_proxy_internal_server_error(s);
            return;
        }

        line.len = ngx_sprintf(line.data,
                       "XCLIENT ADDR=%V%s%V NAME=%V" CRLF,
                       &s->connection->addr_text,
                       (s->login.len ? " LOGIN=" : ""), &s->login, &s->host)
                   - line.data;

        if (s->smtp_helo.len) {
            s->mail_state = ngx_smtp_xclient_helo;

        } else if (s->auth_method == NGX_MAIL_AUTH_NONE) {
            s->mail_state = ngx_smtp_xclient_from;

        } else {
            s->mail_state = ngx_smtp_xclient;
        }

        break;

    case ngx_smtp_xclient_helo:
        ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0,
                       "mail proxy send client ehlo");

        s->connection->log->action = "sending client HELO/EHLO to upstream";

        line.len = sizeof("HELO " CRLF) - 1 + s->smtp_helo.len;

        line.data = ngx_pnalloc(c->pool, line.len);
        if (line.data == NULL) {
            ngx_mail_proxy_internal_server_error(s);
            return;
        }

        line.len = ngx_sprintf(line.data,
                       ((s->esmtp) ? "EHLO %V" CRLF : "HELO %V" CRLF),
                       &s->smtp_helo)
                   - line.data;

        s->mail_state = (s->auth_method == NGX_MAIL_AUTH_NONE) ?
                            ngx_smtp_helo_from : ngx_smtp_helo;

        break;

    case ngx_smtp_helo_from:
    case ngx_smtp_xclient_from:
        ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0,
                       "mail proxy send mail from");

        s->connection->log->action = "sending MAIL FROM to upstream";

        line.len = s->smtp_from.len + sizeof(CRLF) - 1;
        line.data = ngx_pnalloc(c->pool, line.len);
        if (line.data == NULL) {
            ngx_mail_proxy_internal_server_error(s);
            return;
        }

        p = ngx_cpymem(line.data, s->smtp_from.data, s->smtp_from.len);
        *p++ = CR; *p = LF;

        s->mail_state = ngx_smtp_from;

        break;

    case ngx_smtp_from:
        ngx_log_debug0(NGX_LOG_DEBUG_MAIL, rev->log, 0,
                       "mail proxy send rcpt to");

        s->connection->log->action = "sending RCPT TO to upstream";

        line.len = s->smtp_to.len + sizeof(CRLF) - 1;
        line.data = ngx_pnalloc(c->pool, line.len);
        if (line.data == NULL) {
            ngx_mail_proxy_internal_server_error(s);
            return;
        }

        p = ngx_cpymem(line.data, s->smtp_to.data, s->smtp_to.len);
        *p++ = CR; *p = LF;

        s->mail_state = ngx_smtp_to;

        break;

    case ngx_smtp_helo:
    case ngx_smtp_xclient:
    case ngx_smtp_to:

        b = s->proxy->buffer;

        if (s->auth_method == NGX_MAIL_AUTH_NONE) {
            b->pos = b->start;

        } else {
            ngx_memcpy(b->start, smtp_auth_ok, sizeof(smtp_auth_ok) - 1);
            b->last = b->start + sizeof(smtp_auth_ok) - 1;
        }

        s->connection->read->handler = ngx_mail_proxy_handler;
        s->connection->write->handler = ngx_mail_proxy_handler;
        rev->handler = ngx_mail_proxy_handler;
        c->write->handler = ngx_mail_proxy_handler;

        pcf = ngx_mail_get_module_srv_conf(s, ngx_mail_proxy_module);
        ngx_add_timer(s->connection->read, pcf->timeout);
        ngx_del_timer(c->read);

        c->log->action = NULL;
        ngx_log_error(NGX_LOG_INFO, c->log, 0, "client logged in");

        ngx_mail_proxy_handler(s->connection->write);

        return;

    default:
#if (NGX_SUPPRESS_WARN)
        ngx_str_null(&line);
#endif
        break;
    }

    if (c->send(c, line.data, line.len) < (ssize_t) line.len) {
        /*
         * we treat the incomplete sending as NGX_ERROR
         * because it is very strange here
         */
        ngx_mail_proxy_internal_server_error(s);
        return;
    }

    s->proxy->buffer->pos = s->proxy->buffer->start;
    s->proxy->buffer->last = s->proxy->buffer->start;
}


static void
ngx_mail_proxy_dummy_handler(ngx_event_t *wev)
{
    ngx_connection_t    *c;
    ngx_mail_session_t  *s;

    ngx_log_debug0(NGX_LOG_DEBUG_MAIL, wev->log, 0, "mail proxy dummy handler");

    if (ngx_handle_write_event(wev, 0) != NGX_OK) {
        c = wev->data;
        s = c->data;

        ngx_mail_proxy_close_session(s);
    }
}


static ngx_int_t
ngx_mail_proxy_read_response(ngx_mail_session_t *s, ngx_uint_t state)
{
    u_char                 *p;
    ssize_t                 n;
    ngx_buf_t              *b;
    ngx_mail_proxy_conf_t  *pcf;

    s->connection->log->action = "reading response from upstream";

    b = s->proxy->buffer;

    n = s->proxy->upstream.connection->recv(s->proxy->upstream.connection,
                                            b->last, b->end - b->last);

    if (n == NGX_ERROR || n == 0) {
        return NGX_ERROR;
    }

    if (n == NGX_AGAIN) {
        return NGX_AGAIN;
    }

    b->last += n;

    if (b->last - b->pos < 4) {
        return NGX_AGAIN;
    }

    if (*(b->last - 2) != CR || *(b->last - 1) != LF) {
        if (b->last == b->end) {
            *(b->last - 1) = '\0';
            ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                          "upstream sent too long response line: \"%s\"",
                          b->pos);
            return NGX_ERROR;
        }

        return NGX_AGAIN;
    }

    p = b->pos;
#if (NGX_MAIL_HANDLE_GMAIL)
    u_char *ptemp;
#endif

    switch (s->protocol) {

    case NGX_MAIL_POP3_PROTOCOL:
        if (p[0] == '+' && p[1] == 'O' && p[2] == 'K') {
            return NGX_OK;
        }
        break;

    case NGX_MAIL_IMAP_PROTOCOL:
        switch (state) {

        case ngx_imap_start:
#if (NGX_MAIL_SSL)
	case ngx_imap_stls:
#endif
            if (p[0] == '*' && p[1] == ' ' && p[2] == 'O' && p[3] == 'K') {
                return NGX_OK;
            }
            break;

        case ngx_imap_login:
        case ngx_imap_user:
            if (p[0] == '+') {
                return NGX_OK;
            }
            break;

        case ngx_imap_passwd:
#if (NGX_MAIL_SSL)
	case ngx_imap_stls_init:
#endif
#if (NGX_MAIL_HANDLE_GMAIL)
	    *(b->last - 2) = '\0';
	    /* pass & cut inappropriate unasked CAPABILITY answer */
	    if ( (ptemp = ngx_strstrn(p, s->tag.data, s->tag.len - 1)) ) {
		b->pos = p = ptemp;
#else
            if (ngx_strncmp(p, s->tag.data, s->tag.len) == 0) {
#endif
                p += s->tag.len;
                if (p[0] == 'O' && p[1] == 'K') {
                    return NGX_OK;
                }
            }
            break;
        }

        break;

    default: /* NGX_MAIL_SMTP_PROTOCOL */
        switch (state) {

        case ngx_smtp_start:
#if (NGX_MAIL_SSL)
	case ngx_smtp_stls_init:
#endif
            if (p[0] == '2' && p[1] == '2' && p[2] == '0') {
                return NGX_OK;
            }
            break;

        case ngx_smtp_helo:
        case ngx_smtp_helo_xclient:
        case ngx_smtp_helo_from:
        case ngx_smtp_from:
#if (NGX_MAIL_SSL)
	case ngx_smtp_stls:
#endif
            if (p[0] == '2' && p[1] == '5' && p[2] == '0') {
                return NGX_OK;
            }
            break;

        case ngx_smtp_xclient:
        case ngx_smtp_xclient_from:
        case ngx_smtp_xclient_helo:
            if (p[0] == '2' && (p[1] == '2' || p[1] == '5') && p[2] == '0') {
                return NGX_OK;
            }
            break;

        case ngx_smtp_to:
            return NGX_OK;
        }

        break;
    }

    pcf = ngx_mail_get_module_srv_conf(s, ngx_mail_proxy_module);

    if (pcf->pass_error_message == 0) {
        *(b->last - 2) = '\0';
        ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                      "upstream sent invalid response: \"%s\"", p);
        return NGX_ERROR;
    }

    s->out.len = b->last - p - 2;
    s->out.data = p;

    ngx_log_error(NGX_LOG_INFO, s->connection->log, 0,
                  "upstream sent invalid response: \"%V\"", &s->out);

    s->out.len = b->last - b->pos;
    s->out.data = b->pos;

    return NGX_ERROR;
}


static void
ngx_mail_proxy_handler(ngx_event_t *ev)
{
    char                   *action, *recv_action, *send_action;
    size_t                  size;
    ssize_t                 n;
    ngx_buf_t              *b;
    ngx_uint_t              do_write;
    ngx_connection_t       *c, *src, *dst;
    ngx_mail_session_t     *s;
    ngx_mail_proxy_conf_t  *pcf;

    c = ev->data;
    s = c->data;

    if (ev->timedout) {
        c->log->action = "proxying";

        if (c == s->connection) {
            ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT,
                          "client timed out");
            c->timedout = 1;

        } else {
            ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT,
                          "upstream timed out");
        }

        ngx_mail_proxy_close_session(s);
        return;
    }

    if (c == s->connection) {
        if (ev->write) {
            recv_action = "proxying and reading from upstream";
            send_action = "proxying and sending to client";
            src = s->proxy->upstream.connection;
            dst = c;
            b = s->proxy->buffer;

        } else {
            recv_action = "proxying and reading from client";
            send_action = "proxying and sending to upstream";
            src = c;
            dst = s->proxy->upstream.connection;
            b = s->buffer;
        }

    } else {
        if (ev->write) {
            recv_action = "proxying and reading from client";
            send_action = "proxying and sending to upstream";
            src = s->connection;
            dst = c;
            b = s->buffer;

        } else {
            recv_action = "proxying and reading from upstream";
            send_action = "proxying and sending to client";
            src = c;
            dst = s->connection;
            b = s->proxy->buffer;
        }
    }

    do_write = ev->write ? 1 : 0;

    ngx_log_debug3(NGX_LOG_DEBUG_MAIL, ev->log, 0,
                   "mail proxy handler: %d, #%d > #%d",
                   do_write, src->fd, dst->fd);

    for ( ;; ) {

        if (do_write) {

            size = b->last - b->pos;

            if (size && dst->write->ready) {
                c->log->action = send_action;

                n = dst->send(dst, b->pos, size);

                if (n == NGX_ERROR) {
                    ngx_mail_proxy_close_session(s);
                    return;
                }

                if (n > 0) {
                    b->pos += n;

                    if (b->pos == b->last) {
                        b->pos = b->start;
                        b->last = b->start;
                    }
                }
            }
        }

        size = b->end - b->last;

        if (size && src->read->ready) {
            c->log->action = recv_action;

            n = src->recv(src, b->last, size);

            if (n == NGX_AGAIN || n == 0) {
                break;
            }

            if (n > 0) {
                do_write = 1;
                b->last += n;

                continue;
            }

            if (n == NGX_ERROR) {
                src->read->eof = 1;
            }
        }

        break;
    }

    c->log->action = "proxying";

    if (
    (s->connection->read->eof && s->buffer->pos == s->buffer->last)
        || (s->proxy->upstream.connection->read->eof
    	    && s->proxy->buffer->pos == s->proxy->buffer->last)
        || (s->connection->read->eof
            && s->proxy->upstream.connection->read->eof))
    {
        action = c->log->action;
        c->log->action = NULL;
        ngx_log_error(NGX_LOG_INFO, c->log, 0, "proxied session done");
        c->log->action = action;
	
        ngx_mail_proxy_close_session(s);
        return;
    }

    if (ngx_handle_write_event(dst->write, 0) != NGX_OK) {
        ngx_mail_proxy_close_session(s);
        return;
    }

    if (ngx_handle_read_event(dst->read, 0) != NGX_OK) {
        ngx_mail_proxy_close_session(s);
        return;
    }

    if (ngx_handle_write_event(src->write, 0) != NGX_OK) {
        ngx_mail_proxy_close_session(s);
        return;
    }

    if (ngx_handle_read_event(src->read, 0) != NGX_OK) {
        ngx_mail_proxy_close_session(s);
        return;
    }

    if (c == s->connection) {
        pcf = ngx_mail_get_module_srv_conf(s, ngx_mail_proxy_module);
        ngx_add_timer(c->read, pcf->timeout);
    }
}


static void
ngx_mail_proxy_upstream_error(ngx_mail_session_t *s)
{
    if (s->proxy->upstream.connection) {
        ngx_log_debug1(NGX_LOG_DEBUG_MAIL, s->connection->log, 0,
                       "close mail proxy connection: %d",
                       s->proxy->upstream.connection->fd);

        ngx_close_connection(s->proxy->upstream.connection);
    }

    if (s->out.len == 0) {
        ngx_mail_session_internal_server_error(s);
        return;
    }

    s->quit = 1;
    ngx_mail_send(s->connection->write);
}


void
ngx_mail_proxy_internal_server_error(ngx_mail_session_t *s)
{
    if (s->proxy->upstream.connection) {
        ngx_log_debug1(NGX_LOG_DEBUG_MAIL, s->connection->log, 0,
                       "close mail proxy connection: %d",
                       s->proxy->upstream.connection->fd);

        ngx_close_connection(s->proxy->upstream.connection);
    }

    ngx_mail_session_internal_server_error(s);
}


static void
ngx_mail_proxy_close_session(ngx_mail_session_t *s)
{
    if (s->proxy->upstream.connection) {
        ngx_log_debug1(NGX_LOG_DEBUG_MAIL, s->connection->log, 0,
                       "close mail proxy connection: %d",
                       s->proxy->upstream.connection->fd);

        ngx_close_connection(s->proxy->upstream.connection);
    }

    ngx_mail_close_connection(s->connection);
}


static void *
ngx_mail_proxy_create_conf(ngx_conf_t *cf)
{
    ngx_mail_proxy_conf_t  *pcf;

    pcf = ngx_pcalloc(cf->pool, sizeof(ngx_mail_proxy_conf_t));
    if (pcf == NULL) {
        return NULL;
    }

    pcf->enable = NGX_CONF_UNSET;
    pcf->pass_error_message = NGX_CONF_UNSET;
    pcf->xclient = NGX_CONF_UNSET;
    pcf->buffer_size = NGX_CONF_UNSET_SIZE;
    pcf->timeout = NGX_CONF_UNSET_MSEC;

    return pcf;
}


static char *
ngx_mail_proxy_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_mail_proxy_conf_t *prev = parent;
    ngx_mail_proxy_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_value(conf->pass_error_message, prev->pass_error_message, 0);
    ngx_conf_merge_value(conf->xclient, prev->xclient, 1);
    ngx_conf_merge_size_value(conf->buffer_size, prev->buffer_size,
                              (size_t) ngx_pagesize);
    ngx_conf_merge_msec_value(conf->timeout, prev->timeout, 24 * 60 * 60000);

    return NGX_CONF_OK;
}
