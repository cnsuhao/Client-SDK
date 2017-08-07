#include "send_file.h"
#include "get_file.h"

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);

    ev_uint16_t port = 60000;
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        return (1);
    if (argc < 2) {
        return 1;
    }

    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Couldn't create an event_base: exiting\n");
        return 1;
    }

    http = evhttp_new(base);
    if (!http) {
        fprintf(stderr, "couldn't create evhttp. Exiting.\n");
        return 1;
    }

    evhttp_set_gencb(http, do_request_cb, argv[1]);

    handle = evhttp_bind_socket_with_handle(http, "127.0.0.1", port);
    if (!handle) {
        fprintf(stderr, "couldn't bind to port %d. Exiting.\n",
                (int)port);
        return 1;
    }

    event_base_dispatch(base);

    return 0;
}
