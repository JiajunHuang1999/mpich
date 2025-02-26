/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include "hydra.h"
#include "pmip.h"
#include "demux.h"
#include "bsci.h"
#include "topo.h"

struct HYD_pmcd_pmip_s HYD_pmcd_pmip;

static HYD_status init_params(void)
{
    HYD_status status = HYD_SUCCESS;

    HYDU_init_user_global(&HYD_pmcd_pmip.user_global);

    HYD_pmcd_pmip.system_global.global_core_map.local_filler = -1;
    HYD_pmcd_pmip.system_global.global_core_map.local_count = -1;
    HYD_pmcd_pmip.system_global.global_core_map.global_count = -1;
    HYD_pmcd_pmip.system_global.pmi_id_map.filler_start = -1;
    HYD_pmcd_pmip.system_global.pmi_id_map.non_filler_start = -1;

    HYD_pmcd_pmip.system_global.global_process_count = -1;
    HYD_pmcd_pmip.system_global.pmi_fd = NULL;
    HYD_pmcd_pmip.system_global.pmi_rank = -1;
    HYD_pmcd_pmip.system_global.pmi_process_mapping = NULL;

    HYD_pmcd_pmip.upstream.server_name = NULL;
    HYD_pmcd_pmip.upstream.server_port = -1;
    HYD_pmcd_pmip.upstream.control = HYD_FD_UNSET;

    HYD_pmcd_pmip.downstream.out = NULL;
    HYD_pmcd_pmip.downstream.err = NULL;
    HYD_pmcd_pmip.downstream.in = HYD_FD_UNSET;
    HYD_pmcd_pmip.downstream.pid = NULL;
    HYD_pmcd_pmip.downstream.exit_status = NULL;
    HYD_pmcd_pmip.downstream.pmi_rank = NULL;
    HYD_pmcd_pmip.downstream.pmi_fd = NULL;

    HYD_pmcd_pmip.local.id = -1;
    HYD_pmcd_pmip.local.pgid = -1;
    HYD_pmcd_pmip.local.iface_ip_env_name = NULL;
    HYD_pmcd_pmip.local.hostname = NULL;
    HYD_pmcd_pmip.local.spawner_kvsname = NULL;
    HYD_pmcd_pmip.local.proxy_core_count = -1;
    HYD_pmcd_pmip.local.proxy_process_count = -1;
    HYD_pmcd_pmip.local.retries = -1;

    HYD_pmcd_pmip.exec_list = NULL;

    status = HYD_pmcd_pmi_allocate_kvs(&HYD_pmcd_pmip.local.kvs, -1);

    return status;
}

static void cleanup_params(void)
{
    HYDU_finalize_user_global(&HYD_pmcd_pmip.user_global);

    /* System global */
    MPL_free(HYD_pmcd_pmip.system_global.pmi_fd);
    MPL_free(HYD_pmcd_pmip.system_global.pmi_process_mapping);


    /* Upstream */
    MPL_free(HYD_pmcd_pmip.upstream.server_name);


    /* Downstream */
    MPL_free(HYD_pmcd_pmip.downstream.out);
    MPL_free(HYD_pmcd_pmip.downstream.err);
    MPL_free(HYD_pmcd_pmip.downstream.pid);
    MPL_free(HYD_pmcd_pmip.downstream.exit_status);
    MPL_free(HYD_pmcd_pmip.downstream.pmi_rank);
    MPL_free(HYD_pmcd_pmip.downstream.pmi_fd);
    MPL_free(HYD_pmcd_pmip.downstream.pmi_fd_active);


    /* Local */
    MPL_free(HYD_pmcd_pmip.local.iface_ip_env_name);
    MPL_free(HYD_pmcd_pmip.local.hostname);
    MPL_free(HYD_pmcd_pmip.local.spawner_kvsname);

    HYD_pmcd_free_pmi_kvs_list(HYD_pmcd_pmip.local.kvs);


    /* Exec list */
    HYDU_free_exec_list(HYD_pmcd_pmip.exec_list);

    HYDT_topo_finalize();
}

static void signal_cb(int sig)
{
    HYDU_FUNC_ENTER();

    if (sig == SIGPIPE) {
        /* Upstream socket closed; kill all processes */
        HYD_pmcd_pmip_send_signal(SIGKILL);
    } else if (sig == SIGTSTP) {
        HYD_pmcd_pmip_send_signal(sig);
    }
    /* Ignore other signals for now */

    HYDU_FUNC_EXIT();
    return;
}

int main(int argc, char **argv)
{
    int i, count, pid, ret_status, sent, closed, ret, done;
    struct HYD_pmcd_hdr hdr;
    HYD_status status = HYD_SUCCESS;

    status = HYDU_dbg_init("proxy:unset");
    HYDU_ERR_POP(status, "unable to initialization debugging\n");

    status = HYDU_set_signal(SIGPIPE, signal_cb);
    HYDU_ERR_POP(status, "unable to set SIGPIPE\n");

    status = HYDU_set_signal(SIGTSTP, signal_cb);
    HYDU_ERR_POP(status, "unable to set SIGTSTP\n");

    status = HYDU_set_common_signals(signal_cb);
    HYDU_ERR_POP(status, "unable to set common signals\n");

    status = init_params();
    HYDU_ERR_POP(status, "Error initializing proxy params\n");

    status = HYD_pmcd_pmip_get_params(argv);
    HYDU_ERR_POP(status, "bad parameters passed to the proxy\n");

    status = HYDT_dmx_init(&HYD_pmcd_pmip.user_global.demux);
    HYDU_ERR_POP(status, "unable to initialize the demux engine\n");

    /* See if HYDI_CONTROL_FD is set before trying to connect upstream */
    ret = MPL_env2int("HYDI_CONTROL_FD", &HYD_pmcd_pmip.upstream.control);
    if (ret < 0) {
        HYDU_ERR_POP(status, "error reading HYDI_CONTROL_FD environment\n");
    } else if (ret == 0) {
        status = HYDU_sock_connect(HYD_pmcd_pmip.upstream.server_name,
                                   HYD_pmcd_pmip.upstream.server_port,
                                   &HYD_pmcd_pmip.upstream.control,
                                   HYD_pmcd_pmip.local.retries, HYD_CONNECT_DELAY);
        HYDU_ERR_POP(status,
                     "unable to connect to server %s at port %d (check for firewalls!)\n",
                     HYD_pmcd_pmip.upstream.server_name, HYD_pmcd_pmip.upstream.server_port);
    }

    struct HYD_pmcd_init_hdr init_hdr;
    strncpy(init_hdr.signature, "HYD", 4);
    init_hdr.proxy_id = HYD_pmcd_pmip.local.id;
    status = HYDU_sock_write(HYD_pmcd_pmip.upstream.control,
                             &init_hdr, sizeof(init_hdr), &sent, &closed, HYDU_SOCK_COMM_MSGWAIT);
    HYDU_ERR_POP(status, "unable to send the proxy ID to the server\n");
    if (closed)
        goto fn_fail;

    status = HYDT_dmx_register_fd(1, &HYD_pmcd_pmip.upstream.control,
                                  HYD_POLLIN, NULL, HYD_pmcd_pmip_control_cmd_cb);
    HYDU_ERR_POP(status, "unable to register fd\n");

    done = 0;
    while (1) {
        /* Wait for some event to occur */
        status = HYDT_dmx_wait_for_event(-1);
        HYDU_ERR_POP(status, "demux engine error waiting for event\n");

        /* Check to see if there's any open read socket left; if there
         * are, we will just wait for more events. */
        count = 0;
        for (i = 0; i < HYD_pmcd_pmip.local.proxy_process_count; i++) {
            if (HYD_pmcd_pmip.downstream.out[i] != HYD_FD_CLOSED)
                count++;
            if (HYD_pmcd_pmip.downstream.err[i] != HYD_FD_CLOSED)
                count++;

            if (count)
                break;
        }
        if (!count)
            break;

        pid = waitpid(-1, &ret_status, WNOHANG);
        if (pid > 0) {
            for (i = 0; i < HYD_pmcd_pmip.local.proxy_process_count; i++) {
                if (HYD_pmcd_pmip.downstream.pid[i] == pid) {
                    HYD_pmcd_pmip.downstream.exit_status[i] = ret_status;
                    if (WIFSIGNALED(ret_status)) {
                        /* kill all processes */
                        HYD_pmcd_pmip_send_signal(SIGKILL);
                    }
                    done++;
                    break;
                }
            }
        }
    }

    /* collect exit_status unless it is a singleton */
    if (HYD_pmcd_pmip.user_global.singleton_pid > 0) {
        HYDU_ASSERT(HYD_pmcd_pmip.local.proxy_process_count == 1, status);
        HYDU_ASSERT(HYD_pmcd_pmip.downstream.pid[0] == HYD_pmcd_pmip.user_global.singleton_pid,
                    status);
        /* We won't get the singleton's exit status. Assume it's 0. */
        if (HYD_pmcd_pmip.downstream.exit_status[0] == PMIP_EXIT_STATUS_UNSET) {
            HYD_pmcd_pmip.downstream.exit_status[0] = 0;
        }
    } else {
        /* Wait for the processes to finish */
        while (1) {
            pid = waitpid(-1, &ret_status, 0);

            /* Find the pid and mark it as complete. */
            if (pid > 0) {
                for (i = 0; i < HYD_pmcd_pmip.local.proxy_process_count; i++) {
                    if (HYD_pmcd_pmip.downstream.pid[i] == pid) {
                        if (HYD_pmcd_pmip.downstream.exit_status[i] == PMIP_EXIT_STATUS_UNSET) {
                            HYD_pmcd_pmip.downstream.exit_status[i] = ret_status;
                        }
                        done++;
                    }
                }
            }

            /* If no more processes are pending, break out */
            if (done == HYD_pmcd_pmip.local.proxy_process_count)
                break;

            /* Check if there are any messages from the launcher */
            status = HYDT_dmx_wait_for_event(0);
            HYDU_IGNORE_TIMEOUT(status);
            HYDU_ERR_POP(status, "demux engine error waiting for event\n");
        }
    }

    /* Send the exit status upstream */
    HYD_pmcd_init_header(&hdr);
    hdr.cmd = CMD_EXIT_STATUS;
    status =
        HYDU_sock_write(HYD_pmcd_pmip.upstream.control, &hdr, sizeof(hdr), &sent, &closed,
                        HYDU_SOCK_COMM_MSGWAIT);
    HYDU_ERR_POP(status, "unable to send EXIT_STATUS command upstream\n");
    HYDU_ASSERT(!closed, status);

    status = HYDU_sock_write(HYD_pmcd_pmip.upstream.control,
                             HYD_pmcd_pmip.downstream.exit_status,
                             HYD_pmcd_pmip.local.proxy_process_count * sizeof(int), &sent,
                             &closed, HYDU_SOCK_COMM_MSGWAIT);
    HYDU_ERR_POP(status, "unable to return exit status upstream\n");
    HYDU_ASSERT(!closed, status);

    status = HYDT_dmx_deregister_fd(HYD_pmcd_pmip.upstream.control);
    HYDU_ERR_POP(status, "unable to deregister fd\n");
    close(HYD_pmcd_pmip.upstream.control);

    status = HYDT_dmx_finalize();
    HYDU_ERR_POP(status, "error returned from demux finalize\n");

    status = HYDT_bsci_finalize();
    HYDU_ERR_POP(status, "unable to finalize the bootstrap device\n");

    /* cleanup the params structure */
    cleanup_params();

  fn_exit:
    HYDU_dbg_finalize();
    return status;

  fn_fail:
    /* kill all processes */
    HYD_pmcd_pmip_send_signal(SIGKILL);
    goto fn_exit;
}
