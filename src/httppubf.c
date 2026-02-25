#include "httpd.h"

static char     static_timebuf[26] = {0};

static int 
telemetry_cache_update(HTTPT *httpt, const char *topic, const char *data);

int
http_pubv(HTTPD *httpd,  const char *topic_name, const char *fmt, va_list args, int cache)
{
    int     rc  = 0;
    HTTPT   *httpt;
    MQTC    *mqtc;
    int     len;
    char    topic[256];
    char    data[1024];

    if (!httpd) goto quit;
    if (!httpd->httpt) goto quit;
    if (!httpd->httpt->mqtc) goto quit;

    httpt = httpd->httpt;
    mqtc  = httpt->mqtc;

    if (mqtc->flags & MQTC_FLAG_RECONNECT) {
        /* prevent attempts to publish while the MQTT client is in error state */
        goto quit;
    }

    /* construct topic for httpd server */
    if (httpt->prefix && httpt->prefix[0]) {
        /* use the prefix supplied in the configration */
        len = sprintf(topic, "%s", httpt->prefix);
    }
    else {
        /* use our default prefix */
        len = sprintf(topic, "httpd/");
    }
    
    len += sprintf(&topic[len], "%s", topic_name);
    topic[sizeof(topic)-1] = 0;

    len = vsprintf(data, fmt, args);
    if (len < 0) {
        rc = -1;
    }
    else {
        if (len >= sizeof(data)) len = sizeof(data)-1;
        data[len] = 0;
        
        rc = mqtc_pub(mqtc, 0, 1, topic, data);

        if (cache) telemetry_cache_update(httpt, topic, data);

        /* check for errors */
        if (rc) {
            wtof("%s: MQTT Publish error: %d", __func__, rc);
            rc = -1;
        }
        else {
            rc = len;
        }
    }

quit:
    return rc;
}

int
http_pubf(HTTPD *httpd,  const char *topic_name, const char *fmt, ... )
{
    int     rc;
    va_list args;
    
    va_start( args, fmt );
    rc = http_pubv(httpd, topic_name, fmt, args, 1);
    va_end( args );
    
    return rc;
}
    
int
http_pubf_nocache(HTTPD *httpd,  const char *topic_name, const char *fmt, ... )
{
    int     rc;
    va_list args;
    
    va_start( args, fmt );
    rc = http_pubv(httpd, topic_name, fmt, args, 0);
    va_end( args );
    
    return rc;
}
    

char *
http_get_timebuf(void)
{
    char *timebuf = __wsaget(static_timebuf, sizeof(static_timebuf));

quit:
    return timebuf;
}

const char *
http_local_datetime(void)
{
    char        *timebuf    = http_get_timebuf();
    time64_t    timer;
    struct tm*  tm_info;
    
    if (timebuf) {
        time64(&timer);
        tm_info = localtime64(&timer);
        strftime(timebuf, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    }
    
    return timebuf;
}

const char *
http_gmt_datetime(void)
{
    char        *timebuf    = http_get_timebuf();
    time64_t    timer;
    struct tm*  tm_info;
    
    if (timebuf) {
        time64(&timer);
        tm_info = gmtime64(&timer);
        strftime(timebuf, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    }
    
    return timebuf;
}

const char *
http_fmt_datetime(HTTPD *httpd, time64_t *timer, char *timebuf)
{
    int         gmt     = 0;
    struct tm*  tm_info;

    if (!httpd) goto dotime;
    if (!httpd->httpt) goto dotime;

    if (httpd->httpt->flag & HTTPT_FLAG_GMT) gmt = 1;

dotime:    
    if (timebuf) {
        if (gmt) {
            tm_info = gmtime64(timer);
        }
        else {
            tm_info = localtime64(timer);
        }
        strftime(timebuf, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    }
    
    return timebuf;
}


const char *
http_get_datetime(HTTPD *httpd)
{
    int         rc          = 0;
    const char  *timebuf    = NULL;

    if (!httpd) goto quit;
    if (!httpd->httpt) goto quit;
    if (!httpd->httpt->mqtc) goto quit;

    if (httpd->httpt->flag & HTTPT_FLAG_GMT) {
        timebuf = http_gmt_datetime();
    }
    else {
        timebuf = http_local_datetime();
    }
    
quit:
    return timebuf;
}

int
http_pub_datetime(HTTPD *httpd, const char *topic_name)
{
    int         rc          = 0;
    char        *timebuf    = http_get_timebuf();

    if (timebuf) {
        lock(timebuf, LOCK_EXC);
        http_get_datetime(httpd);
        rc = http_pubf(httpd, topic_name, timebuf);
        unlock(timebuf, LOCK_EXC);
    }
    
quit:
    return rc;
}

int
http_pub_cache(HTTPD *httpd)
{
    int         rc      = 0;
    int         lockrc  = 8;
    unsigned    n;
    unsigned    count;
    HTTPT       *httpt;
    MQTC        *mqtc;
    HTTPTC      *httptc;

    if (!httpd) goto quit;
    if (!httpd->httpt) goto quit;
    if (!httpd->httpt->mqtc) goto quit;

    httpt = httpd->httpt;
    mqtc  = httpt->mqtc;

    if (mqtc->flags & MQTC_FLAG_RECONNECT) {
        /* prevent attempts to publish while the MQTT client is in error state */
        goto quit;
    }

    lockrc = lock(&httpt->httptc, LOCK_SHR);
    
    count = array_count(&httpt->httptc);
    for(n=0; n < count; n++) {
        httptc = httpt->httptc[n];
        
        if (!httptc) continue;
        
        rc = mqtc_pub(mqtc, 0, 1, httptc->topic, httptc->data);

        /* check for errors */
        if (rc) {
            wtof("%s: MQTT Publish error: %d", __func__, rc);
            rc = -1;
            goto quit;
        }
    }

quit:
    if (lockrc==0) unlock(&httpt->httptc, LOCK_SHR);
    
    return rc;
}

static int 
telemetry_cache_update(HTTPT *httpt, const char *topic, const char *data)
{
    int         lockrc;
    unsigned    n;
    unsigned    count;
    HTTPTC      *httptc;
    
    lockrc = lock(&httpt->httptc, LOCK_EXC);

    if (!topic) goto quit;
    if (!data) data = "";
    
    count = array_count(&httpt->httptc);
    for(n=0; n < count; n++) {
        httptc = httpt->httptc[n];
        
        if (!httptc) continue;
        if (!httptc->topic) continue;
        
        if (strcmp(httptc->topic, topic)==0) {
            /* found our topic */
            if (httptc->data) {
                if (strcmp(httptc->data, data)==0) {
                    /* data has not changed */
                    goto quit;
                }
                free(httptc->data);
            }
            httptc->data_len = strlen(data);
            httptc->data = strdup(data);
            time64(&httptc->last);
            goto quit;
        }
    }

    /* not found in cache */
    httptc = calloc(1, sizeof(HTTPTC));
    if (!httptc) goto quit;

    /* populate new record and add to cache */
    strcpy(httptc->eye, HTTPTC_EYE);
    time64(&httptc->last);
    httptc->topic_len = strlen(topic);
    httptc->topic = strdup(topic);
    httptc->data_len = strlen(data);
    httptc->data = strdup(data);
    array_add(&httpt->httptc, httptc);

quit:
    if (lockrc==0) unlock(&httpt->httptc, LOCK_EXC);

    return 0;
}
