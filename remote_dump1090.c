/*
 * remote-dump1090 - A helper to send data from a dump1090 instance to another
 *                   instance
 * Copyright (C) 2015 Rafael G. Martins <rafael@rafaelmartins.eng.br>
 *
 * This program can be distributed under the terms of the BSD License.
 * See the file LICENSE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 1024
#endif

#ifndef SOCKET_TIMEOUT
#define SOCKET_TIMEOUT 5
#endif

#ifndef RETRY_SLEEP
#define RETRY_SLEEP 1
#endif

static int send_to_syslog = 0;


static void
xlog(int priority, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    if (send_to_syslog) {
        vsyslog(priority, format, ap);
    }
    else {
        const char *level;
        if (priority == LOG_WARNING)
            level = "warning";
        else if (priority == LOG_ERR)
            level = "error";
        else
            level = "unknown";
        fprintf(stderr, "%s: ", level);
        vfprintf(stderr, format, ap);
    }
    va_end(ap);
}


static int
socket_connect(const char *host, int port)
{
    int fd;
    do {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            xlog(LOG_WARNING, "Failed to create socket for %s:%d, retrying: %s\n",
                host, port, strerror(errno));
            sleep(RETRY_SLEEP);
        }
    }
    while(fd < 0);

    struct timeval timeout;
    timeout.tv_sec = SOCKET_TIMEOUT;
    timeout.tv_usec = 0;
    int rv = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (rv < 0) {
        xlog(LOG_ERR, "Failed to set socket read timeout for %s:%d: %s\n",
            host, port, strerror(errno));
        exit(1);
    }
    rv = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (rv < 0) {
        xlog(LOG_ERR, "Failed to set socket write timeout for %s:%d: %s\n",
            host, port, strerror(errno));
        exit(1);
    }

    struct hostent *h = gethostbyname2(host, AF_INET);
    if (h == NULL) {
        xlog(LOG_ERR, "Failed to parse hostname for %s: %s\n", host,
            hstrerror(h_errno));
        exit(1);
    }

    struct in_addr **addr_list = (struct in_addr**) h->h_addr_list;
    if (addr_list == NULL || addr_list[0] == NULL) {
        xlog(LOG_ERR, "Can't find any IPv4 address for %s\n", host);
        exit(1);
    }
    if (addr_list[1] != NULL)
        xlog(LOG_WARNING, "Hostname with more than one IPv4 address, "
            "using the first one detected: %s\n", inet_ntoa(*addr_list[0]));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr = *addr_list[0];

    do {
        rv = connect(fd, (const struct sockaddr*) &addr, sizeof(addr));
        if (rv < 0) {
            xlog(LOG_WARNING, "Failed to connect to %s:%d, retrying: %s\n",
                host, port, strerror(errno));
            sleep(RETRY_SLEEP);
        }
    }
    while (rv < 0);
    return fd;
}


static void
main_loop(const char *src_host, int src_port, const char *dst_host, int dst_port)
{
    int dst = socket_connect(dst_host, dst_port);
    int src = socket_connect(src_host, src_port);
    char buf[BUFFER_SIZE];

    while (1) {
        ssize_t rv = read(src, buf, BUFFER_SIZE);
        if (rv <= 0) {
            close(src);
            src = socket_connect(src_host, src_port);
            continue;
        }
        rv = write(dst, buf, rv);
        if (rv <= 0) {
            close(dst);
            dst = socket_connect(dst_host, dst_port);
        }
    }

    // dumb calls, but why not?
    close(src);
    close(dst);
}


static void
print_usage(void)
{
    printf(
        "usage:\n"
        "    remote-dump1090 [-h] [-v] [-l] [-s SRC_PORT] [-d DST_PORT] "
        "SRC_HOST DST_HOST\n");
}


static void
print_help(void)
{
    printf(
        "\n"
        "remote-dump1090 - A helper to send data from a dump1090 instance to "
        "another instance\n"
        "\n"
        "usage:\n"
        "    remote-dump1090 [-h] [-v] [-l] [-s SRC_PORT] [-d DST_PORT] "
        "SRC_HOST DST_HOST\n"
        "\n"
        "positional arguments:\n"
        "    SRC_HOST      source instance host name\n"
        "    DST_HOST      destination instance host name\n"
        "\n"
        "optional arguments:\n"
        "    -h            show this help message and exit\n"
        "    -v            show version and exit\n"
        "    -l            send output messages to syslog\n"
        "    -s SRC_PORT   source instance port. defaults to 30002\n"
        "    -d DST_PORT   destination instance port. defaults to 30001\n");
}


int
main(int argc, char **argv)
{
    // ignore SIGPIPE. it happens when the connection is broken during
    // read/write.
    signal(SIGPIPE, SIG_IGN);

    int rv = 0;

    char *src_host = NULL;
    char *dst_host = NULL;

    int src_port = 30002;
    int dst_port = 30001;

    unsigned int i;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'h':
                    print_help();
                    goto end;
                case 'v':
                    printf("%s\n", PACKAGE_STRING);
                    goto end;
                case 'l':
                    send_to_syslog = 1;
                    break;
                case 's':
                    if (argv[i][2] != '\0')
                        src_port = strtoul(argv[i] + 2, NULL, 10);
                    else
                        src_port = strtoul(argv[++i], NULL, 10);
                    break;
                case 'd':
                    if (argv[i][2] != '\0')
                        dst_port = strtoul(argv[i] + 2, NULL, 10);
                    else
                        dst_port = strtoul(argv[++i], NULL, 10);
                    break;
                default:
                    print_usage();
                    xlog(LOG_ERR, "invalid argument: -%c\n", argv[i][1]);
                    rv = 2;
                    goto end;
            }
        }
        else if (src_host == NULL)
            src_host = argv[i];
        else if (dst_host == NULL)
            dst_host = argv[i];
    }

    if (src_host == NULL) {
        print_usage();
        xlog(LOG_ERR, "SRC_HOST is required\n");
        rv = 2;
    }
    else if (dst_host == NULL) {
        print_usage();
        xlog(LOG_ERR, "DST_HOST is required\n");
        rv = 2;
    }
    else
        main_loop(src_host, src_port, dst_host, dst_port);

end:
    return rv;
}
