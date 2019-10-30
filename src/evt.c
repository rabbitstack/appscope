#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "dbg.h"
#include "evt.h"

struct _evt_t
{
    transport_t* transport;
    format_t* format;
    regex_t* log_file_re;
    unsigned src[CFG_SRC_MAX];
};


evt_t*
evtCreate()
{
    evt_t* evt = calloc(1, sizeof(evt_t));
    if (!evt) {
        DBG(NULL);
        return NULL;
    }

    if (!(evt->log_file_re = calloc(1, sizeof(regex_t)))) {
        DBG(NULL);
        evtDestroy(&evt);
        return evt;
    }

    if (regcomp(evt->log_file_re, DEFAULT_LOG_FILE_FILTER, REG_EXTENDED)) {
        // regcomp failed.
        DBG(NULL);
        evtDestroy(&evt);
        return evt;
    }

    return evt;
}

void
evtDestroy(evt_t** evt)
{
    if (!evt || !*evt) return;
    evt_t* o = *evt;
    transportDestroy(&o->transport);
    if (o->log_file_re) {
        regfree(o->log_file_re);
        free(o->log_file_re);
    }
    fmtDestroy(&o->format);
    free(o);
    *evt = NULL;
}

int
evtSend(evt_t* evt, const char* msg)
{
    if (!evt || !msg) return -1;

    return transportSend(evt->transport, msg);
}

int
evtSendEvent(evt_t* evt, event_t* e)
{
    if (!evt || !e) return -1;

    char* msg = fmtString(evt->format, e);
    int rv = evtSend(evt, msg);
    if (msg) free(msg);
    return rv;
}

void
evtFlush(evt_t* evt)
{
    if (!evt) return;
    transportFlush(evt->transport);
}

regex_t*
evtLogFileFilter(evt_t* evt)
{

    if (!evt) {
        static regex_t* default_log_file_re = NULL;

        if (default_log_file_re) return default_log_file_re;

        if (!(default_log_file_re = calloc(1, sizeof(regex_t)))) {
            DBG(NULL);
            return NULL;
        }

        if (regcomp(default_log_file_re, DEFAULT_LOG_FILE_FILTER, REG_EXTENDED)) {
            // regcomp failed.
            DBG(NULL);
            free(default_log_file_re);
            default_log_file_re = NULL;
        }
        return default_log_file_re;
    }

    return evt->log_file_re;
}

unsigned
evtSource(evt_t* evt, cfg_evt_t src)
{
    if (evt && src < CFG_SRC_MAX) {
        return evt->src[src];
    }

    switch (src) {
        case CFG_SRC_LOGFILE:
            return DEFAULT_SRC_LOGFILE;
        case CFG_SRC_CONSOLE:
            return DEFAULT_SRC_CONSOLE;
        case CFG_SRC_SYSLOG:
            return DEFAULT_SRC_SYSLOG;
        case CFG_SRC_METRIC:
            return DEFAULT_SRC_METRIC;
        default:
            DBG(NULL);
            return DEFAULT_SRC_LOGFILE;
    }
}

void
evtTransportSet(evt_t* evt, transport_t* transport)
{
    if (!evt) return;

    // Don't leak if evtTransportSet is called repeatedly
    transportDestroy(&evt->transport);
    evt->transport = transport;
}

void
evtFormatSet(evt_t* evt, format_t* format)
{
    if (!evt) return;

    // Don't leak if evtFormatSet is called repeatedly
    fmtDestroy(&evt->format);
    evt->format = format;
}

void
evtLogFileFilterSet(evt_t* evt, const char* str)
{
    if (!evt) return;

    regex_t* temp_re = calloc(1, sizeof(regex_t));
    if (!temp_re) {
        DBG(NULL);
        return;
    }

    const char* filter_string;
    if (!str || str[0]=='\0') {
        // no str.  Revert to default filter
        filter_string = DEFAULT_LOG_FILE_FILTER;
    } else {
        filter_string = str;
    }

    int temp_re_valid = !regcomp(temp_re, filter_string, REG_EXTENDED);
    if (!temp_re_valid) {
        // regcomp failed on filter_string.  Try the default
        filter_string = DEFAULT_LOG_FILE_FILTER;
        temp_re_valid = !regcomp(temp_re, filter_string, REG_EXTENDED);
    }

    if (temp_re_valid) {
        // Out with the old
        regfree(evt->log_file_re);
        free(evt->log_file_re);
        // In with the new
        evt->log_file_re = temp_re;
    } else {
        DBG("%s", str);
        free(temp_re);
    }
}

void
evtSourceSet(evt_t* evt, cfg_evt_t src, unsigned val)
{
    if (!evt || src >= CFG_SRC_MAX) return;
    evt->src[src] = val;
}
