#include "httpd.h"

static char lockword[] = "HTTPD Statistics";

static HTTPSTAT *find_by_key(HTTPSTAT ***stat, short key1, short key2, short resp);
static HTTPSTAT *new_stat(short key1, short key2, short resp);
static HTTPSTAT *update_stat(HTTPSTAT *stat, time64_t *now, double *sec);
typedef enum rpttype {
	RPT_MONTHS,
	RPT_DAYS,
	RPT_HOURS,
	RPT_MINS
} RPTTYPE;
static int stat_report(HTTPD *httpd, unsigned cnt, char ***rpt, RPTTYPE type);
static int save_stats(FILE *fp, HTTPSTAT ***array, const char *label);
static void clear(HTTPSTAT ***array);

int httpstat_load(HTTPD *httpd, const char *dataset)
{
	int			rc	= 0;
	FILE		*fp = NULL;
	HTTPSTAT	*stat;
	int 		which = -1;
	int			len;
	char 		buf[256];

	lock(lockword, LOCK_SHR);

	fp = fopen(dataset, "r,record");
	if (!fp) {
		wtof("HTTPD430E Unable to open \"%s\" for input", dataset);
		goto quit;
	}

	while(len=fread(buf, sizeof(buf), 1, fp)) {
		if (buf[0]=='.' && buf[1]=='/') {
			if (memcmp(&buf[2], "MON", 3)==0) {
				which = RPT_MONTHS;
				continue;
			}
			if (memcmp(&buf[2], "DAY", 3)==0) {
				which = RPT_DAYS;
				continue;
			}
			if (memcmp(&buf[2], "HOU", 3)==0) {
				which = RPT_HOURS;
				continue;
			}
			if (memcmp(&buf[2], "MIN", 3)==0) {
				which = RPT_MINS;
				continue;
			}
			wtof("%s: Invalid record in %s", __func__, dataset);
			wtodumpf(buf, len, "%s: Invalid record", __func__);
			rc = EINVAL;
			goto quit;
		}

		if (which < 0) continue;

		if (memcmp(buf, HTTPSTAT_EYE, sizeof(HTTPSTAT_EYE)-1)!=0) {
			wtof("%s: Invalid record in %s", __func__, dataset);
			wtodumpf(buf, len, "%s: Invalid record", __func__);
			rc = EINVAL;
			goto quit;
		}
		
		stat = calloc(1, sizeof(HTTPSTAT));
		if (!stat) {
			wtof("%s: Out of memory", __func__);
			rc = ENOMEM;
			goto quit;
		}

		memcpy(stat, buf, sizeof(HTTPSTAT));

		switch (which) {
		case RPT_MONTHS:
			rc = array_add(&httpd->st_month, stat);
			break;
		case RPT_DAYS:
			rc = array_add(&httpd->st_day, stat);
			break;
		case RPT_HOURS:
			rc = array_add(&httpd->st_hour, stat);
			break;
		case RPT_MINS:
			rc = array_add(&httpd->st_min, stat);
			break;
		}	/* switch */

		if (rc) {
			wtof("%s: Out of memory", __func__);
			rc = ENOMEM;
			goto quit;
		}
	}	/* while */

    rc = 0;

quit:
	unlock(lockword, LOCK_SHR);

	if (fp) fclose(fp);

	return rc;
}

int httpstat_save(HTTPD *httpd, const char *dataset)
{
	int			rc	= 0;
	FILE		*fp = NULL;

	lock(lockword, LOCK_SHR);

	fp = fopen(dataset, "w,record,recfm=fb,lrecl=80,blksize=27920,space=trk(1,1)");
	if (!fp) {
		wtof("HTTPD430E Unable to open \"%s\" for output", dataset);
		goto quit;
	}

	if (httpd->st_month) {
		rc = save_stats(fp, &httpd->st_month, "MONTH");
		if (rc) goto quit;
	}
	if (httpd->st_day) {
		rc = save_stats(fp, &httpd->st_day, "DAY");
		if (rc) goto quit;
	}
	if (httpd->st_hour) {
		rc = save_stats(fp, &httpd->st_hour, "HOUR");
		if (rc) goto quit;
	}
	if (httpd->st_min) {
		rc = save_stats(fp, &httpd->st_min, "MINUTE");
		if (rc) goto quit;
	}

quit:
	unlock(lockword, LOCK_SHR);

	if (fp) fclose(fp);
	return rc;
}

static int save_stats(FILE *fp, HTTPSTAT ***array, const char *label)
{
	int			rc	= 0;
	unsigned	count, n;
	HTTPSTAT 	*stat;
	char 		buf[80] = {0};
	
	// wtof("%s: enter label=%s", __func__, label);
	
	snprintf(buf, sizeof(buf), "./%s", label);
	rc = fwrite(buf, sizeof(buf), 1, fp);
	if (rc != 1) goto error;

	memset(buf, 0, sizeof(buf));
	count = array_count(array);
	for(n=1; n <= count; n++) {
		stat = array_get(array, n);
		if (!stat) continue;

		memcpy(buf, stat, sizeof(HTTPSTAT));
		rc = fwrite(buf, sizeof(buf), 1, fp);
		if (rc != 1) goto error;
	}

	rc = 0;
	goto quit;
	
error:
	// wtof("%s: error rc=%d", __func__, rc);
	rc = errno;

quit:
	// wtof("%s: exit rc=%d", __func__, rc);
	return rc;
}


void httpstat_report_free(char ***rpt)
{
	unsigned 	count, n;
	char 		*buf;
	
	if (rpt) {
		count = array_count(rpt);
		for(n=count; n > 0; n--) {
			buf = array_get(rpt, n);
			if (buf) free(buf);
		}
		array_free(rpt);
		*rpt = NULL;
	}
}

char **httpstat_report(HTTPD *httpd, unsigned months, unsigned days, unsigned hours, unsigned mins)
{
	char 		**rpt		= NULL;
	int			rc;
	
	lock(lockword, LOCK_SHR);
	
	if (months) {
		rc = stat_report(httpd, months, &rpt, RPT_MONTHS);
		if (rc) goto quit;
	}
	if (days)	{
		rc = stat_report(httpd, days,   &rpt, RPT_DAYS);
		if (rc) goto quit;
	}
	if (hours)	{
		rc = stat_report(httpd, hours,  &rpt, RPT_HOURS);
		if (rc) goto quit;
	}
	if (mins)	{
		rc = stat_report(httpd, mins,   &rpt, RPT_MINS);
		if (rc) goto quit;
	}
	
quit:
	unlock(lockword, LOCK_SHR);
	return rpt;
}

static int 
stat_report(HTTPD *httpd, unsigned cnt, char ***rpt, RPTTYPE type)
{
	unsigned	results = 0;
	HTTPSTAT 	***array;
	const char *title;
	time64_t	now;
	time64_t	low;
	int			rc;
	HTTPSTAT	*stat;
	struct tm   *tm;
	int			len;
	unsigned	count;
	unsigned	n;
	char		buf[256];

#define SEC_PER_MONTH	(30*24*60*60)
#define SEC_PER_DAY		(24*60*60)
#define SEC_PER_HOUR	(60*60)
#define SEC_PER_MIN		(60)

	now	= time64(NULL);
	switch(type) {
		case RPT_MONTHS:
			title = "HTTPD411I Stats for past %u Month%s";
			__64_sub_u32(&now, cnt * SEC_PER_MONTH, &low);
			array = &httpd->st_month;
			break;
		case RPT_DAYS:
			title = "HTTPD411I Stats for past %u Day%s";
			__64_sub_u32(&now, cnt * SEC_PER_DAY, &low);
			array = &httpd->st_day;
			break;
		case RPT_HOURS:
			title = "HTTPD411I Stats for past %u Hour%s";
			__64_sub_u32(&now, cnt * SEC_PER_HOUR, &low);
			array = &httpd->st_hour;
			break;
		case RPT_MINS:
		default:
			title = "HTTPD411I Stats for past %u Minute%s";
			__64_sub_u32(&now, cnt * SEC_PER_MIN, &low);
			array = &httpd->st_min;
			break;
	}
	
	rc = array_addf(rpt, title, cnt, cnt > 1 ? "s" : "");
	if (rc) goto quit;
	
	count = array_count(array);
	if (count) {
		rc = array_addf(rpt, "HTTPD412I  Lowest Average Highest Resp - - - - Time Period - - - -");
	}
	else {
		rc = array_addf(rpt, "HTTPD404I No Statistics exist");
		goto quit;
	}
	if (rc) goto quit;

	for (n=count; n > 0; n--) {
		stat = array_get(array, n);
		if (!stat) continue;

		if (__64_cmp(&stat->last, &low)== __64_SMALLER) continue;
		
		tm = localtime64(&stat->first);
		len = strftime(buf, sizeof(buf), "%b %d %H:%M - ", tm);
		tm = localtime64(&stat->last);
		len = strftime(&buf[len], sizeof(buf)-len, "%b %d %H:%M", tm);
		
		rc = array_addf(rpt, "HTTPD413I %7.3f %7.3f %7.3f  %03d %s",
			stat->lowest, (stat->total / (stat->tally*1.0)), stat->highest,
			stat->resp, buf);
		if (rc) goto quit;
		results++;
	}

	if (!results) {
		rc = array_addf(rpt, "HTTPD404I No statistic records matched");
		if (rc) goto quit;
	}

quit:
	rc = array_addf(rpt, "HTTPD414I --------------------------------------------------------");

	return rc;
}

static void clear(HTTPSTAT ***array) 
{
	unsigned	count, n;
	
	if (array) {
		count = array_count(array);
		for(n=count; n > 0; n--) {
			HTTPSTAT *stat = array_del(array, n);
			if (!stat) continue;
			free(stat);
		}
		array_free(array);
	}
}

void httpstat_clear(HTTPD *httpd)
{
	lock(lockword, LOCK_EXC);

	if (httpd->st_day) {
		clear(&httpd->st_day);
		httpd->st_day = NULL;
	}
	if (httpd->st_hour) {
		clear(&httpd->st_hour);
		httpd->st_hour = NULL;
	}
	if (httpd->st_min) {
		clear(&httpd->st_min);
		httpd->st_min = NULL;
	}
	if (httpd->st_month) {
		clear(&httpd->st_month);
		httpd->st_month = NULL;
	}

	unlock(lockword, LOCK_EXC);
}

int
httpstat_add(HTTPD *httpd, HTTPC *httpc)
{
	time64_t	now		= time64(NULL);
	double 		sec		= httpc->end - httpc->start;
	short 		resp	= httpc->resp;
	struct tm	*tm		= localtime64(&now);
	short 		year	= tm->tm_year;
	short 		month	= tm->tm_mon + 1;
	short		day		= tm->tm_yday;
	short 		hour	= tm->tm_hour;
	short 		min		= tm->tm_min;
	HTTPSTAT	*stat;
	unsigned	count;
    
	lock(lockword, LOCK_EXC);

	/* Minute */
	stat = find_by_key(&httpd->st_min, hour, min, resp);
	if (!stat) goto quit;
	update_stat(stat, &now, &sec);

	// wtodumpf(stat, sizeof(HTTPSTAT), "minute=%d", min);

	/* Hour */
	stat = find_by_key(&httpd->st_hour, day, hour, resp);
	if (!stat) goto quit;
	update_stat(stat, &now, &sec);

	// wtodumpf(stat, sizeof(HTTPSTAT), "hour=%d", hour);
	
	/* Day */
	stat = find_by_key(&httpd->st_day, year, day, resp);
	if (!stat) goto quit;
	update_stat(stat, &now, &sec);

	// wtodumpf(stat, sizeof(HTTPSTAT), "day=%d", day);

	/* Month */
	stat = find_by_key(&httpd->st_month, year, month, resp);
	if (!stat) goto quit;
	update_stat(stat, &now, &sec);

	// wtodumpf(stat, sizeof(HTTPSTAT), "month=%d", month);

	/* Limit to httpd->cfg_st_min_max minute records */
	count = array_count(&httpd->st_min);
	if (count <= httpd->cfg_st_min_max) {
		/* we don't need to look at anything else */
		goto quit;
	}
	else {
		/* pop the first record and free it */
		stat = array_del(&httpd->st_min, 1);
		free(stat);
	}

	/* Limit to httpd->cfg_st_hour_max hour records */
	count = array_count(&httpd->st_hour);
	if (count > httpd->cfg_st_hour_max) {
		/* pop the first record and free it */
		stat = array_del(&httpd->st_hour, 1);
		free(stat);
	}

	/* Limit to httpd->cfg_st_day_max day records */
	count = array_count(&httpd->st_day);
	if (count > httpd->cfg_st_day_max) {
		/* pop the first record and free it */
		stat = array_del(&httpd->st_day, 1);
		free(stat);
	}

	/* Limit to httpd->cfg_st_month_max month records */
	count = array_count(&httpd->st_month);
	if (count > httpd->cfg_st_month_max) {
		/* pop the first record and free it */
		stat = array_del(&httpd->st_month, 1);
		free(stat);
	}

quit:
	unlock(lockword, LOCK_EXC);
	return 0;
}

static HTTPSTAT *
update_stat(HTTPSTAT *stat, time64_t *now, double *sec)
{
	if (!stat) goto quit;
	
	if (!stat->first.u32[1]) stat->first = *now;
	stat->last = *now;
	stat->total += *sec;
	stat->tally += 1;
	if (stat->tally > 9999 || stat->total > 9999.0) {
		/* to prevent overflow we'll average the total now and reset the tally */
		stat->total = stat->total / (stat->tally * 1.0);
		stat->tally = 1;
	}
	if (*sec < stat->lowest) stat->lowest = *sec;
	if (*sec > stat->highest) stat->highest = *sec;

quit:	
	return stat;
}

static HTTPSTAT *
find_by_key(HTTPSTAT ***pstat_array, short key1, short key2, short resp)
{
	HTTPSTAT	**pstat	= *pstat_array;
	HTTPSTAT	*stat	= NULL;
	unsigned	count, n;

	// wtof("%s: pstat_array=%p", __func__, pstat_array);
	// wtof("%s: pstat      =%p", __func__, pstat);
	
	count = array_count(pstat_array);
	// wtof("%s: count=%u", __func__, count);
	for(n=0; n < count; n++) {
		stat = pstat[n];
		if (stat->key1 == key1 && stat->key2 == key2 && stat->resp == resp) {
			goto quit;
		}
	}

	stat = new_stat(key1, key2, resp);
	// wtof("%s: after new_stat() stat=%p", __func__, stat);
	if (stat) {
		array_add(pstat_array, stat);
		// count = array_count(pstat_array);
		// wtof("%s: after array_add() count=%u", __func__, count);
	}

quit:
	return stat;
}

static HTTPSTAT *
new_stat(short key1, short key2, short resp)
{
	HTTPSTAT	*stat	= calloc(1, sizeof(HTTPSTAT));
	
	if (stat) {
		strcpy(stat->eye, HTTPSTAT_EYE);
		stat->total = 0.0;
		stat->highest = 0.0;
		stat->lowest = 9999.0;
		stat->key1 = key1;
		stat->key2 = key2;
		stat->resp = resp;
	}
	
	return stat;
}
