/*
 * Copyright (c) 2011-*, (mt) MediaTemple <mediatemple.net>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CON-
 * SEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

void print_config (char *conf_file);

/* This function attempts to parse the config file /etc/inotispy.conf.
 * It is important to note that Inotispy *WILL* run with a missing or
 * broken configuration file. All the important default values exist
 * within the source tree and the config file feature is a simple way
 * to give users a small amount of control over how Inotispy behaves.
 */
int init_config(int silent, char *config_file)
{
    int int_rv;
    char *str_rv;
    char *conf_file;
    gboolean bool_rv;
    GError *error;
    GKeyFile *kf;

    error = NULL;

    /* Create config struct and assign default values. */
    CONFIG = g_slice_new(struct inotispy_config);
    CONFIG->zmq_uri = ZMQ_URI;
    CONFIG->log_file = LOG_FILE;
    CONFIG->log_level = LOG_LEVEL_NOTICE;
    CONFIG->log_syslog = FALSE;
    CONFIG->max_inotify_events = INOTIFY_MAX_EVENTS;
    CONFIG->silent = FALSE;

    /* Attempt to read in config file. */
    if (config_file) {
        int_rv = mk_string(&conf_file, "%s", config_file);
    } else {
        int_rv = mk_string(&conf_file, "%s/%s",
                           INOTISPY_SYSCONFDIR, INOTISPY_CONFIG_FILE);
    }

    if (int_rv == -1) {
        fprintf(stderr,
                "** Failed to allocate memory for config file: %s/%s **",
                INOTISPY_SYSCONFDIR, INOTISPY_CONFIG_FILE);
        return 1;
    }

    kf = g_key_file_new();
    if (!g_key_file_load_from_file
        (kf, conf_file, G_KEY_FILE_NONE, &error)) {
        fprintf(stderr, "\n** Failed to load config file %s: %s. **\n\n",
                conf_file, error->message);
        print_config("DEFAULTS LIST");
        free(conf_file);

        /* As stated above Inotify is designed to run with or without
         * this config file present or correct. So we don't bail out
         * here, we just return a failure back to main(), and Inotispy
         * will run using the default config values.
         */
        return 1;
    }

    /* zmq_uri */
    str_rv = g_key_file_get_string(kf, CONF_GROUP, "zmq_uri", &error);
    if (error != NULL) {
        fprintf(stderr, "Failed to read config value for 'zmq_uri': %s\n",
                error->message);
        error = NULL;
    } else {
        int_rv = mk_string(&CONFIG->zmq_uri, "%s", str_rv);
        if (int_rv == -1) {
            fprintf(stderr,
                    "** Failed to allocate memory for user supplied 0MQ URI %s: %s %s **",
                    str_rv, "using default 0MQ URI", ZMQ_URI);
            CONFIG->zmq_uri = ZMQ_URI;
        }

        g_free(str_rv);
    }

    /* log_file */
    str_rv = g_key_file_get_string(kf, CONF_GROUP, "log_file", &error);
    if (error != NULL) {
        fprintf(stderr, "Failed to read config value for 'log_file': %s\n",
                error->message);
        error = NULL;
    } else {
        int_rv = mk_string(&CONFIG->log_file, "%s", str_rv);
        if (int_rv == -1) {
            fprintf(stderr,
                    "** Failed to allocate memory for user supplied log file %s: %s %s **",
                    str_rv, "using default log file", LOG_FILE);
            CONFIG->log_file = LOG_FILE;
        }

        g_free(str_rv);
    }

    /* log_level */
    str_rv = g_key_file_get_string(kf, CONF_GROUP, "log_level", &error);
    if (error != NULL) {
        fprintf(stderr,
                "Failed to read config value for 'log_level': %s\n",
                error->message);
        error = NULL;
    } else {
        if (strcmp(str_rv, "trace") == 0) {
            CONFIG->log_level = LOG_LEVEL_TRACE;
        } else if (strcmp(str_rv, "debug") == 0) {
            CONFIG->log_level = LOG_LEVEL_DEBUG;
        } else if (strcmp(str_rv, "notice") == 0) {
            CONFIG->log_level = LOG_LEVEL_NOTICE;
        } else if (strcmp(str_rv, "warn") == 0) {
            CONFIG->log_level = LOG_LEVEL_WARN;
        } else if (strcmp(str_rv, "error") == 0) {
            CONFIG->log_level = LOG_LEVEL_ERROR;
        } else {
            fprintf(stderr,
                    "Found invalid valud for 'log_level': %s\n", str_rv);
        }

        g_free(str_rv);
    }

    /* log_syslog */
    bool_rv = g_key_file_get_boolean(kf, CONF_GROUP, "log_syslog", &error);
    if (error != NULL) {
        fprintf(stderr,
                "Failed to read config value for 'log_syslog': %s\n",
                error->message);
        error = NULL;
    } else {
        CONFIG->log_syslog = bool_rv;
    }

    /* max_inotify_events */
    int_rv =
        g_key_file_get_integer(kf, CONF_GROUP,
                               "max_inotify_events", &error);
    if (error != NULL) {
        fprintf(stderr,
                "Failed to read config value for 'max_inotify_events': %s\n",
                error->message);
        error = NULL;
    } else {
        CONFIG->max_inotify_events = int_rv;
    }

    /* Silent mode.
     *
     * The command line argument '-s' takes precidence over what's in the
     * config file. So if that flag was set then it doesn't matter what
     * value was set in the config file.
     */
    if (silent) {
        CONFIG->silent = TRUE;
    } else {
        bool_rv = g_key_file_get_boolean(kf, CONF_GROUP, "silent", &error);
        if (error != NULL) {
            fprintf(stderr,
                    "Failed to read config value for 'silent': %s\n",
                    error->message);
            error = NULL;
        } else {
            CONFIG->silent = bool_rv;
        }
    }

    if (!CONFIG->silent)
        print_config(conf_file);

    free(conf_file);
    return 0;
}

void print_config (char *conf_file)
{
    fprintf(stderr, "Using configuration values from %s:\n", conf_file);
    fprintf(stderr, " - zmq_uri            : %s\n", CONFIG->zmq_uri);
    fprintf(stderr, " - log_file           : %s\n", CONFIG->log_file);
    fprintf(stderr, " - log_level          : %s (%d)\n",
            level_str(CONFIG->log_level), CONFIG->log_level);
    fprintf(stderr, " - log_syslog         : %s\n",
            (CONFIG->log_syslog ? "true" : "false"));
    fprintf(stderr, " - max_inotify_events : %d\n",
            CONFIG->max_inotify_events);
    fprintf(stderr, " - silent mode        : %s\n",
            (CONFIG->silent ? "true" : "false"));
}

