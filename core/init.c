#include "uwsgi.h"

extern struct uwsgi_server uwsgi;

void uwsgi_init_default() {

	uwsgi.backtrace_depth = 64;
        uwsgi.max_apps = 64;

        uwsgi.master_queue = -1;

        uwsgi.signal_socket = -1;
        uwsgi.my_signal_socket = -1;
        uwsgi.cache_server_fd = -1;
        uwsgi.stats_fd = -1;

        uwsgi.original_log_fd = -1;

        uwsgi.emperor_fd_config = -1;
        // default emperor scan frequency
        uwsgi.emperor_freq = 3;
        uwsgi.emperor_throttle = 1000;
        uwsgi.emperor_heartbeat = 30;
        // max 3 minutes throttling
        uwsgi.emperor_max_throttle = 1000 * 180;
        uwsgi.emperor_pid = -1;

        uwsgi.subscribe_freq = 10;
        uwsgi.subscription_tolerance = 17;

        uwsgi.cluster_fd = -1;
        uwsgi.cores = 1;
        uwsgi.threads = 1;

        uwsgi.default_app = -1;

        uwsgi.buffer_size = 4096;
        uwsgi.numproc = 1;

        uwsgi.forkbomb_delay = 2;

        uwsgi.async = 1;
        uwsgi.listen_queue = 100;

        uwsgi.cheaper_overload = 3;

        uwsgi.log_master_bufsize = 8192;

        uwsgi.max_vars = MAX_VARS;
        uwsgi.vec_size = 4 + 1 + (4 * MAX_VARS);

        uwsgi.shared->options[UWSGI_OPTION_SOCKET_TIMEOUT] = 4;
        uwsgi.shared->options[UWSGI_OPTION_LOGGING] = 1;

#ifdef UWSGI_SPOOLER
        uwsgi.shared->spooler_frequency = 30;

        uwsgi.shared->spooler_signal_pipe[0] = -1;
        uwsgi.shared->spooler_signal_pipe[1] = -1;
#endif
        uwsgi.shared->mule_signal_pipe[0] = -1;
        uwsgi.shared->mule_signal_pipe[1] = -1;

        uwsgi.shared->mule_queue_pipe[0] = -1;
        uwsgi.shared->mule_queue_pipe[1] = -1;

        uwsgi.shared->worker_log_pipe[0] = -1;
        uwsgi.shared->worker_log_pipe[1] = -1;

#ifdef UWSGI_SSL
        // 1 day of tolerance
        uwsgi.subscriptions_sign_check_tolerance = 3600 * 24;
#endif

}

void uwsgi_setup_reload() {

        char env_reload_buf[11];

	char *env_reloads = getenv("UWSGI_RELOADS");
        if (env_reloads) {
                //convert env value to int
                uwsgi.reloads = atoi(env_reloads);
                uwsgi.reloads++;
                //convert reloads to string
                int rlen = snprintf(env_reload_buf, 10, "%u", uwsgi.reloads);
                if (rlen > 0) {
                        env_reload_buf[rlen] = 0;
                        if (setenv("UWSGI_RELOADS", env_reload_buf, 1)) {
                                uwsgi_error("setenv()");
                        }
                }
                uwsgi.is_a_reload = 1;
        }
        else {
                if (setenv("UWSGI_RELOADS", "0", 1)) {
                        uwsgi_error("setenv()");
                }
        }

}

void uwsgi_autoload_plugins_by_name(char *argv_zero) {

	char *plugins_requested = NULL;

	char *original_proc_name = getenv("UWSGI_ORIGINAL_PROC_NAME");
        if (!original_proc_name) {
                // here we use argv[0];
                original_proc_name = argv_zero;
                setenv("UWSGI_ORIGINAL_PROC_NAME", original_proc_name, 1);
        }
        char *p = strrchr(original_proc_name, '/');
        if (p == NULL)
                p = original_proc_name;
        p = strstr(p, "uwsgi_");
        if (p != NULL) {
                plugins_requested = strtok(uwsgi_str(p + 6), "_");
                while (plugins_requested) {
                        uwsgi_log("[uwsgi] implicit plugin requested %s\n", plugins_requested);
                        uwsgi_load_plugin(-1, plugins_requested, NULL);
                        plugins_requested = strtok(NULL, "_");
                }
        }

        plugins_requested = getenv("UWSGI_PLUGINS");
        if (plugins_requested) {
                plugins_requested = uwsgi_concat2(plugins_requested, "");
                char *p = strtok(plugins_requested, ",");
                while (p != NULL) {
                        uwsgi_load_plugin(-1, p, NULL);
                        p = strtok(NULL, ",");
                }
        }

}

void uwsgi_commandline_config() {
	int i;

	uwsgi.option_index = -1;

        char *optname;
        while ((i = getopt_long(uwsgi.argc, uwsgi.argv, uwsgi.short_options, uwsgi.long_options, &uwsgi.option_index)) != -1) {

                if (i == '?') {
                        uwsgi_log("getopt_long() error\n");
                        exit(1);
                }

                if (uwsgi.option_index > -1) {
                        optname = (char *) uwsgi.long_options[uwsgi.option_index].name;
                }
                else {
                        optname = uwsgi_get_optname_by_index(i);
                }
                if (!optname) {
                        uwsgi_log("unable to parse command line options\n");
                        exit(1);
                }
                uwsgi.option_index = -1;
                add_exported_option(optname, optarg, 0);
        }


#ifdef UWSGI_DEBUG
        uwsgi_log("optind:%d argc:%d\n", optind, argc);
#endif

        if (optind < uwsgi.argc) {
                for (i = optind; i < uwsgi.argc; i++) {
                        char *lazy = uwsgi.argv[i];
                        if (lazy[0] != '[') {
                                uwsgi_opt_load(NULL, lazy, NULL);
                                // manage magic mountpoint
                                int magic = 0;
                                int j;
                                for (j = 0; j < uwsgi.gp_cnt; j++) {
                                        if (uwsgi.gp[j]->magic) {
                                                if (uwsgi.gp[j]->magic(NULL, lazy)) {
                                                        magic = 1;
                                                        break;
                                                }
                                        }
                                }
                                if (!magic) {
                                        for (j = 0; j < 256; j++) {
                                                if (uwsgi.p[j]->magic) {
                                                        if (uwsgi.p[j]->magic(NULL, lazy)) {
                                                                magic = 1;
                                                                break;
                                                        }
                                                }
                                        }
                                }
                        }
                }
        }

}
