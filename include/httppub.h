#ifndef HTTPPUB_H
#define HTTPPUB_H

#ifndef HTTPD_H
#error #include "httpd.h" first
#endif

extern char *
http_get_timebuf(void)                                                  
    asm("HTTPPUBB");

extern int
http_pub_cache(HTTPD *httpd)                                            
    asm("HTTPPUBC");

extern int
http_pub_datetime(HTTPD *httpd, const char *topic_name)                 
    asm("HTTPPUBD");

extern int
http_pubf(HTTPD *httpd,  const char *topic_name, const char *fmt, ... ) 
    asm("HTTPPUBF");

extern const char *
http_gmt_datetime(void)                                                 
    asm("HTTPPUBG");

extern const char *
http_local_datetime(void)                                               
    asm("HTTPPUBL");

extern int
http_pubf_nocache(HTTPD *httpd,  const char *topic_name, const char *fmt, ... ) 
    asm("HTTPPUBN");

extern const char *
http_get_datetime(HTTPD *httpd)                                         
    asm("HTTPPUBT");

extern const char *
http_fmt_datetime(HTTPD *httpd, time64_t *timer, char *timebuf)         
    asm("HTTPPUBU");

extern int
http_pubv(HTTPD *httpd,  const char *topic_name, const char *fmt, va_list args, int cache) 
    asm("HTTPPUBV");

#endif  /* HTTPPUB_H */
