/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *  Copyright (C) by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 *
 */

#include "mpiimpl.h"
#include "recexchalgo.h"
#include "algo_common.h"


int MPIR_Allreduce_intra_recexch(const void *sendbuf,
                                 void *recvbuf,
                                 MPI_Aint count,
                                 MPI_Datatype datatype,
                                 MPI_Op op, MPIR_Comm * comm, int k, int single_phase_recv,
                                 MPIR_Errflag_t * errflag)
{
    int mpi_errno = MPI_SUCCESS, mpi_errno_ret = MPI_SUCCESS;
    int is_commutative, rank, nranks, nbr, myidx;
    int buf = 0;
    MPI_Aint true_extent, true_lb, extent;
    int step1_sendto = -1, step1_nrecvs = 0, *step1_recvfrom = NULL;
    int step2_nphases = 0, **step2_nbrs;
    int i, j, phase, p_of_k, T;
    bool in_step2 = true;
    void **nbr_buffer = NULL;
    int index = 0, comm_alloc = 1;
    MPIR_Request *sreqs[MAX_RADIX], *rreqs[MAX_RADIX * 2];
    MPIR_Request **send_reqs = NULL, **recv_reqs = NULL;
    int send_nreq = 0, recv_nreq = 0, total_phases = 0;

    rank = comm->rank;
    nranks = comm->local_size;
    printf("Process %d is running the MPIR_Allreduce_intra_recexch\n", rank);
    fflush(stdout);
    is_commutative = MPIR_Op_is_commutative(op);

    /* if there is only 1 rank, copy data from sendbuf
     * to recvbuf and exit */
    if (nranks == 1) {
        if (sendbuf != MPI_IN_PLACE)
            mpi_errno = MPIR_Localcopy(sendbuf, count, datatype, recvbuf, count, datatype);
        return mpi_errno;
    }

    /* need to allocate temporary buffer to store incoming data */
    MPIR_Type_get_true_extent_impl(datatype, &true_lb, &true_extent);
    MPIR_Datatype_get_extent_macro(datatype, extent);
    extent = MPL_MAX(extent, true_extent);

    /* copy local data into recvbuf */
    if (sendbuf != MPI_IN_PLACE && count > 0) {
        mpi_errno = MPIR_Localcopy(sendbuf, count, datatype, recvbuf, count, datatype);
        if (mpi_errno)
            MPIR_ERR_POP(mpi_errno);
    }

    /* If k value is greater than the maximum radix for which nbrs
     * are stored in comm, generate nbrs here in the function. Otherwise,
     * get the nbrs information stored in the communicator */
    if (k > MAX_RADIX) {
        comm_alloc = 0;
        /* get the neighbors, the function allocates the required memory */
        MPII_Recexchalgo_get_neighbors(rank, nranks, &k, &step1_sendto,
                                       &step1_recvfrom, &step1_nrecvs,
                                       &step2_nbrs, &step2_nphases, &p_of_k, &T);
        /* Allocate requests */
        if (step1_sendto == -1) {
            recv_reqs =
                (MPIR_Request **) MPL_malloc((k - 1) * 2 * sizeof(MPIR_Request *), MPL_MEM_BUFFER);
            MPIR_ERR_CHKANDJUMP(!recv_reqs, mpi_errno, MPI_ERR_OTHER, "**nomem");
            send_reqs =
                (MPIR_Request **) MPL_malloc((k - 1) * sizeof(MPIR_Request *), MPL_MEM_BUFFER);
            MPIR_ERR_CHKANDJUMP(!send_reqs, mpi_errno, MPI_ERR_OTHER, "**nomem");
        }
    } else {
        index = k - 2;  /* we calculate starting k = 2 */
        /* If this is the first time a recursive exchange is called with k, populate the nbr calculation in
         * the communicator */
        if (comm->coll.nbrs_defined[index] == 0) {
            comm->coll.nbrs_defined[index] = 1;
            comm->coll.k[index] = k;
            comm->coll.step1_sendto[index] = -1;
            comm->coll.step1_nrecvs[index] = 0;
            comm->coll.step2_nphases[index] = 0;
            mpi_errno = MPII_Recexchalgo_get_neighbors(rank, nranks, &comm->coll.k[index],
                                                       &comm->coll.step1_sendto[index],
                                                       &comm->coll.step1_recvfrom[index],
                                                       &comm->coll.step1_nrecvs[index],
                                                       &comm->coll.step2_nbrs[index],
                                                       &comm->coll.step2_nphases[index],
                                                       &comm->coll.pofk[index], &T);
        }
        k = comm->coll.k[index];
        p_of_k = comm->coll.pofk[index];
        step1_sendto = comm->coll.step1_sendto[index];
        step1_recvfrom = comm->coll.step1_recvfrom[index];
        step1_nrecvs = comm->coll.step1_nrecvs[index];
        step2_nbrs = comm->coll.step2_nbrs[index];
        step2_nphases = comm->coll.step2_nphases[index];

        recv_reqs = rreqs;
        send_reqs = sreqs;
    }
    /* lazy allocation of 8K recv buffer. If the message size is 8K and if the buffer has not been
     * allocated before, do a lazy allocation here. We do this allocation here since the K value could be updated
     * after the get_nbrs computation */
    if (k <= MAX_RADIX && (step1_sendto == -1) && count * extent <= 8192 &&
        comm->coll.recexch_allreduce_nbr_buffer == NULL) {
        comm->coll.recexch_allreduce_nbr_buffer =
            (void **) MPL_malloc(sizeof(void *) * 2 * (MAX_RADIX - 1), MPL_MEM_COLL);
        MPIR_ERR_CHKANDJUMP(!comm->coll.recexch_allreduce_nbr_buffer, mpi_errno, MPI_ERR_OTHER,
                            "**nomem");
        for (j = 0; j < 2 * (MAX_RADIX - 1); j++) {
            /* allocate buffer for 8K */
            comm->coll.recexch_allreduce_nbr_buffer[j] = MPL_malloc(8192, MPL_MEM_COLL);
            MPIR_ERR_CHKANDJUMP(!comm->coll.recexch_allreduce_nbr_buffer[j], mpi_errno,
                                MPI_ERR_OTHER, "**nomem");
        }
    }
    if (k <= MAX_RADIX && step1_sendto == -1 && count * extent <= 8192) {
        nbr_buffer = comm->coll.recexch_allreduce_nbr_buffer;
    }

    /* get nearest power-of-two less than or equal to comm_size */
    in_step2 = (step1_sendto == -1);    /* whether this rank participates in Step 2 */

    if (in_step2 && (k > MAX_RADIX || count * extent > 8192)) {
        if (single_phase_recv) {        /* create recvs for single phase */
            nbr_buffer = (void **) MPL_malloc(sizeof(void *) * (k - 1), MPL_MEM_COLL);
            MPIR_ERR_CHKANDJUMP(!nbr_buffer, mpi_errno, MPI_ERR_OTHER, "**nomem");
            for (j = 0; j < (k - 1); j++) {
                nbr_buffer[j] = MPL_malloc(count * extent, MPL_MEM_COLL);
                MPIR_ERR_CHKANDJUMP(!nbr_buffer[j], mpi_errno, MPI_ERR_OTHER, "**nomem");
                nbr_buffer[j] = (void *) ((char *) nbr_buffer[j] - true_lb);
            }
        } else {        /* create buffers for 2 phases */
            nbr_buffer = (void **) MPL_malloc(sizeof(void *) * 2 * (k - 1), MPL_MEM_COLL);
            MPIR_ERR_CHKANDJUMP(!nbr_buffer, mpi_errno, MPI_ERR_OTHER, "**nomem");
            for (j = 0; j < 2 * (k - 1); j++) {
                nbr_buffer[j] = MPL_malloc(count * extent, MPL_MEM_COLL);
                MPIR_ERR_CHKANDJUMP(!nbr_buffer[j], mpi_errno, MPI_ERR_OTHER, "**nomem");
                nbr_buffer[j] = (void *) ((char *) nbr_buffer[j] - true_lb);
            }
        }
    }

    if (!in_step2) {    /* even */
        /* non-participating rank sends the data to a participating rank */
        mpi_errno = MPIC_Send(recvbuf, count,
                              datatype, step1_sendto, MPIR_ALLREDUCE_TAG, comm, errflag);
        if (mpi_errno) {
            /* for communication errors, just record the error but continue */
            *errflag =
                MPIX_ERR_PROC_FAILED ==
                MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED : MPIR_ERR_OTHER;
            MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
            MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
        }

    } else {    /* odd */
        if (step1_nrecvs) {
            for (i = 0; i < step1_nrecvs; i++) {        /* participating rank gets data from non-partcipating ranks */
                mpi_errno = MPIC_Irecv(nbr_buffer[i], count,
                                       datatype, step1_recvfrom[i],
                                       MPIR_ALLREDUCE_TAG, comm, &recv_reqs[recv_nreq++]);
                if (mpi_errno) {
                    /* for communication errors, just record the error but continue */
                    *errflag =
                        MPIX_ERR_PROC_FAILED ==
                        MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED : MPIR_ERR_OTHER;
                    MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                    MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
                }
            }
            mpi_errno = MPIC_Waitall(recv_nreq, recv_reqs, MPI_STATUSES_IGNORE, errflag);
            if (mpi_errno && mpi_errno != MPI_ERR_IN_STATUS)
                MPIR_ERR_POP(mpi_errno);

            /* Do reduction of reduced data */
            for (i = 0; i < step1_nrecvs && count > 0; i++) {   /* participating rank gets data from non-partcipating ranks */
                mpi_errno = MPIR_Reduce_local(nbr_buffer[i], recvbuf, count, datatype, op);
                if (mpi_errno) {
                    /* for communication errors, just record the error but continue */
                    *errflag =
                        MPIX_ERR_PROC_FAILED ==
                        MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED : MPIR_ERR_OTHER;
                    MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                    MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
                }
            }
        }
    }


    /* step2 */
    if (!is_commutative && in_step2 && count > 0) {
        /* sort the neighbor list so that receives can be posted in order */
        for (phase = 0; phase < step2_nphases; phase++)
            qsort(step2_nbrs[phase], k - 1, sizeof(int), MPII_Algo_compare_int);
    }

    /* step2 sends and reduces */
    for (phase = 0; phase < step2_nphases && in_step2; phase++) {
        buf = 0;
        recv_nreq = 0;
        total_phases = (single_phase_recv) ? 1 : 2;
        for (j = 0; j < total_phases && (j + phase) < step2_nphases; j++) {
            for (i = 0; i < (k - 1); i++) {
                nbr = step2_nbrs[phase + j][i];
                mpi_errno =
                    MPIC_Irecv(nbr_buffer[buf++], count, datatype, nbr, MPIR_ALLREDUCE_TAG,
                               comm, &recv_reqs[recv_nreq++]);
                if (mpi_errno) {
                    /* for communication errors, just record the error but continue */
                    *errflag =
                        MPIX_ERR_PROC_FAILED ==
                        MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED : MPIR_ERR_OTHER;
                    MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                    MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
                }
            }
        }

        send_nreq = 0;
        myidx = 0;
        /* send data to all the neighbors */
        for (i = 0; i < k - 1; i++) {
            nbr = step2_nbrs[phase][i];
            mpi_errno = MPIC_Isend(recvbuf, count, datatype, nbr, MPIR_ALLREDUCE_TAG, comm,
                                   &send_reqs[send_nreq++], errflag);
            if (mpi_errno) {
                /* for communication errors, just record the error but continue */
                *errflag =
                    MPIX_ERR_PROC_FAILED ==
                    MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED : MPIR_ERR_OTHER;
                MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
            }
            if (rank > nbr) {
                myidx = i + 1;
            }
        }

        mpi_errno = MPIC_Waitall(send_nreq, send_reqs, MPI_STATUSES_IGNORE, errflag);
        if (mpi_errno && mpi_errno != MPI_ERR_IN_STATUS)
            MPIR_ERR_POP(mpi_errno);

        buf = myidx - 1;
        mpi_errno = MPIC_Waitall((k - 1), recv_reqs, MPI_STATUSES_IGNORE, errflag);
        if (mpi_errno && mpi_errno != MPI_ERR_IN_STATUS)
            MPIR_ERR_POP(mpi_errno);

        for (i = myidx - 1; i >= 0 && count > 0; i--, buf--) {
            mpi_errno = MPIR_Reduce_local(nbr_buffer[buf], recvbuf, count, datatype, op);
            if (mpi_errno) {
                /* for communication errors, just record the error but continue */
                *errflag =
                    MPIX_ERR_PROC_FAILED ==
                    MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED : MPIR_ERR_OTHER;
                MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
            }
        }

        buf = myidx;
        for (i = myidx; i < k - 1 && count > 0; i++, buf++) {
            if (is_commutative) {
                mpi_errno = MPIR_Reduce_local(nbr_buffer[buf], recvbuf, count, datatype, op);
                if (mpi_errno) {
                    /* for communication errors, just record the error but continue */
                    *errflag =
                        MPIX_ERR_PROC_FAILED ==
                        MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED : MPIR_ERR_OTHER;
                    MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                    MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
                }
            } else {
                mpi_errno = MPIR_Reduce_local(recvbuf, nbr_buffer[buf], count, datatype, op);
                if (mpi_errno) {
                    /* for communication errors, just record the error but continue */
                    *errflag =
                        MPIX_ERR_PROC_FAILED ==
                        MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED : MPIR_ERR_OTHER;
                    MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                    MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
                }

                mpi_errno =
                    MPIR_Localcopy(nbr_buffer[buf], count, datatype, recvbuf, count, datatype);
                if (mpi_errno) {
                    /* for communication errors, just record the error but continue */
                    *errflag =
                        MPIX_ERR_PROC_FAILED ==
                        MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED : MPIR_ERR_OTHER;
                    MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                    MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
                }
            }
        }

        if (single_phase_recv == false) {       /* post sends and do reduction for the 2nd phase */
            phase++;
            if (phase < step2_nphases) {
                send_nreq = 0;
                myidx = 0;
                /* send data to all the neighbors */
                for (i = 0; i < k - 1; i++) {
                    nbr = step2_nbrs[phase][i];

                    mpi_errno =
                        MPIC_Isend(recvbuf, count, datatype, nbr, MPIR_ALLREDUCE_TAG, comm,
                                   &send_reqs[send_nreq++], errflag);
                    if (mpi_errno) {
                        /* for communication errors, just record the error but continue */
                        *errflag =
                            MPIX_ERR_PROC_FAILED ==
                            MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED : MPIR_ERR_OTHER;
                        MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                        MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
                    }
                    if (rank > nbr) {
                        myidx = i + 1;
                    }
                }

                mpi_errno = MPIC_Waitall(send_nreq, send_reqs, MPI_STATUSES_IGNORE, errflag);
                if (mpi_errno && mpi_errno != MPI_ERR_IN_STATUS)
                    MPIR_ERR_POP(mpi_errno);

                mpi_errno =
                    MPIC_Waitall((k - 1), recv_reqs + (k - 1), MPI_STATUSES_IGNORE, errflag);
                if (mpi_errno && mpi_errno != MPI_ERR_IN_STATUS)
                    MPIR_ERR_POP(mpi_errno);

                buf = (k - 1) + myidx - 1;
                for (i = myidx - 1; i >= 0 && count > 0; i--, buf--) {
                    mpi_errno = MPIR_Reduce_local(nbr_buffer[buf], recvbuf, count, datatype, op);
                    if (mpi_errno) {
                        /* for communication errors, just record the error but continue */
                        *errflag =
                            MPIX_ERR_PROC_FAILED ==
                            MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED : MPIR_ERR_OTHER;
                        MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                        MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
                    }
                }

                buf = (k - 1) + myidx;
                for (i = myidx; i < k - 1 && count > 0; i++, buf++) {
                    if (is_commutative) {
                        mpi_errno =
                            MPIR_Reduce_local(nbr_buffer[buf], recvbuf, count, datatype, op);
                        if (mpi_errno) {
                            /* for communication errors, just record the error but continue */
                            *errflag =
                                MPIX_ERR_PROC_FAILED ==
                                MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED :
                                MPIR_ERR_OTHER;
                            MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                            MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
                        }
                    } else {
                        mpi_errno =
                            MPIR_Reduce_local(recvbuf, nbr_buffer[buf], count, datatype, op);
                        if (mpi_errno) {
                            /* for communication errors, just record the error but continue */
                            *errflag =
                                MPIX_ERR_PROC_FAILED ==
                                MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED :
                                MPIR_ERR_OTHER;
                            MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                            MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
                        }
                        mpi_errno =
                            MPIR_Localcopy(nbr_buffer[buf], count, datatype, recvbuf, count,
                                           datatype);
                        if (mpi_errno) {
                            /* for communication errors, just record the error but continue */
                            *errflag =
                                MPIX_ERR_PROC_FAILED ==
                                MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED :
                                MPIR_ERR_OTHER;
                            MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                            MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
                        }
                    }
                }
            }
        }
    }

    /* Step 3: This is reverse of Step 1. Rans that participated in Step 2
     * send the data to non-partcipating rans */
    if (step1_sendto != -1) {   /* I am a Step 2 non-participating rank */
        mpi_errno = MPIC_Recv(recvbuf, count, datatype, step1_sendto, MPIR_ALLREDUCE_TAG, comm,
                              MPI_STATUS_IGNORE, errflag);
        if (mpi_errno) {
            /* for communication errors, just record the error but continue */
            *errflag =
                MPIX_ERR_PROC_FAILED ==
                MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED : MPIR_ERR_OTHER;
            MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
            MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
        }
    } else {
        for (i = 0; i < step1_nrecvs; i++) {
            mpi_errno =
                MPIC_Isend(recvbuf, count, datatype, step1_recvfrom[i], MPIR_ALLREDUCE_TAG,
                           comm, &send_reqs[i], errflag);
            if (mpi_errno) {
                /* for communication errors, just record the error but continue */
                *errflag =
                    MPIX_ERR_PROC_FAILED ==
                    MPIR_ERR_GET_CLASS(mpi_errno) ? MPIR_ERR_PROC_FAILED : MPIR_ERR_OTHER;
                MPIR_ERR_SET(mpi_errno, *errflag, "**fail");
                MPIR_ERR_ADD(mpi_errno_ret, mpi_errno);
            }
        }

        mpi_errno = MPIC_Waitall(i, send_reqs, MPI_STATUSES_IGNORE, errflag);
        if (mpi_errno && mpi_errno != MPI_ERR_IN_STATUS)
            MPIR_ERR_POP(mpi_errno);
    }

    if (in_step2 && (k > MAX_RADIX || count * extent > 8192)) {
        if (single_phase_recv == true) {
            for (j = 0; j < (k - 1); j++) {
                nbr_buffer[j] = (void *) ((char *) nbr_buffer[j] + true_lb);
                MPL_free(nbr_buffer[j]);
            }
        } else {
            for (j = 0; j < 2 * (k - 1); j++) {
                nbr_buffer[j] = (void *) ((char *) nbr_buffer[j] + true_lb);
                MPL_free(nbr_buffer[j]);
            }
        }
        MPL_free(nbr_buffer);
    }

    if (!comm_alloc) {
        /* free the memory */
        for (i = 0; i < step2_nphases; i++)
            MPL_free(step2_nbrs[i]);
        MPL_free(step2_nbrs);
        MPL_free(step1_recvfrom);
        if (in_step2) {
            MPL_free(recv_reqs);
            MPL_free(send_reqs);
        }
    }
  fn_exit:
    if (mpi_errno_ret)
        mpi_errno = mpi_errno_ret;
    else if (*errflag != MPIR_ERR_NONE)
        MPIR_ERR_SET(mpi_errno, *errflag, "**coll_fail");
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}
