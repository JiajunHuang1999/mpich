/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *  (C) 2006 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 *
 *  Portions of this code were written by Intel Corporation.
 *  Copyright (C) 2011-2016 Intel Corporation.  Intel provides this material
 *  to Argonne National Laboratory subject to Software Grant and Corporate
 *  Contributor License Agreement dated February 8, 2012.
 */
#ifndef CH4R_RECVQ_H_INCLUDED
#define CH4R_RECVQ_H_INCLUDED

#include <mpidimpl.h>
#include "mpidig.h"
#include "utlist.h"
#include "ch4_impl.h"

extern unsigned PVAR_LEVEL_posted_recvq_length ATTRIBUTE((unused));
extern unsigned PVAR_LEVEL_unexpected_recvq_length ATTRIBUTE((unused));
extern unsigned long long PVAR_COUNTER_posted_recvq_match_attempts ATTRIBUTE((unused));
extern unsigned long long PVAR_COUNTER_unexpected_recvq_match_attempts ATTRIBUTE((unused));
extern MPIR_T_pvar_timer_t PVAR_TIMER_time_failed_matching_postedq ATTRIBUTE((unused));
extern MPIR_T_pvar_timer_t PVAR_TIMER_time_matching_unexpectedq ATTRIBUTE((unused));

MPL_STATIC_INLINE_PREFIX int MPIDIG_match_posted(int rank, int tag,
                                                 MPIR_Context_id_t context_id, MPIR_Request * req)
{
    return (rank == MPIDIG_REQUEST(req, rank) || MPIDIG_REQUEST(req, rank) == MPI_ANY_SOURCE) &&
        (tag == MPIR_TAG_MASK_ERROR_BITS(MPIDIG_REQUEST(req, tag)) ||
         MPIDIG_REQUEST(req, tag) == MPI_ANY_TAG) && context_id == MPIDIG_REQUEST(req, context_id);
}

MPL_STATIC_INLINE_PREFIX int MPIDIG_match_unexp(int rank, int tag,
                                                MPIR_Context_id_t context_id, MPIR_Request * req)
{
    return (rank == MPIDIG_REQUEST(req, rank) || rank == MPI_ANY_SOURCE) &&
        (tag == MPIR_TAG_MASK_ERROR_BITS(MPIDIG_REQUEST(req, tag)) ||
         tag == MPI_ANY_TAG) && context_id == MPIDIG_REQUEST(req, context_id);
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_recvq_init
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX int MPIDIG_recvq_init(void)
{
    int mpi_errno = MPI_SUCCESS;
    MPIR_T_PVAR_LEVEL_REGISTER_STATIC(RECVQ, MPI_UNSIGNED, posted_recvq_length, 0,      /* init value */
                                      MPI_T_VERBOSITY_USER_DETAIL, MPI_T_BIND_NO_OBJECT, (MPIR_T_PVAR_FLAG_READONLY | MPIR_T_PVAR_FLAG_CONTINUOUS), "CH4",      /* category name */
                                      "length of the posted message receive queue");

    MPIR_T_PVAR_LEVEL_REGISTER_STATIC(RECVQ, MPI_UNSIGNED, unexpected_recvq_length, 0,  /* init value */
                                      MPI_T_VERBOSITY_USER_DETAIL, MPI_T_BIND_NO_OBJECT, (MPIR_T_PVAR_FLAG_READONLY | MPIR_T_PVAR_FLAG_CONTINUOUS), "CH4",      /* category name */
                                      "length of the unexpected message receive queue");

    MPIR_T_PVAR_COUNTER_REGISTER_STATIC(RECVQ, MPI_UNSIGNED_LONG_LONG, posted_recvq_match_attempts, MPI_T_VERBOSITY_USER_DETAIL, MPI_T_BIND_NO_OBJECT, (MPIR_T_PVAR_FLAG_READONLY | MPIR_T_PVAR_FLAG_CONTINUOUS), "CH4",        /* category name */
                                        "number of search passes on the posted message receive queue");

    MPIR_T_PVAR_COUNTER_REGISTER_STATIC(RECVQ,
                                        MPI_UNSIGNED_LONG_LONG,
                                        unexpected_recvq_match_attempts,
                                        MPI_T_VERBOSITY_USER_DETAIL,
                                        MPI_T_BIND_NO_OBJECT,
                                        (MPIR_T_PVAR_FLAG_READONLY | MPIR_T_PVAR_FLAG_CONTINUOUS),
                                        "CH4",
                                        "number of search passes on the unexpected message receive queue");

    MPIR_T_PVAR_TIMER_REGISTER_STATIC(RECVQ, MPI_DOUBLE, time_failed_matching_postedq, MPI_T_VERBOSITY_USER_DETAIL, MPI_T_BIND_NO_OBJECT, (MPIR_T_PVAR_FLAG_READONLY | MPIR_T_PVAR_FLAG_CONTINUOUS), "CH4",     /* category name */
                                      "total time spent on unsuccessful search passes on the posted receives queue");

    MPIR_T_PVAR_TIMER_REGISTER_STATIC(RECVQ, MPI_DOUBLE, time_matching_unexpectedq, MPI_T_VERBOSITY_USER_DETAIL, MPI_T_BIND_NO_OBJECT, (MPIR_T_PVAR_FLAG_READONLY | MPIR_T_PVAR_FLAG_CONTINUOUS), "CH4",        /* category name */
                                      "total time spent on search passes on the unexpected receive queue");

    return mpi_errno;
}

#ifdef MPIDI_CH4U_USE_PER_COMM_QUEUE

#undef FUNCNAME
#define FUNCNAME MPIDIG_enqueue_posted
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX void MPIDIG_enqueue_posted(MPIR_Request * req, MPIDIG_rreq_t ** list)
{
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_ENQUEUE_POSTED);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_ENQUEUE_POSTED);
    MPIDIG_REQUEST(req, req->rreq.request) = (uint64_t) req;
    DL_APPEND(*list, &req->dev.ch4.am.req->rreq);
    MPIR_T_PVAR_LEVEL_INC(RECVQ, posted_recvq_length, 1);
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_ENQUEUE_POSTED);
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_enqueue_unexp
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX void MPIDIG_enqueue_unexp(MPIR_Request * req, MPIDIG_rreq_t ** list)
{
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_ENQUEUE_UNEXP);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_ENQUEUE_UNEXP);
    MPIDIG_REQUEST(req, req->rreq.request) = (uint64_t) req;
    DL_APPEND(*list, &req->dev.ch4.am.req->rreq);
    MPIR_T_PVAR_LEVEL_INC(RECVQ, unexpected_recvq_length, 1);
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_ENQUEUE_UNEXP);
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_delete_unexp
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX void MPIDIG_delete_unexp(MPIR_Request * req, MPIDIG_rreq_t ** list)
{
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_DELETE_UNEXP);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_DELETE_UNEXP);
    DL_DELETE(*list, &req->dev.ch4.am.req->rreq);
    MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_DELETE_UNEXP);
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_dequeue_unexp_strict
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX MPIR_Request *MPIDIG_dequeue_unexp_strict(int rank, int tag,
                                                                   MPIR_Context_id_t context_id,
                                                                   MPIDIG_rreq_t ** list)
{
    MPIDIG_rreq_t *curr, *tmp;
    MPIR_Request *req = NULL;
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_DEQUEUE_UNEXP_STRICT);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_DEQUEUE_UNEXP_STRICT);

    MPIR_T_PVAR_TIMER_START(RECVQ, time_matching_unexpectedq);
    DL_FOREACH_SAFE(*list, curr, tmp) {
        MPIR_T_PVAR_COUNTER_INC(RECVQ, unexpected_recvq_match_attempts, 1);
        req = (MPIR_Request *) curr->request;
        if (!(MPIDIG_REQUEST(req, req->status) & MPIDIG_REQ_BUSY) &&
            MPIDIG_match_unexp(rank, tag, context_id, req)) {
            DL_DELETE(*list, curr);
            MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
            break;
        }
        req = NULL;
    }
    MPIR_T_PVAR_TIMER_END(RECVQ, time_matching_unexpectedq);
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_DEQUEUE_UNEXP_STRICT);
    return req;
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_dequeue_unexp
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX MPIR_Request *MPIDIG_dequeue_unexp(int rank, int tag,
                                                            MPIR_Context_id_t context_id,
                                                            MPIDIG_rreq_t ** list)
{
    MPIDIG_rreq_t *curr, *tmp;
    MPIR_Request *req = NULL;
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_DEQUEUE_UNEXP);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_DEQUEUE_UNEXP);

    MPIR_T_PVAR_TIMER_START(RECVQ, time_matching_unexpectedq);
    DL_FOREACH_SAFE(*list, curr, tmp) {
        MPIR_T_PVAR_COUNTER_INC(RECVQ, unexpected_recvq_match_attempts, 1);
        req = (MPIR_Request *) curr->request;
        if (MPIDIG_match_unexp(rank, tag, context_id, req)) {
            DL_DELETE(*list, curr);
            MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
            break;
        }
        req = NULL;
    }
    MPIR_T_PVAR_TIMER_END(RECVQ, time_matching_unexpectedq);
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_DEQUEUE_UNEXP);
    return req;
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_find_unexp
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX MPIR_Request *MPIDIG_find_unexp(int rank, int tag,
                                                         MPIR_Context_id_t context_id,
                                                         MPIDIG_rreq_t ** list)
{
    MPIDIG_rreq_t *curr, *tmp;
    MPIR_Request *req = NULL;
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_FIND_UNEXP);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_FIND_UNEXP);

    MPIR_T_PVAR_TIMER_START(RECVQ, time_matching_unexpectedq);
    DL_FOREACH_SAFE(*list, curr, tmp) {
        MPIR_T_PVAR_COUNTER_INC(RECVQ, unexpected_recvq_match_attempts, 1);
        req = (MPIR_Request *) curr->request;
        if (MPIDIG_match_unexp(rank, tag, context_id, req)) {
            break;
        }
        req = NULL;
    }
    MPIR_T_PVAR_TIMER_END(RECVQ, time_matching_unexpectedq);
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_FIND_UNEXP);
    return req;
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_dequeue_posted
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX MPIR_Request *MPIDIG_dequeue_posted(int rank, int tag,
                                                             MPIR_Context_id_t context_id,
                                                             MPIDIG_rreq_t ** list)
{
    MPIR_Request *req = NULL;
    MPIDIG_rreq_t *curr, *tmp;
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_DEQUEUE_POSTED);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_DEQUEUE_POSTED);

    MPIR_T_PVAR_TIMER_START(RECVQ, time_failed_matching_postedq);
    DL_FOREACH_SAFE(*list, curr, tmp) {
        MPIR_T_PVAR_COUNTER_INC(RECVQ, posted_recvq_match_attempts, 1);
        req = (MPIR_Request *) curr->request;
        if (MPIDIG_match_posted(rank, tag, context_id, req)) {
            DL_DELETE(*list, curr);
            MPIR_T_PVAR_LEVEL_DEC(RECVQ, posted_recvq_length, 1);
            break;
        }
        req = NULL;
    }
    if (!req)
        MPIR_T_PVAR_TIMER_END(RECVQ, time_failed_matching_postedq);

    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_DEQUEUE_POSTED);
    return req;
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_delete_posted
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX int MPIDIG_delete_posted(MPIDIG_rreq_t * req, MPIDIG_rreq_t ** list)
{
    int found = 0;
    MPIDIG_rreq_t *curr, *tmp;
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_DELETE_POSTED);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_DELETE_POSTED);
    MPIR_T_PVAR_TIMER_START(RECVQ, time_failed_matching_postedq);
    DL_FOREACH_SAFE(*list, curr, tmp) {
        MPIR_T_PVAR_COUNTER_INC(RECVQ, posted_recvq_match_attempts, 1);
        if (curr == req) {
            DL_DELETE(*list, curr);
            found = 1;
            MPIR_T_PVAR_LEVEL_DEC(RECVQ, posted_recvq_length, 1);
            break;
        }
    }
    if (!found)
        MPIR_T_PVAR_TIMER_END(RECVQ, time_failed_matching_postedq);

    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_DELETE_POSTED);
    return found;
}

#ifdef MPIDI_CH4_ULFM

#undef FUNCNAME
#define FUNCNAME MPIDIG_recvq_clean
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX int MPIDIG_recvq_clean(MPIR_Comm * comm)
{
    int mpi_errno = MPI_SUCCESS;
    MPIR_Context_id_t context_offset = 0;
    MPIR_Request *req = NULL;
    MPIDIG_rreq_t *curr = NULL, *tmp = NULL;

    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_RECVQ_CLEAN);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_RECVQ_CLEAN);

    DL_FOREACH_SAFE(MPIDIG_COMM(comm, unexp_list), curr, tmp) {
        req = (MPIR_Request *) curr->request;
        context_offset = MPIDIG_REQUEST(req, context_id) - comm->recvcontext_id;
        if (context_offset & MPIR_CONTEXT_INTRA_COLL) {
            if (MPIDIG_REQUEST(req, tag) != MPIR_AGREE_TAG
                && MPIDIG_REQUEST(req, tag) != MPIR_SHRINK_TAG) {
                req->status.MPI_ERROR = MPI_ERR_OTHER;
                MPID_Request_complete(req);
                DL_DELETE(MPIDIG_COMM(comm, unexp_list), curr);
                MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                continue;
            }
        } else {        /* This is a p2p message */
            req->status.MPI_ERROR = MPI_ERR_OTHER;
            MPID_Request_complete(req);
            DL_DELETE(MPIDIG_COMM(comm, unexp_list), curr);
            MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
            continue;
        }
    }

    DL_FOREACH_SAFE(MPIDIG_COMM(comm, posted_list), curr, tmp) {
        req = (MPIR_Request *) curr->request;
        context_offset = MPIDIG_REQUEST(req, context_id) - comm->recvcontext_id;
        if (context_offset & MPIR_CONTEXT_INTRA_COLL) {
            if (MPIDIG_REQUEST(req, tag) != MPIR_AGREE_TAG
                && MPIDIG_REQUEST(req, tag) != MPIR_SHRINK_TAG) {
                req->status.MPI_ERROR = MPI_ERR_OTHER;
                MPID_Request_complete(req);
                DL_DELETE(MPIDIG_COMM(comm, posted_list), curr);
                MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                continue;
            }
        } else {        /* This is a p2p message */
            req->status.MPI_ERROR = MPI_ERR_OTHER;
            MPID_Request_complete(req);
            DL_DELETE(MPIDIG_COMM(comm, posted_list), curr);
            MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
            continue;
        }
    }

    if (MPIR_Comm_is_node_aware(comm)) {
        /* intranode */
        DL_FOREACH_SAFE(MPIDIG_COMM(comm->node_comm, unexp_list), curr, tmp) {
            req = (MPIR_Request *) curr->request;
            context_offset = MPIDIG_REQUEST(req, context_id)
                - comm->node_comm->recvcontext_id;
            if (context_offset & MPIR_CONTEXT_INTRA_COLL) {
                if (MPIDIG_REQUEST(req, tag) != MPIR_AGREE_TAG
                    && MPIDIG_REQUEST(req, tag) != MPIR_SHRINK_TAG) {
                    req->status.MPI_ERROR = MPI_ERR_OTHER;
                    MPID_Request_complete(req);
                    DL_DELETE(MPIDIG_COMM(comm->node_comm, unexp_list), curr);
                    MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                    continue;
                }
            } else {    /* This is a p2p message */
                req->status.MPI_ERROR = MPI_ERR_OTHER;
                MPID_Request_complete(req);
                DL_DELETE(MPIDIG_COMM(comm->node_comm, unexp_list), curr);
                MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                continue;
            }
        }
        DL_FOREACH_SAFE(MPIDIG_COMM(comm->node_roots_comm, unexp_list), curr, tmp) {
            req = (MPIR_Request *) curr->request;
            context_offset = MPIDIG_REQUEST(req, context_id)
                - comm->node_roots_comm->recvcontext_id;
            if (context_offset & MPIR_CONTEXT_INTRA_COLL) {
                if (MPIDIG_REQUEST(req, tag) != MPIR_AGREE_TAG
                    && MPIDIG_REQUEST(req, tag) != MPIR_SHRINK_TAG) {
                    req->status.MPI_ERROR = MPI_ERR_OTHER;
                    MPID_Request_complete(req);
                    DL_DELETE(MPIDIG_COMM(comm->node_roots_comm, unexp_list), curr);
                    MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                    continue;
                }
            } else {    /* This is a p2p message */
                req->status.MPI_ERROR = MPI_ERR_OTHER;
                MPID_Request_complete(req);
                DL_DELETE(MPIDIG_COMM(comm->node_roots_comm, unexp_list), curr);
                MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                continue;
            }
        }
        /* internode */
        DL_FOREACH_SAFE(MPIDIG_COMM(comm->node_comm, posted_list), curr, tmp) {
            req = (MPIR_Request *) curr->request;
            context_offset = MPIDIG_REQUEST(req, context_id)
                - comm->node_comm->recvcontext_id;
            if (context_offset & MPIR_CONTEXT_INTRA_COLL) {
                if (MPIDIG_REQUEST(req, tag) != MPIR_AGREE_TAG
                    && MPIDIG_REQUEST(req, tag) != MPIR_SHRINK_TAG) {
                    req->status.MPI_ERROR = MPI_ERR_OTHER;
                    MPID_Request_complete(req);
                    DL_DELETE(MPIDIG_COMM(comm->node_comm, posted_list), curr);
                    MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                    continue;
                }
            } else {    /* This is a p2p message */
                req->status.MPI_ERROR = MPI_ERR_OTHER;
                MPID_Request_complete(req);
                DL_DELETE(MPIDIG_COMM(comm->node_comm, posted_list), curr);
                MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                continue;
            }
        }
        DL_FOREACH_SAFE(MPIDIG_COMM(comm->node_roots_comm, posted_list), curr, tmp) {
            req = (MPIR_Request *) curr->request;
            context_offset = MPIDIG_REQUEST(req, context_id)
                - comm->node_roots_comm->recvcontext_id;
            if (context_offset & MPIR_CONTEXT_INTRA_COLL) {
                if (MPIDIG_REQUEST(req, tag) != MPIR_AGREE_TAG
                    && MPIDIG_REQUEST(req, tag) != MPIR_SHRINK_TAG) {
                    req->status.MPI_ERROR = MPI_ERR_OTHER;
                    MPID_Request_complete(req);
                    DL_DELETE(MPIDIG_COMM(comm->node_roots_comm, posted_list), curr);
                    MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                    continue;
                }
            } else {    /* This is a p2p message */
                req->status.MPI_ERROR = MPI_ERR_OTHER;
                MPID_Request_complete(req);
                DL_DELETE(MPIDIG_COMM(comm->node_roots_comm, posted_list), curr);
                MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                continue;
            }
        }
    }

  fn_exit:
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_RECVQ_CLEAN);
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

#endif

#else /* #ifdef MPIDI_CH4U_USE_PER_COMM_QUEUE */

/* Use global queue */

#undef FUNCNAME
#define FUNCNAME MPIDIG_enqueue_posted
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX void MPIDIG_enqueue_posted(MPIR_Request * req, MPIDIG_rreq_t ** list)
{
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_ENQUEUE_POSTED);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_ENQUEUE_POSTED);
    MPIDIG_REQUEST(req, req->rreq.request) = (uint64_t) req;
    DL_APPEND(MPIDI_global.posted_list, &req->dev.ch4.am.req->rreq);
    MPIR_T_PVAR_LEVEL_INC(RECVQ, posted_recvq_length, 1);
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_ENQUEUE_POSTED);
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_enqueue_unexp
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX void MPIDIG_enqueue_unexp(MPIR_Request * req, MPIDIG_rreq_t ** list)
{
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_ENQUEUE_UNEXP);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_ENQUEUE_UNEXP);
    MPIDIG_REQUEST(req, req->rreq.request) = (uint64_t) req;
    DL_APPEND(MPIDI_global.unexp_list, &req->dev.ch4.am.req->rreq);
    MPIR_T_PVAR_LEVEL_INC(RECVQ, unexpected_recvq_length, 1);
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_ENQUEUE_UNEXP);
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_delete_unexp
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX void MPIDIG_delete_unexp(MPIR_Request * req, MPIDIG_rreq_t ** list)
{
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_DELETE_UNEXP);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_DELETE_UNEXP);
    DL_DELETE(MPIDI_global.unexp_list, &req->dev.ch4.am.req->rreq);
    MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_DELETE_UNEXP);
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_dequeue_unexp_strict
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX MPIR_Request *MPIDIG_dequeue_unexp_strict(int rank, int tag,
                                                                   MPIR_Context_id_t context_id,
                                                                   MPIDIG_rreq_t ** list)
{
    MPIDIG_rreq_t *curr, *tmp;
    MPIR_Request *req = NULL;
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_DEQUEUE_UNEXP_STRICT);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_DEQUEUE_UNEXP_STRICT);

    MPIR_T_PVAR_TIMER_START(RECVQ, time_matching_unexpectedq);
    DL_FOREACH_SAFE(MPIDI_global.unexp_list, curr, tmp) {
        MPIR_T_PVAR_COUNTER_INC(RECVQ, unexpected_recvq_match_attempts, 1);
        req = (MPIR_Request *) curr->request;
        if (!(MPIDIG_REQUEST(req, req->status) & MPIDIG_REQ_BUSY) &&
            MPIDIG_match_unexp(rank, tag, context_id, req)) {
            DL_DELETE(MPIDI_global.unexp_list, curr);
            MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
            break;
        }
        req = NULL;
    }
    MPIR_T_PVAR_TIMER_END(RECVQ, time_matching_unexpectedq);
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_DEQUEUE_UNEXP_STRICT);
    return req;
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_dequeue_unexp
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX MPIR_Request *MPIDIG_dequeue_unexp(int rank, int tag,
                                                            MPIR_Context_id_t context_id,
                                                            MPIDIG_rreq_t ** list)
{
    MPIDIG_rreq_t *curr, *tmp;
    MPIR_Request *req = NULL;
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_DEQUEUE_UNEXP);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_DEQUEUE_UNEXP);

    MPIR_T_PVAR_TIMER_START(RECVQ, time_matching_unexpectedq);
    DL_FOREACH_SAFE(MPIDI_global.unexp_list, curr, tmp) {
        MPIR_T_PVAR_COUNTER_INC(RECVQ, unexpected_recvq_match_attempts, 1);
        req = (MPIR_Request *) curr->request;
        if (MPIDIG_match_unexp(rank, tag, context_id, req)) {
            DL_DELETE(MPIDI_global.unexp_list, curr);
            MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
            break;
        }
        req = NULL;
    }
    MPIR_T_PVAR_TIMER_END(RECVQ, time_matching_unexpectedq);
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_DEQUEUE_UNEXP);
    return req;
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_find_unexp
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX MPIR_Request *MPIDIG_find_unexp(int rank, int tag,
                                                         MPIR_Context_id_t context_id,
                                                         MPIDIG_rreq_t ** list)
{
    MPIDIG_rreq_t *curr, *tmp;
    MPIR_Request *req = NULL;
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_FIND_UNEXP);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_FIND_UNEXP);

    MPIR_T_PVAR_TIMER_START(RECVQ, time_matching_unexpectedq);
    DL_FOREACH_SAFE(MPIDI_global.unexp_list, curr, tmp) {
        MPIR_T_PVAR_COUNTER_INC(RECVQ, unexpected_recvq_match_attempts, 1);
        req = (MPIR_Request *) curr->request;
        if (MPIDIG_match_unexp(rank, tag, context_id, req)) {
            break;
        }
        req = NULL;
    }
    MPIR_T_PVAR_TIMER_END(RECVQ, time_matching_unexpectedq);
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_FIND_UNEXP);
    return req;
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_dequeue_posted
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX MPIR_Request *MPIDIG_dequeue_posted(int rank, int tag,
                                                             MPIR_Context_id_t context_id,
                                                             MPIDIG_rreq_t ** list)
{
    MPIR_Request *req = NULL;
    MPIDIG_rreq_t *curr, *tmp;
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_DEQUEUE_POSTED);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_DEQUEUE_POSTED);

    MPIR_T_PVAR_TIMER_START(RECVQ, time_failed_matching_postedq);
    DL_FOREACH_SAFE(MPIDI_global.posted_list, curr, tmp) {
        MPIR_T_PVAR_COUNTER_INC(RECVQ, posted_recvq_match_attempts, 1);
        req = (MPIR_Request *) curr->request;
        if (MPIDIG_match_posted(rank, tag, context_id, req)) {
            DL_DELETE(MPIDI_global.posted_list, curr);
            MPIR_T_PVAR_LEVEL_DEC(RECVQ, posted_recvq_length, 1);
            break;
        }
        req = NULL;
    }
    if (!req)
        MPIR_T_PVAR_TIMER_END(RECVQ, time_failed_matching_postedq);

    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_DEQUEUE_POSTED);
    return req;
}

#undef FUNCNAME
#define FUNCNAME MPIDIG_delete_posted
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX int MPIDIG_delete_posted(MPIDIG_rreq_t * req, MPIDIG_rreq_t ** list)
{
    int found = 0;
    MPIDIG_rreq_t *curr, *tmp;
    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_DELETE_POSTED);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_DELETE_POSTED);
    MPIR_T_PVAR_TIMER_START(RECVQ, time_failed_matching_postedq);
    DL_FOREACH_SAFE(MPIDI_global.posted_list, curr, tmp) {
        MPIR_T_PVAR_COUNTER_INC(RECVQ, posted_recvq_match_attempts, 1);
        if (curr == req) {
            DL_DELETE(MPIDI_global.posted_list, curr);
            found = 1;
            MPIR_T_PVAR_LEVEL_DEC(RECVQ, posted_recvq_length, 1);
            break;
        }
    }
    if (!found)
        MPIR_T_PVAR_TIMER_END(RECVQ, time_failed_matching_postedq);

    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDIG_DELETE_POSTED);
    return found;
}

#ifdef MPIDI_CH4_ULFM

#undef FUNCNAME
#define FUNCNAME MPIDIG_recvq_clean
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX int MPIDIG_recvq_clean(MPIR_Comm * comm)
{
    int mpi_errno = MPI_SUCCESS;
    MPIR_Context_id_t context_offset = 0;
    MPIR_Request *req = NULL;
    MPIDIG_rreq_t *curr = NULL, *tmp = NULL;

    MPIR_FUNC_VERBOSE_STATE_DECL(MPID_STATE_MPIDIG_RECVQ_CLEAN);
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_RECVQ_CLEAN);

    DL_FOREACH_SAFE(MPIDI_global.unexp_list, curr, tmp) {
        req = (MPIR_Request *) curr->request;
        context_offset = MPIDIG_REQUEST(req, context_id) - comm->recvcontext_id;

        if (context_offset & MPIR_CONTEXT_INTRA_COLL) {
            if (MPIDIG_REQUEST(req, tag) != MPIR_AGREE_TAG
                && MPIDIG_REQUEST(req, tag) != MPIR_SHRINK_TAG) {
                MPL_DBG_MSG_FMT(MPIDI_CH4_DBG_COMM, VERBOSE,
                                (MPL_DBG_FDEST,
                                 "revoke unexp coll pkt rank=%d tag=%d contextid=%d",
                                 MPIDIG_COMM(req, rank), MPIDIG_COMM(req, tag),
                                 MPIDIG_COMM(req, context_id)));
                req->status.MPI_ERROR = MPI_ERR_OTHER;
                MPID_Request_complete(req);
                DL_DELETE(MPIDI_global.unexp_list, curr);
                MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                continue;
            }
        } else {
            MPL_DBG_MSG_FMT(MPIDI_CH4_DBG_COMM, VERBOSE,
                            (MPL_DBG_FDEST,
                             "revoke unexp pt2pt pkt rank=%d tag=%d contextid=%d",
                             MPIDIG_COMM(req, rank), MPIDIG_COMM(req, tag),
                             MPIDIG_COMM(req, context_id)));
            req->status.MPI_ERROR = MPI_ERR_OTHER;
            MPID_Request_complete(req);
            DL_DELETE(MPIDI_global.unexp_list, curr);
            MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
            continue;
        }

        if (MPIR_Comm_is_node_aware(comm)) {
            int offset;
            context_offset = MPIDIG_REQUEST(req, context_id) - comm->recvcontext_id;
            offset = (comm->comm_kind == MPIR_COMM_KIND__INTRACOMM) ? MPIR_CONTEXT_INTRA_PT2PT
                : MPIR_CONTEXT_INTER_PT2PT;
            if (context_offset == MPIR_CONTEXT_INTRANODE_OFFSET + offset) {
                MPL_DBG_MSG_FMT(MPIDI_CH4_DBG_COMM, VERBOSE,
                                (MPL_DBG_FDEST,
                                 "revoke unexp internode pt2pt pkt rank=%d tag=%d contextid=%d",
                                 MPIDIG_COMM(req, rank), MPIDIG_COMM(req, tag),
                                 MPIDIG_COMM(req, context_id)));
                req->status.MPI_ERROR = MPI_ERR_OTHER;
                MPID_Request_complete(req);
                DL_DELETE(MPIDI_global.unexp_list, curr);
                MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                continue;
            }
            offset = (comm->comm_kind == MPIR_COMM_KIND__INTRACOMM) ? MPIR_CONTEXT_INTRA_COLL
                : MPIR_CONTEXT_INTER_COLL;
            if (context_offset == MPIR_CONTEXT_INTRANODE_OFFSET + offset) {
                if (MPIDIG_REQUEST(req, tag) != MPIR_AGREE_TAG
                    && MPIDIG_REQUEST(req, tag) != MPIR_SHRINK_TAG) {
                    MPL_DBG_MSG_FMT(MPIDI_CH4_DBG_COMM, VERBOSE,
                                    (MPL_DBG_FDEST,
                                     "revoke unexp internode coll pkt rank=%d tag=%d contextid=%d",
                                     MPIDIG_COMM(req, rank), MPIDIG_COMM(req, tag),
                                     MPIDIG_COMM(req, context_id)));
                    req->status.MPI_ERROR = MPI_ERR_OTHER;
                    MPID_Request_complete(req);
                    DL_DELETE(MPIDI_global.unexp_list, curr);
                    MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                    continue;
                }
            }
            offset = (comm->comm_kind == MPIR_COMM_KIND__INTRACOMM) ? MPIR_CONTEXT_INTRA_PT2PT
                : MPIR_CONTEXT_INTER_PT2PT;
            if (context_offset == MPIR_CONTEXT_INTERNODE_OFFSET + offset) {
                MPL_DBG_MSG_FMT(MPIDI_CH4_DBG_COMM, VERBOSE,
                                (MPL_DBG_FDEST,
                                 "revoke unexp intranode pt2pt pkt rank=%d tag=%d contextid=%d",
                                 MPIDIG_COMM(req, rank), MPIDIG_COMM(req, tag),
                                 MPIDIG_COMM(req, context_id)));
                req->status.MPI_ERROR = MPI_ERR_OTHER;
                MPID_Request_complete(req);
                DL_DELETE(MPIDI_global.unexp_list, curr);
                MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                continue;
            }
            offset = (comm->comm_kind == MPIR_COMM_KIND__INTRACOMM) ? MPIR_CONTEXT_INTRA_COLL
                : MPIR_CONTEXT_INTER_COLL;
            if (context_offset == MPIR_CONTEXT_INTERNODE_OFFSET + offset) {
                if (MPIDIG_REQUEST(req, tag) != MPIR_AGREE_TAG
                    && MPIDIG_REQUEST(req, tag) != MPIR_SHRINK_TAG) {
                    MPL_DBG_MSG_FMT(MPIDI_CH4_DBG_COMM, VERBOSE,
                                    (MPL_DBG_FDEST,
                                     "revoke unexp intranode coll pkt rank=%d tag=%d contextid=%d",
                                     MPIDIG_COMM(req, rank), MPIDIG_COMM(req, tag),
                                     MPIDIG_COMM(req, context_id)));
                    req->status.MPI_ERROR = MPI_ERR_OTHER;
                    MPID_Request_complete(req);
                    DL_DELETE(MPIDI_global.unexp_list, curr);
                    MPIR_T_PVAR_LEVEL_DEC(RECVQ, unexpected_recvq_length, 1);
                    continue;
                }
            }
        }
    }

    DL_FOREACH_SAFE(MPIDI_global.posted_list, curr, tmp) {
        req = (MPIR_Request *) curr->request;
        context_offset = MPIDIG_REQUEST(req, context_id) - comm->recvcontext_id;

        if (context_offset == MPIR_CONTEXT_INTRA_PT2PT) {
            MPL_DBG_MSG_FMT(MPIDI_CH4_DBG_COMM, VERBOSE,
                            (MPL_DBG_FDEST,
                             "revoke posted pt2pt pkt rank=%d tag=%d contextid=%d",
                             MPIDIG_COMM(req, rank), MPIDIG_COMM(req, tag),
                             MPIDIG_COMM(req, context_id)));
            req->status.MPI_ERROR = MPI_ERR_OTHER;
            MPID_Request_complete(req);
            DL_DELETE(MPIDI_global.posted_list, curr);
            MPIR_T_PVAR_LEVEL_DEC(RECVQ, posted_recvq_length, 1);
            continue;
        } else if (context_offset == MPIR_CONTEXT_INTRA_COLL) {
            if (MPIDIG_REQUEST(req, tag) != MPIR_AGREE_TAG
                && MPIDIG_REQUEST(req, tag) != MPIR_SHRINK_TAG) {
                MPL_DBG_MSG_FMT(MPIDI_CH4_DBG_COMM, VERBOSE,
                                (MPL_DBG_FDEST,
                                 "revoke posted coll pkt rank=%d tag=%d contextid=%d",
                                 MPIDIG_COMM(req, rank), MPIDIG_COMM(req, tag),
                                 MPIDIG_COMM(req, context_id)));
                req->status.MPI_ERROR = MPI_ERR_OTHER;
                MPID_Request_complete(req);
                DL_DELETE(MPIDI_global.posted_list, curr);
                MPIR_T_PVAR_LEVEL_DEC(RECVQ, posted_recvq_length, 1);
                continue;
            }
        }

        if (MPIR_Comm_is_node_aware(comm)) {
            int offset;
            context_offset = MPIDIG_REQUEST(req, context_id) - comm->recvcontext_id;
            offset = (comm->comm_kind == MPIR_COMM_KIND__INTRACOMM) ? MPIR_CONTEXT_INTRA_PT2PT
                : MPIR_CONTEXT_INTER_PT2PT;
            if (context_offset == MPIR_CONTEXT_INTRANODE_OFFSET + offset) {
                MPL_DBG_MSG_FMT(MPIDI_CH4_DBG_COMM, VERBOSE,
                                (MPL_DBG_FDEST,
                                 "revoke posted intranode pt2pt pkt rank=%d tag=%d contextid=%d",
                                 MPIDIG_COMM(req, rank), MPIDIG_COMM(req, tag),
                                 MPIDIG_COMM(req, context_id)));
                req->status.MPI_ERROR = MPI_ERR_OTHER;
                MPID_Request_complete(req);
                DL_DELETE(MPIDI_global.posted_list, curr);
                MPIR_T_PVAR_LEVEL_DEC(RECVQ, posted_recvq_length, 1);
                continue;
            }
            offset = (comm->comm_kind == MPIR_COMM_KIND__INTRACOMM) ? MPIR_CONTEXT_INTRA_COLL
                : MPIR_CONTEXT_INTER_COLL;
            if (context_offset == MPIR_CONTEXT_INTRANODE_OFFSET + offset) {
                if (MPIDIG_REQUEST(req, tag) != MPIR_AGREE_TAG
                    && MPIDIG_REQUEST(req, tag) != MPIR_SHRINK_TAG) {
                    MPL_DBG_MSG_FMT(MPIDI_CH4_DBG_COMM, VERBOSE,
                                    (MPL_DBG_FDEST,
                                     "revoke posted intranode coll pkt rank=%d tag=%d contextid=%d",
                                     MPIDIG_COMM(req, rank), MPIDIG_COMM(req, tag),
                                     MPIDIG_COMM(req, context_id)));
                    req->status.MPI_ERROR = MPI_ERR_OTHER;
                    MPID_Request_complete(req);
                    DL_DELETE(MPIDI_global.posted_list, curr);
                    MPIR_T_PVAR_LEVEL_DEC(RECVQ, posted_recvq_length, 1);
                    continue;
                }
            }
            offset = (comm->comm_kind == MPIR_COMM_KIND__INTRACOMM) ? MPIR_CONTEXT_INTRA_PT2PT
                : MPIR_CONTEXT_INTER_PT2PT;
            if (context_offset == MPIR_CONTEXT_INTERNODE_OFFSET + offset) {
                MPL_DBG_MSG_FMT(MPIDI_CH4_DBG_COMM, VERBOSE,
                                (MPL_DBG_FDEST,
                                 "revoke posted internode pt2pt pkt rank=%d tag=%d contextid=%d",
                                 MPIDIG_COMM(req, rank), MPIDIG_COMM(req, tag),
                                 MPIDIG_COMM(req, context_id)));
                req->status.MPI_ERROR = MPI_ERR_OTHER;
                MPID_Request_complete(req);
                DL_DELETE(MPIDI_global.posted_list, curr);
                MPIR_T_PVAR_LEVEL_DEC(RECVQ, posted_recvq_length, 1);
                continue;
            }
            offset = (comm->comm_kind == MPIR_COMM_KIND__INTRACOMM) ? MPIR_CONTEXT_INTRA_COLL
                : MPIR_CONTEXT_INTER_COLL;
            if (context_offset == MPIR_CONTEXT_INTERNODE_OFFSET + offset) {
                if (MPIDIG_REQUEST(req, tag) != MPIR_AGREE_TAG
                    && MPIDIG_REQUEST(req, tag) != MPIR_SHRINK_TAG) {
                    MPL_DBG_MSG_FMT(MPIDI_CH4_DBG_COMM, VERBOSE,
                                    (MPL_DBG_FDEST,
                                     "revoke posted internode coll pkt rank=%d tag=%d contextid=%d",
                                     MPIDIG_COMM(req, rank), MPIDIG_COMM(req, tag),
                                     MPIDIG_COMM(req, context_id)));
                    req->status.MPI_ERROR = MPI_ERR_OTHER;
                    MPID_Request_complete(req);
                    DL_DELETE(MPIDI_global.posted_list, curr);
                    MPIR_T_PVAR_LEVEL_DEC(RECVQ, posted_recvq_length, 1);
                    continue;
                }
            }
        }
    }

  fn_exit:
    MPIR_FUNC_VERBOSE_ENTER(MPID_STATE_MPIDIG_RECVQ_CLEAN);
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

#endif

#endif /* MPIDI_CH4U_USE_PER_COMM_QUEUE */

#endif /* CH4R_RECVQ_H_INCLUDED */
