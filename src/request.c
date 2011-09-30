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

#include "log.h"
#include "request.h"

JOBJ _parse_json(char *json)
{
    JOBJ jobj;
    JTOK jtok;

    jtok = json_tokener_new();
    jobj = json_tokener_parse_ex(jtok, json, -1);

    if (jtok->err != json_tokener_success) {
        _LOG_TRACE("Failed to parse JSON: %s", json);
        jobj = NULL;
    }

    json_tokener_free(jtok);

    return jobj;
}

Request *request_parse(char *json)
{
    JOBJ jobj, val;
    Request *req;

    _LOG_TRACE("In requst_parse() with JSON data: %s", json);

    jobj = _parse_json(json);

    if (jobj == NULL) {
        _LOG_ERROR("Failed to parse JSON: %s", json);
        return NULL;
    }

    val = json_object_object_get(jobj, "call");

    if (val == NULL) {
        _LOG_ERROR("Failed to find 'call' field in JSON: %s", json);
        return NULL;
    } else if (!json_object_is_type(val, json_type_string)) {
        _LOG_ERROR("Found 'call' field, but it is not a of type 'string'");
        return NULL;
    }

    req = (Request *) malloc(sizeof(Request));

    asprintf(&req->call, "%s", (char *) json_object_get_string(val));
    asprintf(&req->json, "%s", json);
    req->parser = jobj;

    return req;
}

char *request_get_key_str(Request * req, char *key)
{
    JOBJ val;

    _LOG_TRACE("Calling request_get_key_str(): json='%s' key='%s'",
               req->json, key);

    val = json_object_object_get(req->parser, key);

    if (val == NULL) {
        _LOG_ERROR("Failed to find key '%s' in JSON: %s", key, req->json);
        return NULL;
    } else if (!json_object_is_type(val, json_type_string)) {
        _LOG_ERROR("Found key '%s', but it is not a of type 'string'",
                   key);
        return NULL;
    }

    return (char *) json_object_get_string(val);
}

/* The following function returns -1 upon error because zero
 * is a valid value.
 */
int request_get_key_int(Request * req, char *key)
{
    JOBJ val;

    _LOG_TRACE("Calling request_get_key_int(): json='%s' key='%s'",
               req->json, key);

    val = json_object_object_get(req->parser, key);

    if (val == NULL) {
        _LOG_ERROR("Failed to find key '%s' in JSON: %s", key, req->json);
        return -1;
    } else if (!json_object_is_type(val, json_type_int)) {
        _LOG_ERROR("Found key '%s', but it is not a of type 'int'", key);
        return -1;
    }

    return (int) json_object_get_int(val);

}

char *request_get_call(Request * req)
{
    return request_get_key_str(req, "call");
}

char *request_get_path(Request * req)
{
    int last;
    char *path;

    path = request_get_key_str(req, "path");

    /* Clean up path by removing the trailing slash, if it exists. */
    if (strlen(path) > 0) {
        last = strlen(path) - 1;
        if (path[last] == '/')
            path[last] = '\0';
    }

    return path;
}

int request_get_max_events(Request * req)
{
    int max_events;

    max_events = request_get_key_int(req, "max_events");

    if (max_events == -1) {
        _LOG_TRACE("Did not find user defined max events in JSON request");
        return 0;
    }

    return max_events;
}

int request_get_mask(Request * req)
{
    int mask;

    mask = request_get_key_int(req, "mask");

    if (mask == -1) {
        _LOG_TRACE("Did not find user defined mask in JSON request");
        return 0;
    }

    return mask;
}

/* For the following function, the return value upon error is -1.
 * The reason for this is that the value 0 is a valid request that
 * means the user wants to retrieve *all* the events in the queue.
 * So returning a -1 will let the dispatched event callback know
 * that it should bail out of this request without dequeueing any
 * events.
 */
int request_get_count(Request * req)
{
    int count;

    count = request_get_key_int(req, "count");

    if (count == -1) {
        _LOG_TRACE("Did not find a valid event count in JSON request");
        return 1;
    }

    if (count < 0) {
        _LOG_WARN
            ("Invalid event count: %d. Value must be zero or greater.'",
             count);
        return -1;
    }

    return count;
}

char *request_to_string(Request * req)
{
    return req->json;
}

void request_free(Request * req)
{
    json_object_put(req->parser);
    free(req->json);
    free(req->call);
    free(req);
}
