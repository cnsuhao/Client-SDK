#include "server.h"

const char * guess_content_type(const char *path) {
    const char *last_period, *extension;
    const struct table_entry *ent;
    last_period = strrchr(path, '.');
    if (!last_period || strchr(last_period, '/'))
        return "application/misc";
    extension = last_period + 1;
    for (ent = &content_type_table[0]; ent->extension; ++ent) {
        if (!evutil_ascii_strcasecmp(ent->extension, extension))
            return ent->content_type;
    }
}

void send_file_cb(struct evhttp_request *req, void *arg) {
    struct evbuffer *evb = NULL;
    const char *docroot = arg;
    const char *uri = evhttp_request_get_uri(req);
    struct evhttp_uri *decoded = NULL;
    const char *path;
    char *decoded_path;
    char *whole_path = NULL;
    size_t len;
    int fd = -1;
    struct stat st;

    vdn_proc("");

    if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
        return;
    }

    decoded = evhttp_uri_parse(uri);
    if (!decoded) {
        printf("It's not a good URI. Sending BADREQUEST\n");
        evhttp_send_error(req, HTTP_BADREQUEST, 0);
        return;
    }

    path = evhttp_uri_get_path(decoded);
    if (!path) path = "/";

    decoded_path = evhttp_uridecode(path, 0, NULL);
    if (decoded_path == NULL)
        goto err;

    if (strstr(decoded_path, ".."))
        goto err;

    len = strlen(decoded_path)+strlen(docroot)+2;
    if (!(whole_path = malloc(len))) {
        perror("malloc");
        goto err;
    }
    evutil_snprintf(whole_path, len, "%s/%s", docroot, decoded_path);

    if (stat(whole_path, &st)<0) {
        goto err;
    }

    /* This holds the content we're sending. */
    evb = evbuffer_new();

    if (S_ISDIR(st.st_mode)) {
        goto err;
    } else {
        /* it's a file; add it to the buffer to get sent via sendfile */
        const char *type = guess_content_type(decoded_path);
        if ((fd = open(whole_path, O_RDONLY)) < 0) {
            perror("open");
            goto err;
        }
        if (fstat(fd, &st)<0) {
            perror("fstat");
            goto err;
        }
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", type);
        evbuffer_add_file(evb, fd, 0, st.st_size);
    }

    evhttp_send_reply(req, 200, "OK", evb);
    goto done;
err:
    evhttp_send_error(req, 404, NULL);
    if (fd>=0)
        close(fd);
done:
    if (decoded)
        evhttp_uri_free(decoded);
    if (decoded_path)
        free(decoded_path);
    if (whole_path)
        free(whole_path);
    if (evb)
        evbuffer_free(evb);
}

