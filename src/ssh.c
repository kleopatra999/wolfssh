/* ssh.c 
 *
 * Copyright (C) 2014 wolfSSL Inc.
 *
 * This file is part of wolfSSH.
 *
 * wolfSSH is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSH is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssh/ssh.h>
#include <wolfssh/internal.h>
#include <wolfssh/log.h>


int wolfSSH_Init(void)
{
    WLOG(WS_LOG_DEBUG, "Enter wolfSSH_Init()");
    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_Init(), returning %d", WS_SUCCESS);
    return WS_SUCCESS;
}


int wolfSSH_Cleanup(void)
{
    WLOG(WS_LOG_DEBUG, "Enter wolfSSH_Cleanup()");
    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_Cleanup(), returning %d", WS_SUCCESS);
    return WS_SUCCESS;
}


static WOLFSSH_CTX* CtxInit(WOLFSSH_CTX* ctx, void* heap)
{
    WLOG(WS_LOG_DEBUG, "Enter CtxInit()");

    if (ctx == NULL)
        return ctx;

    WMEMSET(ctx, 0, sizeof(WOLFSSH_CTX));

    if (heap)
        ctx->heap = heap;

#ifndef WOLFSSH_USER_IO
    ctx->ioRecvCb = wsEmbedRecv;
    ctx->ioSendCb = wsEmbedSend;
#endif /* WOLFSSH_USER_IO */

    return ctx;
}


WOLFSSH_CTX* wolfSSH_CTX_new(void* heap)
{
    WOLFSSH_CTX* ctx;

    WLOG(WS_LOG_DEBUG, "Enter wolfSSH_CTX_new()");

    ctx = (WOLFSSH_CTX*)WMALLOC(sizeof(WOLFSSH_CTX), heap, WOLFSSH_CTX_TYPE);
    ctx = CtxInit(ctx, heap);

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_CTX_new(), ctx = %p", ctx);

    return ctx;
}


static void CtxResourceFree(WOLFSSH_CTX* ctx)
{
    /* when context holds resources, free here */
    (void)ctx;

    WLOG(WS_LOG_DEBUG, "Enter CtxResourceFree()");
}


void wolfSSH_CTX_free(WOLFSSH_CTX* ctx)
{
    WLOG(WS_LOG_DEBUG, "Enter wolfSSH_CTX_free()");

    if (ctx) {
        CtxResourceFree(ctx);
        WFREE(ctx, ctx->heap, WOLFSSH_CTX_TYPE);
    }
}


static WOLFSSH* SshInit(WOLFSSH* ssh, WOLFSSH_CTX* ctx)
{
    WLOG(WS_LOG_DEBUG, "Enter SshInit()");

    if (ssh == NULL)
        return ssh;

    WMEMSET(ssh, 0, sizeof(WOLFSSH));  /* default init to zeros */

    if (ctx)
        ssh->ctx = ctx;
    else {
        WLOG(WS_LOG_ERROR, "Trying to init a wolfSSH w/o wolfSSH_CTX");
        wolfSSH_free(ssh);
        return NULL;
    }

    ssh->rfd         = -1;         /* set to invalid */
    ssh->wfd         = -1;         /* set to invalid */
    ssh->ioReadCtx   = &ssh->rfd;  /* prevent invalid access if not correctly */
    ssh->ioWriteCtx  = &ssh->wfd;  /* set */
    ssh->blockSz     = 8;
    ssh->keyExchangeId = ID_NONE;
    ssh->publicKeyId   = ID_NONE;
    ssh->encryptionId  = ID_NONE;
    ssh->integrityId   = ID_NONE;
    ssh->pendingKeyExchangeId = ID_NONE;
    ssh->pendingPublicKeyId   = ID_NONE;
    ssh->pendingEncryptionId  = ID_NONE;
    ssh->pendingIntegrityId   = ID_NONE;
    ssh->inputBuffer = BufferNew(0, ctx->heap);
    ssh->outputBuffer = BufferNew(0, ctx->heap);

    return ssh;
}


WOLFSSH* wolfSSH_new(WOLFSSH_CTX* ctx)
{
    WOLFSSH* ssh;
    void*    heap = NULL;

    if (ctx)
        heap = ctx->heap;

    WLOG(WS_LOG_DEBUG, "Enter wolfSSH_new()");

    ssh = (WOLFSSH*)WMALLOC(sizeof(WOLFSSH), heap, WOLFSSH_TYPE);
    ssh = SshInit(ssh, ctx);

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_new(), ssh = %p", ssh);

    return ssh;
}


static void SshResourceFree(WOLFSSH* ssh, void* heap)
{
    /* when ssh holds resources, free here */
    (void)heap;

    WLOG(WS_LOG_DEBUG, "Enter sshResourceFree()");
    WFREE(ssh->peerId, heap, WOLFSSH_ID_TYPE);
    BufferFree(ssh->inputBuffer);
    BufferFree(ssh->outputBuffer);
}


void wolfSSH_free(WOLFSSH* ssh)
{
    void* heap = ssh->ctx ? ssh->ctx->heap : NULL;
    WLOG(WS_LOG_DEBUG, "Enter wolfSSH_free()");

    if (ssh) {
        SshResourceFree(ssh, heap);
        WFREE(ssh, heap, WOLFSSH_TYPE);
    }
}


int wolfSSH_set_fd(WOLFSSH* ssh, int fd)
{
    WLOG(WS_LOG_DEBUG, "Enter wolfSSH_set_fd()");

    if (ssh) {
        ssh->rfd = fd;
        ssh->wfd = fd;

        ssh->ioReadCtx  = &ssh->rfd;
        ssh->ioWriteCtx = &ssh->wfd;

        return WS_SUCCESS;
    }
    return WS_BAD_ARGUMENT;
}


int wolfSSH_get_fd(const WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Enter wolfSSH_get_fd()");

    if (ssh)
        return ssh->rfd;

    return WS_BAD_ARGUMENT;
}


int wolfSSH_accept(WOLFSSH* ssh)
{
    switch (ssh->acceptState) {
        case ACCEPT_BEGIN:
            while (ssh->clientState < CLIENT_VERSION_DONE) {
                if ( (ssh->error = ProcessClientVersion(ssh)) < 0) {
                    WLOG(WS_LOG_DEBUG, "accept reply error: %d", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->acceptState = ACCEPT_CLIENT_VERSION_DONE;
            WLOG(WS_LOG_DEBUG, "accept state ACCEPT_CLIENT_VERSION_DONE");

        case ACCEPT_CLIENT_VERSION_DONE:
            SendServerVersion(ssh);
            ssh->acceptState = SERVER_VERSION_SENT;
            WLOG(WS_LOG_DEBUG, "accept state SERVER_VERSION_SENT");

        case SERVER_VERSION_SENT:
            while (ssh->clientState < CLIENT_ALGO_DONE) {
                if ( (ssh->error = ProcessReply(ssh)) < 0) {
                    WLOG(WS_LOG_DEBUG, "accept reply error: %d", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            break;
    }

    return WS_FATAL_ERROR;
}


