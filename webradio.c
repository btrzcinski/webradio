/**
 * WebRadio
 *
 * Outputs an ICY stream scrubbed of metadata for a player (like GStreamer)
 * to use, while capturing the metadata and logging it.
 * 
 * Copyright(C) Barnett Trzcinski <btrzcinski@gmail.com>.
 * All rights reserved.
 */

#include "defines.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

#include <signal.h>

#include <libsoup/soup.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gio/gunixoutputstream.h>

void header_printer(const char *name,
        const char *value, gpointer user_data)
{
    g_debug("response header > %s: %s", name, value);
}

/* Override the log handler for debug messages so they go to stderr too */
void debug_log_handler(const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *message,
    gpointer user_data)
{
    const gchar *debug_domains = g_getenv("G_MESSAGES_DEBUG");
    if (debug_domains == NULL) return;

    /* debug_domains is a list of log_domains to print messages for,
     * or the special log_domain "all" to print for all domains */

    gchar **debug_domains_list = g_strsplit(debug_domains, " ", 0);
    if (g_strv_contains((const gchar * const *)debug_domains_list, "all") ||
        g_strv_contains((const gchar * const *)debug_domains_list, log_domain))
    {
        fprintf(stderr, "** Debug: %s\n", message);
    }

    g_strfreev(debug_domains_list);
}

static bool exit_requested = false;

void signal_handler(int sig)
{
    switch(sig)
    {
        case SIGINT:
        case SIGTERM:
            exit_requested = true;
            break;
    }
}

int main(int argc, char **argv)
{
    int status;
    struct sigaction act;
    bzero(&act, sizeof(struct sigaction));

    act.sa_handler = &signal_handler;
    status = sigaction(SIGINT, &act, NULL);
    status |= sigaction(SIGTERM, &act, NULL);
    if(status != 0)
    {
        perror("sigaction");
        exit(1);
    }

    g_log_set_handler(NULL, G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG | G_LOG_FLAG_RECURSION,
        debug_log_handler, NULL);

    char *stream_url = "http://7659.live.streamtheworld.com:80/977_80AAC_SC";
    if (argc > 1) stream_url = argv[1];

    g_message("Streaming %s", stream_url);

    GOutputStream *stdout_stream; 
    g_debug("opening stdout");
    stdout_stream = g_unix_output_stream_new(1, FALSE);
    if(stdout_stream == NULL)
    {
        g_critical("couldn't open stdout");
        exit(1);
    }
    g_object_ref_sink(stdout_stream);

    SoupSession *session;
    session = soup_session_new();
    g_object_ref_sink(session);

    SoupMessage *message;
    message = soup_message_new("GET", stream_url); 
    g_object_ref_sink(message);
    soup_message_headers_append(message->request_headers,
            "icy-metadata", "1");

    GInputStream *stream;
    GError *error = NULL;
    stream = soup_session_send(session, message, NULL, &error);
    if(stream == NULL)
    {
        g_critical("Couldn't send HTTP request: %s", error->message);
        g_error_free(error);
        exit(1);
    }
    g_object_ref_sink(stream);

    /* Let's print the response headers */
    SoupMessageHeaders *headers;
    g_object_get(G_OBJECT(message),
            "response-headers", &headers,
            NULL);
    soup_message_headers_foreach(headers, &header_printer, NULL);

    const char *meta_int_str;
    meta_int_str = soup_message_headers_get_one(headers, "icy-metaint");
    if(meta_int_str == NULL)
    {
        g_critical("Couldn't find icy-metaint header in response");
        exit(1);
    }
    int meta_int = atoi(meta_int_str);
    g_debug("Metadata interval set to %d", meta_int);

    gssize byte_count = 0;    
    char buffer[BUF_SIZE];
    g_message("streaming");
    while(!exit_requested)
    {
        gssize bytes_to_read = (meta_int - byte_count > BUF_SIZE)
            ? BUF_SIZE : meta_int - byte_count;
        gssize bytes_read;
        bytes_read = g_input_stream_read(stream,
            buffer,
            bytes_to_read,
            NULL,
            &error);
        if(bytes_read < 0)
        {
            g_critical("Could not read from stream: %s", error->message);
            g_error_free(error);
            break;
        }
        if(bytes_read == 0)
        {
            /* stream was closed? odd */
            g_critical("The stream was unexpectedly closed");
            break;
        }

        /* write the data we got to the output stdout_stream */
        gsize bytes_written;
        gboolean io_result;
        io_result = g_output_stream_write_all(stdout_stream,
                buffer,
                bytes_read,
                &bytes_written,
                NULL,
                &error);
        if(!io_result)
        {
            g_critical("Could not write to stdout: %s", error->message);
            g_error_free(error);
            break;
        }
        else
        {
            g_debug("wrote %lu bytes to stdout", bytes_written);
            g_assert(bytes_written == bytes_read);
        }

        /* otherwise, let's see where we are */
        byte_count += bytes_read;
        g_assert(byte_count <= meta_int);
        if(byte_count >= meta_int)
        {
            /* now we need to read ahead and handle metadata */
            byte_count = 0;

            /* read one byte */
            bytes_read = g_input_stream_read(stream,
                    buffer,
                    1,
                    NULL,
                    &error);
            if(bytes_read <= 0)
            {
                g_critical("Couldn't read metadata byte header: %s", error->message);
                g_error_free(error);
                break;
            }

            g_assert(bytes_read == 1);
            gsize meta_size = (gsize)(*buffer) * 16;
            if(meta_size == 0)
            {
                g_debug("No metadata in this interval");
                continue;
            }

            io_result = g_input_stream_read_all(stream,
                    buffer,
                    meta_size,
                    &bytes_read,
                    NULL,
                    &error);
            if(!io_result)
            {
                g_critical("Couldn't read metadata from stream: %s", error->message);
                g_error_free(error);
                break;
            }
            
            g_assert(bytes_read == meta_size);
            buffer[bytes_read] = '\0';
            GTimeVal current_time;
            g_get_current_time(&current_time);
            gchar *current_time_str = g_time_val_to_iso8601(&current_time);
            g_message("(%s) metadata: %s", current_time_str, buffer);
        }  
    }

    g_message("exiting cleanly");

    /* cleanup */
    g_object_unref(stream);
    g_object_unref(message);
    g_object_unref(session);
    g_object_unref(stdout_stream);
    
    exit(0);
}

