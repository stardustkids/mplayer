/*

dvbstream
(C) Dave Chapman <dave@dchapman.com> 2001, 2002.

The latest version can be found at http://www.linuxstb.org/dvbstream

Modified for use with MPlayer, for details see the changelog at
http://svn.mplayerhq.hu/mplayer/trunk/
$Id$

Copyright notice:

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "stream.h"
#include "libmpdemux/demuxer.h"
#include "help_mp.h"
#include "m_option.h"
#include "m_struct.h"
#include "mp_msg.h"
#include "path.h"
#include "libavutil/avstring.h"

#include "dvbin.h"
#include "dvb_tune.h"

#define MAX_CHANNELS 8
#define CHANNEL_LINE_LEN 256
#define min(a, b) ((a) <= (b) ? (a) : (b))


//TODO: CAMBIARE list_ptr e da globale a per_priv


static struct stream_priv_s
{
	char *prog;
	int card;
	int timeout;
	char *file;
	int fe;
	int dmx;
}
stream_defaults =
{
	"", 0, 30, NULL, 0, -1
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s, f)
#define MAX_CARDS 16
#define MAX_FE 4
#define MAX_DMX 4

/// URL definition
static const m_option_t stream_params[] = {
	{"prog", ST_OFF(prog), CONF_TYPE_STRING, 0, 0 ,0, NULL},
	{"card", ST_OFF(card), CONF_TYPE_INT, M_OPT_RANGE, 0, MAX_CARDS - 1, NULL},
	{"timeout",ST_OFF(timeout),  CONF_TYPE_INT, M_OPT_RANGE, 1, 240, NULL},
	{"file", ST_OFF(file), CONF_TYPE_STRING, 0, 0 ,0, NULL},
	{"fe", ST_OFF(fe), CONF_TYPE_INT, M_OPT_RANGE, 0, MAX_FE - 1, NULL},
	{"dmx", ST_OFF(dmx), CONF_TYPE_INT, M_OPT_RANGE, -1, MAX_DMX - 1, NULL},

	{"hostname", 	ST_OFF(prog), CONF_TYPE_STRING, 0, 0, 0, NULL },
	{"username", 	ST_OFF(card), CONF_TYPE_INT, M_OPT_RANGE, 0, MAX_CARDS - 1, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

static const struct m_struct_st stream_opts = {
	"dvbin",
	sizeof(struct stream_priv_s),
	&stream_defaults,
	stream_params
};



const m_option_t dvbin_opts_conf[] = {
	{"prog", &stream_defaults.prog, CONF_TYPE_STRING, 0, 0 ,0, NULL},
	{"card", &stream_defaults.card, CONF_TYPE_INT, M_OPT_RANGE, 0, MAX_CARDS - 1, NULL},
	{"timeout",  &stream_defaults.timeout,  CONF_TYPE_INT, M_OPT_RANGE, 1, 240, NULL},
	{"file", &stream_defaults.file, CONF_TYPE_STRING, 0, 0 ,0, NULL},
	{"fe", &stream_defaults.fe, CONF_TYPE_INT, M_OPT_RANGE, 0, MAX_FE - 1, NULL},
	{"dmx", &stream_defaults.dmx, CONF_TYPE_INT, M_OPT_RANGE, -1, MAX_DMX - 1, NULL},

	{NULL, NULL, 0, 0, 0, 0, NULL}
};


#if DVB_API_VERSION >= 5
extern int ts_prog; // see ../cfg-common[-opts].h, ../libmpdemux/demux_ts.c
extern demuxer_t *mp_get_demuxer(void);

#define S2API_SYMDEF(x) {#x, x}

static const struct symbol_def_s
{
  const char *name;
  const int id;
} s2api_symbols[] = {
  S2API_SYMDEF (DTV_UNDEFINED), S2API_SYMDEF (DTV_TUNE),
      S2API_SYMDEF (DTV_CLEAR), S2API_SYMDEF (DTV_FREQUENCY),
      S2API_SYMDEF (DTV_MODULATION), S2API_SYMDEF (DTV_BANDWIDTH_HZ),
      S2API_SYMDEF (DTV_INVERSION), S2API_SYMDEF (DTV_DISEQC_MASTER),
      S2API_SYMDEF (DTV_SYMBOL_RATE), S2API_SYMDEF (DTV_INNER_FEC),
      S2API_SYMDEF (DTV_VOLTAGE), S2API_SYMDEF (DTV_TONE),
      S2API_SYMDEF (DTV_PILOT), S2API_SYMDEF (DTV_ROLLOFF),
      S2API_SYMDEF (DTV_DISEQC_SLAVE_REPLY),
      S2API_SYMDEF (DTV_FE_CAPABILITY_COUNT), S2API_SYMDEF (DTV_FE_CAPABILITY),
      S2API_SYMDEF (DTV_DELIVERY_SYSTEM),
/*
      S2API_SYMDEF (DTV_ISDBT_PARTIAL_RECEPTION),
      S2API_SYMDEF (DTV_ISDBT_SOUNR_BROADCASTING),
      S2API_SYMDEF (DTV_ISDBT_SB_SUBCHANNEL_ID),
      S2API_SYMDEF (DTV_ISDBT_SB_SEGMENT_IDX),
      S2API_SYMDEF (DTV_ISDBT_SB_SEGMENT_COUNT),
      S2API_SYMDEF (DTV_ISDBT_LAYERA_FEC),
      S2API_SYMDEF (DTV_ISDBT_LAYERA_MODULATION),
      S2API_SYMDEF (DTV_ISDBT_LAYERA_SEGMENT_COUNT),
      S2API_SYMDEF (DTV_ISDBT_LAYERA_TIME_INTERLEAVING),
      S2API_SYMDEF (DTV_ISDBT_LAYERB_FEC),
      S2API_SYMDEF (DTV_ISDBT_LAYERB_MODULATION),
      S2API_SYMDEF (DTV_ISDBT_LAYERB_SEGMENT_COUNT),
      S2API_SYMDEF (DTV_ISDBT_LAYERB_TIME_INTERLEAVING),
      S2API_SYMDEF (DTV_ISDBT_LAYERC_FEC),
      S2API_SYMDEF (DTV_ISDBT_LAYERC_MODULATION),
      S2API_SYMDEF (DTV_ISDBT_LAYERC_SEGMENT_COUNT),
      S2API_SYMDEF (DTV_ISDBT_LAYERC_TIME_INTERLEAVING),
 */
      S2API_SYMDEF (DTV_API_VERSION),
      S2API_SYMDEF (DTV_CODE_RATE_HP), S2API_SYMDEF (DTV_CODE_RATE_LP),
      S2API_SYMDEF (DTV_GUARD_INTERVAL), S2API_SYMDEF (DTV_TRANSMISSION_MODE),
      S2API_SYMDEF (DTV_HIERARCHY),
/*
      S2API_SYMDEF (DTV_ISDBT_LAYER_ENABLED), S2API_SYMDEF (DTV_ISDBS_TS_ID),
 */
  {"DTV_ISDBS_TS_ID", 42},
      S2API_SYMDEF (PILOT_ON), S2API_SYMDEF (PILOT_OFF),
      S2API_SYMDEF (PILOT_AUTO),
      S2API_SYMDEF (ROLLOFF_35), S2API_SYMDEF (ROLLOFF_20),
      S2API_SYMDEF (ROLLOFF_25), S2API_SYMDEF (ROLLOFF_AUTO),
      S2API_SYMDEF (SYS_UNDEFINED), S2API_SYMDEF (SYS_DVBC_ANNEX_AC),
      S2API_SYMDEF (SYS_DVBC_ANNEX_B), S2API_SYMDEF (SYS_DVBT),
      S2API_SYMDEF (SYS_DSS), S2API_SYMDEF (SYS_DVBS),
      S2API_SYMDEF (SYS_DVBS2), S2API_SYMDEF (SYS_DVBH),
      S2API_SYMDEF (SYS_ISDBT), S2API_SYMDEF (SYS_ISDBS),
      S2API_SYMDEF (SYS_ISDBC), S2API_SYMDEF (SYS_ATSC),
      S2API_SYMDEF (SYS_ATSCMH), S2API_SYMDEF (SYS_DMBTH),
      S2API_SYMDEF (SYS_CMMB), S2API_SYMDEF (SYS_DAB),
      S2API_SYMDEF (SEC_VOLTAGE_13), S2API_SYMDEF (SEC_VOLTAGE_18),
      S2API_SYMDEF (SEC_VOLTAGE_OFF),
  {NULL, 0}
};

static int s2api_id (const char *name)
{
	const struct symbol_def_s *p;

	if (name == NULL)
		return 0;
	if (*name == '\0')
		return 0;

	for (p = s2api_symbols; p->name != NULL; p++)
		if (!strcmp(name, p->name))
			return p->id;
	return 0;
}

static void parse_s2api_props(char *txt, dvb_channel_t *ch, int type)
{
	char pname[256], val[256];
	const char *prop_conf = "%255[^|=]=%255[^|=]";
	char *p, *q, *r;
	int has_dtv_tune = 0, has_freq = 0;
	struct dtv_property props[DTV_IOCTL_MAX_MSGS];
	int i, len, d;

	if (!txt)
		return;

	p = txt;
	do {
		q = strchr(p, '|');
		if (q != NULL)
			*q = '\0';
		if (sscanf(p, prop_conf, pname, val) != 2)
			goto try_next;
		if (pname[0] == '\0' || val[0] == '\0')
			goto try_next;

		props[ch->tvps.num].cmd = s2api_id(pname);
		if (props[ch->tvps.num].cmd == 0)
			props[ch->tvps.num].cmd = strtoul(pname, &r, 0);
		if (props[ch->tvps.num].cmd == 0)
			goto try_next;

		if (val[0] != '[') {
			props[ch->tvps.num].u.data = strtoul(val, &r, 0);
			if (isalpha(*r))
				props[ch->tvps.num].u.data = s2api_id(val);
		} else {
			len = strtoul(val + 1, &r, 0);
			if (*r != ']' || len > 32)
				goto try_next;
			props[ch->tvps.num].u.buffer.len = len;
			for (i = 0; i < len; i++) {
				p = r;
				if (*p == '\0')
					goto try_next;
				d = strtoul(++p, &r, 0);
				if ((*r != ',' && *r != '\0') || d > 255)
					goto try_next;
				props[ch->tvps.num].u.buffer.data[i] = d;
			}
		}
		if (props[ch->tvps.num].cmd == DTV_FREQUENCY) {
			ch->freq = props[ch->tvps.num].u.data;
			has_freq = 1;
		} else if (props[ch->tvps.num].cmd == DTV_TUNE)
			has_dtv_tune = 1;
		else if (props[ch->tvps.num].cmd == DTV_DELIVERY_SYSTEM &&
			   props[ch->tvps.num].u.data != type &&
			   type != SYS_UNDEFINED) {
			has_freq = 0;    // skip this channel
			break;
		}
		ch->tvps.num++;
		if (ch->tvps.num >= DTV_IOCTL_MAX_MSGS)
			break;
try_next:
		p = q;
		p++;
	} while (q != NULL);

	if (ch->tvps.num == 0 || !has_freq) {
		ch->freq = 0;
		ch->tvps.num = 0;
		ch->tvps.props = NULL;
		return;
	}
	if (!has_dtv_tune) {
		if (ch->tvps.num == DTV_IOCTL_MAX_MSGS)
			ch->tvps.num--;
		props[ch->tvps.num].cmd = DTV_TUNE;
		props[ch->tvps.num].u.data = 1;
		ch->tvps.num++;
	}
	ch->tvps.props = malloc (ch->tvps.num * sizeof(struct dtv_property));
	if (ch->tvps.props) {
		memcpy(ch->tvps.props, props, ch->tvps.num * sizeof(struct dtv_property));
		ch->pids[0] = 8192;
		ch->pids_cnt = 1;
	} else {
		ch->freq = 0;
		ch->tvps.num = 0;
	}
	return;
}
#endif

static dvb_channels_list *dvb_get_channels(char *filename, int type)
{
	dvb_channels_list  *list;
	FILE *f;
	char line[CHANNEL_LINE_LEN], *colon;

	int fields, cnt, pcnt, k;
	int has8192, has0;
	dvb_channel_t *ptr, *tmp, chn;
	char tmp_lcr[256], tmp_hier[256], inv[256], bw[256], cr[256], mod[256], transm[256], gi[256], vpid_str[256], apid_str[256];
	char dtv_props[256];
	const char *cbl_conf = "%d:%255[^:]:%d:%255[^:]:%255[^:]:%255[^:]:%255[^:]\n";
	const char *sat_conf = "%d:%c:%d:%d:%255[^:]:%255[^:]\n";
	const char *ter_conf = "%d:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]\n";
	const char *atsc_conf = "%d:%255[^:]:%255[^:]:%255[^:]\n";
	const char *s2api_conf = "%255[^:]:%i\n";
	int alloced_num;

	mp_msg(MSGT_DEMUX, MSGL_V, "CONFIG_READ FILE: %s, type: %d\n", filename, type);
	if((f=fopen(filename, "r"))==NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_FATAL, "CAN'T READ CONFIG FILE %s\n", filename);
		return NULL;
	}

	list = malloc(sizeof(dvb_channels_list));
	if(list == NULL)
	{
		fclose(f);
		mp_msg(MSGT_DEMUX, MSGL_ERR, "DVB_GET_CHANNELS: couldn't malloc enough memory\n");
		return NULL;
	}

	ptr = &chn;
	list->NUM_CHANNELS = 0;
	alloced_num = 32;
	list->channels = malloc(sizeof(dvb_channel_t) * alloced_num);
	if (!list->channels)
		goto done;
	while(! feof(f))
	{
		if( fgets(line, CHANNEL_LINE_LEN, f) == NULL )
			continue;

		if((line[0] == '#') || (strlen(line) == 0))
			continue;

		colon = strchr(line, ':');
		if(colon)
		{
			k = colon - line;
			if(!k)
				continue;
			ptr->name = malloc(k+1);
			if(! ptr->name)
				continue;
			av_strlcpy(ptr->name, line, k+1);
		}
		else
			continue;
		k++;
		apid_str[0] = vpid_str[0] = 0;
		ptr->pids_cnt = 0;
		ptr->freq = 0;
#if DVB_API_VERSION >= 5
		ptr->tvps.num = 0;
#endif
		if(type == TUNER_TER)
		{
			fields = sscanf(&line[k], ter_conf,
				&ptr->freq, inv, bw, cr, tmp_lcr, mod,
				transm, gi, tmp_hier, vpid_str, apid_str);
			mp_msg(MSGT_DEMUX, MSGL_V,
				"TER, NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d",
				list->NUM_CHANNELS, fields, ptr->name, ptr->freq);
		}
		else if(type == TUNER_CBL)
		{
			fields = sscanf(&line[k], cbl_conf,
				&ptr->freq, inv, &ptr->srate,
				cr, mod, vpid_str, apid_str);
			mp_msg(MSGT_DEMUX, MSGL_V,
				"CBL, NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d, SRATE: %d",
				list->NUM_CHANNELS, fields, ptr->name, ptr->freq, ptr->srate);
		}
#ifdef DVB_ATSC
		else if(type == TUNER_ATSC)
		{
			fields = sscanf(&line[k], atsc_conf,
				 &ptr->freq, mod, vpid_str, apid_str);
			mp_msg(MSGT_DEMUX, MSGL_V,
				"ATSC, NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d\n",
				list->NUM_CHANNELS, fields, ptr->name, ptr->freq);
		}
#endif
#if DTV_API_VERSION >= 5
		else if((type & TUNER_S2API_BASE) &&
			(type - TUNER_S2API_BASE  <= SYS_DAB))
		{
			fields = sscanf(&line[k], s2api_conf,
				dtv_props, &ptr->progid);
			mp_msg(MSGT_DEMUX, MSGL_V, "parse S2API: %s. prog:%d\n", dtv_props, ptr->progid);
			parse_s2api_props(dtv_props, ptr, type - TUNER_S2API_BASE);
			if (ptr->freq)
				mp_msg(MSGT_DEMUX, MSGL_V,
					"S2API, NUM: %d, NUM_FIELDS: %d, NAME: %s, PROPS: %s\n",
					list->NUM_CHANNELS, fields, ptr->name, dtv_props);
		}
#endif
		else //SATELLITE
		{
			fields = sscanf(&line[k], sat_conf,
				&ptr->freq, &ptr->pol, &ptr->diseqc, &ptr->srate, vpid_str, apid_str);
			ptr->pol = toupper(ptr->pol);
			ptr->freq *=  1000UL;
			ptr->srate *=  1000UL;
			ptr->tone = -1;
			ptr->inv = INVERSION_AUTO;
			ptr->cr = FEC_AUTO;
			if((ptr->diseqc > 4) || (ptr->diseqc < 0))
			    continue;
			if(ptr->diseqc > 0)
			    ptr->diseqc--;
			mp_msg(MSGT_DEMUX, MSGL_V,
				"SAT, NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d, SRATE: %d, POL: %c, DISEQC: %d",
				list->NUM_CHANNELS, fields, ptr->name, ptr->freq, ptr->srate, ptr->pol, ptr->diseqc);
		}

		if(vpid_str[0])
		{
			pcnt = sscanf(vpid_str, "%d+%d+%d+%d+%d+%d+%d", &ptr->pids[0], &ptr->pids[1], &ptr->pids[2], &ptr->pids[3],
				&ptr->pids[4], &ptr->pids[5], &ptr->pids[6]);
			if(pcnt > 0)
			{
				ptr->pids_cnt = pcnt;
				fields++;
			}
		}

		if(apid_str[0])
		{
			cnt = ptr->pids_cnt;
			pcnt = sscanf(apid_str, "%d+%d+%d+%d+%d+%d+%d+%d", &ptr->pids[cnt], &ptr->pids[cnt+1], &ptr->pids[cnt+2],
				&ptr->pids[cnt+3], &ptr->pids[cnt+4], &ptr->pids[cnt+5], &ptr->pids[cnt+6], &ptr->pids[cnt+7]);
			if(pcnt > 0)
			{
				ptr->pids_cnt += pcnt;
				fields++;
			}
		}

		if((fields < 2) || (ptr->pids_cnt < 0) || (ptr->freq == 0) || (strlen(ptr->name) == 0))
			continue;

		has8192 = has0 = 0;
		for(cnt = 0; cnt < ptr->pids_cnt; cnt++)
		{
			if(ptr->pids[cnt] == 8192)
				has8192 = 1;
			if(ptr->pids[cnt] == 0)
				has0 = 1;
		}
		if(has8192)
		{
			ptr->pids[0] = 8192;
			ptr->pids_cnt = 1;
		}
		else if(! has0)
		{
			ptr->pids[ptr->pids_cnt] = 0;	//PID 0 is the PAT
			ptr->pids_cnt++;
		}
		mp_msg(MSGT_DEMUX, MSGL_V, " PIDS: ");
		for(cnt = 0; cnt < ptr->pids_cnt; cnt++)
			mp_msg(MSGT_DEMUX, MSGL_V, " %d ", ptr->pids[cnt]);
		mp_msg(MSGT_DEMUX, MSGL_V, "\n");

		if((type == TUNER_TER) || (type == TUNER_CBL))
		{
			if(! strcmp(inv, "INVERSION_ON"))
				ptr->inv = INVERSION_ON;
			else if(! strcmp(inv, "INVERSION_OFF"))
				ptr->inv = INVERSION_OFF;
			else
				ptr->inv = INVERSION_AUTO;


			if(! strcmp(cr, "FEC_1_2"))
				ptr->cr =FEC_1_2;
			else if(! strcmp(cr, "FEC_2_3"))
				ptr->cr =FEC_2_3;
			else if(! strcmp(cr, "FEC_3_4"))
				ptr->cr =FEC_3_4;
			else if(! strcmp(cr, "FEC_4_5"))
				ptr->cr =FEC_4_5;
			else if(! strcmp(cr, "FEC_6_7"))
				ptr->cr =FEC_6_7;
			else if(! strcmp(cr, "FEC_8_9"))
				ptr->cr =FEC_8_9;
			else if(! strcmp(cr, "FEC_5_6"))
				ptr->cr =FEC_5_6;
			else if(! strcmp(cr, "FEC_7_8"))
				ptr->cr =FEC_7_8;
			else if(! strcmp(cr, "FEC_NONE"))
				ptr->cr =FEC_NONE;
			else ptr->cr =FEC_AUTO;
		}


		if((type == TUNER_TER) || (type == TUNER_CBL) || (type == TUNER_ATSC))
		{
			if(! strcmp(mod, "QAM_128"))
				ptr->mod = QAM_128;
			else if(! strcmp(mod, "QAM_256"))
				ptr->mod = QAM_256;
			else if(! strcmp(mod, "QAM_64"))
				ptr->mod = QAM_64;
			else if(! strcmp(mod, "QAM_32"))
				ptr->mod = QAM_32;
			else if(! strcmp(mod, "QAM_16"))
				ptr->mod = QAM_16;
#ifdef DVB_ATSC
			else if(! strcmp(mod, "VSB_8") || ! strcmp(mod, "8VSB"))
				ptr->mod = VSB_8;
			else if(! strcmp(mod, "VSB_16") || !strcmp(mod, "16VSB"))
				ptr->mod = VSB_16;
			else if(! strcmp(mod, "QAM_AUTO"))
				ptr->mod = QAM_AUTO;

#endif
		}

		if(type == TUNER_TER)
		{
			if(! strcmp(bw, "BANDWIDTH_6_MHZ"))
				ptr->bw = BANDWIDTH_6_MHZ;
			else if(! strcmp(bw, "BANDWIDTH_7_MHZ"))
				ptr->bw = BANDWIDTH_7_MHZ;
			else if(! strcmp(bw, "BANDWIDTH_8_MHZ"))
				ptr->bw = BANDWIDTH_8_MHZ;


			if(! strcmp(transm, "TRANSMISSION_MODE_2K"))
				ptr->trans = TRANSMISSION_MODE_2K;
			else if(! strcmp(transm, "TRANSMISSION_MODE_8K"))
				ptr->trans = TRANSMISSION_MODE_8K;
			else if(! strcmp(transm, "TRANSMISSION_MODE_AUTO"))
				ptr->trans = TRANSMISSION_MODE_AUTO;

			if(! strcmp(gi, "GUARD_INTERVAL_1_32"))
				ptr->gi = GUARD_INTERVAL_1_32;
			else if(! strcmp(gi, "GUARD_INTERVAL_1_16"))
				ptr->gi = GUARD_INTERVAL_1_16;
			else if(! strcmp(gi, "GUARD_INTERVAL_1_8"))
				ptr->gi = GUARD_INTERVAL_1_8;
			else if(! strcmp(gi, "GUARD_INTERVAL_1_4"))
				ptr->gi = GUARD_INTERVAL_1_4;
			else ptr->gi = GUARD_INTERVAL_AUTO;

			if(! strcmp(tmp_lcr, "FEC_1_2"))
				ptr->cr_lp =FEC_1_2;
			else if(! strcmp(tmp_lcr, "FEC_2_3"))
				ptr->cr_lp =FEC_2_3;
			else if(! strcmp(tmp_lcr, "FEC_3_4"))
				ptr->cr_lp =FEC_3_4;
			else if(! strcmp(tmp_lcr, "FEC_4_5"))
				ptr->cr_lp =FEC_4_5;
			else if(! strcmp(tmp_lcr, "FEC_6_7"))
				ptr->cr_lp =FEC_6_7;
			else if(! strcmp(tmp_lcr, "FEC_8_9"))
				ptr->cr_lp =FEC_8_9;
			else if(! strcmp(tmp_lcr, "FEC_5_6"))
				ptr->cr_lp =FEC_5_6;
			else if(! strcmp(tmp_lcr, "FEC_7_8"))
				ptr->cr_lp =FEC_7_8;
			else if(! strcmp(tmp_lcr, "FEC_NONE"))
				ptr->cr_lp =FEC_NONE;
			else ptr->cr_lp =FEC_AUTO;


			if(! strcmp(tmp_hier, "HIERARCHY_1"))
				ptr->hier = HIERARCHY_1;
			else if(! strcmp(tmp_hier, "HIERARCHY_2"))
				ptr->hier = HIERARCHY_2;
			else if(! strcmp(tmp_hier, "HIERARCHY_4"))
				ptr->hier = HIERARCHY_4;
			else if(! strcmp(tmp_hier, "HIERARCHY_AUTO"))
				ptr->hier = HIERARCHY_AUTO;
			else	ptr->hier = HIERARCHY_NONE;
		}

		if(list->NUM_CHANNELS + 1 >= alloced_num) {
			alloced_num += 32;
			tmp = realloc(list->channels,
				     sizeof(dvb_channel_t) * alloced_num);
			if(tmp == NULL)
				break;

			list->channels = tmp;
		}
		memcpy(&(list->channels[list->NUM_CHANNELS]), ptr, sizeof(dvb_channel_t));
		list->NUM_CHANNELS++;
		if(sizeof(dvb_channel_t) * list->NUM_CHANNELS >= 1024*1024)
		{
			mp_msg(MSGT_DEMUX, MSGL_V, "dvbin.c, > 1MB allocated for channels struct, dropping the rest of the file\r\n");
			break;
		}
	}

done:
	fclose(f);
	if(list->NUM_CHANNELS == 0)
	{
		free(list->channels);
		free(list);
		return NULL;
	}

	list->current = 0;
	return list;
}

void dvb_free_config(dvb_config_t *config)
{
	int i, j;

	for(i=0; i<config->count; i++)
	{
		free(config->cards[i].name);
		if(!config->cards[i].list)
			continue;
		if(config->cards[i].list->channels)
		{
			for(j=0; j<config->cards[i].list->NUM_CHANNELS; j++)
			{
				free(config->cards[i].list->channels[j].name);
#if DVB_API_VERSION >= 5
				if(config->cards[i].list->channels[j].tvps.num)
					free(config->cards[i].list->channels[j].tvps.props);
#endif
			}
			free(config->cards[i].list->channels);
		}
		free(config->cards[i].list);
	}
	free(config);
}

static int dvb_streaming_read(stream_t *stream, char *buffer, int size)
{
	struct pollfd pfds[1];
	int pos=0, tries, rk, fd;
	dvb_priv_t *priv  = (dvb_priv_t *) stream->priv;

	mp_msg(MSGT_DEMUX, MSGL_DBG3, "dvb_streaming_read(%d)\n", size);

	tries = priv->retry + 1;

	fd = stream->fd;
	while(pos < size)
	{
		pfds[0].fd = fd;
		pfds[0].events = POLLIN | POLLPRI;

		rk = size - pos;
		if(poll(pfds, 1, 500) <= 0)
		{
			mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_streaming_read, attempt N. %d failed with errno %d when reading %d bytes\n", tries, errno, size-pos);
			errno = 0;
			if(--tries > 0)
				continue;
			else
				break;
		}
		if((rk = read(fd, &buffer[pos], rk)) > 0)
		{
			pos += rk;
			mp_msg(MSGT_DEMUX, MSGL_DBG3, "ret (%d) bytes\n", pos);
		}
	}


	if(! pos)
		mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_streaming_read, return %d bytes\n", pos);

	return pos;
}

static void dvbin_close(stream_t *stream);

int dvb_set_channel(stream_t *stream, int card, int n)
{
	dvb_channels_list *new_list;
	dvb_channel_t *channel;
	dvb_priv_t *priv = stream->priv;
	char buf[4096];
	dvb_config_t *conf = (dvb_config_t *) priv->config;
	int devno;
	int feno, dmxno;
	int i;

	if((card < 0) || (card > conf->count))
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_set_channel: INVALID CARD NUMBER: %d vs %d, abort\n", card, conf->count);
		return 0;
	}

	devno = conf->cards[card].devno;
	feno = conf->cards[card].feno;
	dmxno = conf->cards[card].dmxno;
	new_list = conf->cards[card].list;
	if((n > new_list->NUM_CHANNELS) || (n < 0))
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_set_channel: INVALID CHANNEL NUMBER: %d, for card %d, abort\n", n, card);
		return 0;
	}
	channel = &(new_list->channels[n]);

	if(priv->is_on)	//the fds are already open and we have to stop the demuxers
	{
		for(i = 0; i < priv->demux_fds_cnt; i++)
			dvb_demux_stop(priv->demux_fds[i]);

		priv->retry = 0;
		while(dvb_streaming_read(stream, buf, 4096) > 0);	//empty both the stream's and driver's buffer
		if(priv->card != card)
		{
			dvbin_close(stream);
			if(! dvb_open_devices(priv, devno, feno, dmxno, channel->pids_cnt))
			{
				mp_msg(MSGT_DEMUX, MSGL_ERR, "DVB_SET_CHANNEL, COULDN'T OPEN DEVICES OF CARD: %d, EXIT\n", card);
				return 0;
			}
		}
		else	//close all demux_fds with pos > pids required for the new channel or open other demux_fds if we have too few
		{
			if(! dvb_fix_demuxes(priv, channel->pids_cnt))
				return 0;
		}
	}
	else
	{
		if(! dvb_open_devices(priv, devno, feno, dmxno, channel->pids_cnt))
		{
			mp_msg(MSGT_DEMUX, MSGL_ERR, "DVB_SET_CHANNEL2, COULDN'T OPEN DEVICES OF CARD: %d, EXIT\n", card);
			return 0;
		}
	}

	priv->card = card;
	priv->list = new_list;
	priv->retry = 5;
	new_list->current = n;
	stream->fd = priv->dvr_fd;
	mp_msg(MSGT_DEMUX, MSGL_V, "DVB_SET_CHANNEL: new channel name=%s, card: %d, channel %d\n", channel->name, card, n);

	stream->buf_pos = stream->buf_len = 0;
	stream->pos = 0;
	stream->eof = 0;

	if(channel->freq != priv->last_freq)
	{
#if DVB_API_VERSION >= 5
		if (priv->config->cards[card].type & TUNER_S2API_BASE)
		{
			if (!dvb_tune_s2(priv, channel, priv->timeout))
				return 0;
		} else
#endif
		if (! dvb_tune(priv, channel->freq, channel->pol, channel->srate, channel->diseqc, channel->tone,
			channel->inv, channel->mod, channel->gi, channel->trans, channel->bw, channel->cr, channel->cr_lp, channel->hier, priv->timeout))
			return 0;
	}

	priv->last_freq = channel->freq;
	priv->is_on = 1;

#if DVB_API_VERSION >= 5
	if (priv->config->cards[card].type & TUNER_S2API_BASE)
	{
		// set program-id property of demuxer.
		ts_prog = channel->progid;
	}
#endif

	//sets demux filters and restart the stream
	for(i = 0; i < channel->pids_cnt; i++)
	{
		if(! dvb_set_ts_filt(priv->demux_fds[i], channel->pids[i], DMX_PES_OTHER))
			return 0;
	}

	return 1;
}



int dvb_step_channel(stream_t *stream, int dir)
{
	int new_current;
	dvb_channels_list *list;
	dvb_priv_t *priv = stream->priv;

	mp_msg(MSGT_DEMUX, MSGL_V, "DVB_STEP_CHANNEL dir %d\n", dir);

	if(priv == NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_step_channel: NULL priv_ptr, quit\n");
		return 0;
	}

	list = priv->list;
	if(list == NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_step_channel: NULL list_ptr, quit\n");
		return 0;
	}

	new_current = (list->NUM_CHANNELS + list->current + (dir == DVB_CHANNEL_HIGHER ? 1 : -1)) % list->NUM_CHANNELS;

	return dvb_set_channel(stream, priv->card, new_current);
}




static void dvbin_close(stream_t *stream)
{
	int i;
	dvb_priv_t *priv  = (dvb_priv_t *) stream->priv;

	for(i = priv->demux_fds_cnt-1; i >= 0; i--)
	{
		priv->demux_fds_cnt--;
		mp_msg(MSGT_DEMUX, MSGL_V, "DVBIN_CLOSE, close(%d), fd=%d, COUNT=%d\n", i, priv->demux_fds[i], priv->demux_fds_cnt);
		close(priv->demux_fds[i]);
	}
	close(priv->dvr_fd);

	close(priv->fe_fd);
	priv->fe_fd = priv->sec_fd = priv->dvr_fd = -1;

	priv->is_on = 0;
	dvb_free_config(priv->config);
}


static int dvb_streaming_start(stream_t *stream, struct stream_priv_s *opts, int tuner_type, char *progname)
{
	int i;
	dvb_channel_t *channel = NULL;
	dvb_priv_t *priv = stream->priv;

	mp_msg(MSGT_DEMUX, MSGL_V, "\r\ndvb_streaming_start(PROG: %s, CARD: %d, FILE: %s)\r\n",
	    opts->prog, opts->card, opts->file);

	priv->is_on = 0;

	i = 0;
	while((channel == NULL) && i < priv->list->NUM_CHANNELS)
	{
		if(! strcmp(priv->list->channels[i].name, progname))
			channel = &(priv->list->channels[i]);

		i++;
	}

	if(channel != NULL)
	{
		priv->list->current = i-1;
		mp_msg(MSGT_DEMUX, MSGL_V, "PROGRAM NUMBER %d: name=%s, freq=%u\n", i-1, channel->name, channel->freq);
	}
	else
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "\n\nDVBIN: no such channel \"%s\"\n\n", progname);
		return 0;
	}


	if(!dvb_set_channel(stream, priv->card, priv->list->current))
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "ERROR, COULDN'T SET CHANNEL  %i: ", priv->list->current);
		dvbin_close(stream);
		return 0;
	}

	mp_msg(MSGT_DEMUX, MSGL_V,  "SUCCESSFUL EXIT from dvb_streaming_start\n");

	return 1;
}




static int dvb_open(stream_t *stream, int mode, void *opts, int *file_format)
{
	// I don't force  the file format bacause, although it's almost always TS,
	// there are some providers that stream an IP multicast with M$ Mpeg4 inside
	struct stream_priv_s* p = (struct stream_priv_s*)opts;
	dvb_priv_t *priv;
	char *progname;
	int tuner_type = 0, i;


	if(mode != STREAM_READ)
		return STREAM_UNSUPPORTED;

	stream->priv = calloc(1, sizeof(dvb_priv_t));
	if(stream->priv ==  NULL)
		return STREAM_ERROR;

	if(p->dmx == -1)
		p->dmx = p->fe;

	priv = (dvb_priv_t *)stream->priv;
	priv->fe_fd = priv->sec_fd = priv->dvr_fd = -1;
	priv->config = dvb_get_config(p->file);
	if(priv->config == NULL)
	{
		free(priv);
		mp_msg(MSGT_DEMUX, MSGL_ERR, "DVB CONFIGURATION IS EMPTY, exit\n");
		return STREAM_ERROR;
	}

	priv->card = -1;
	for(i=0; i<priv->config->count; i++)
	{
		if(priv->config->cards[i].devno == p->card &&
		   priv->config->cards[i].feno == p->fe &&
		   priv->config->cards[i].dmxno == p->dmx)
		{
			priv->card = i;
			break;
		}
	}

	if(priv->card == -1)
 	{
		free(priv);
		mp_msg(MSGT_DEMUX, MSGL_ERR, "NO CONFIGURATION FOUND FOR CARD N. %d, exit\n", p->card);
 		return STREAM_ERROR;
 	}
	priv->timeout = p->timeout;

	tuner_type = priv->config->cards[priv->card].type;

	if(tuner_type == 0)
	{
		free(priv);
		mp_msg(MSGT_DEMUX, MSGL_V, "OPEN_DVB: UNKNOWN OR UNDETECTABLE TUNER TYPE, EXIT\n");
		return STREAM_ERROR;
	}


	priv->tuner_type = tuner_type;

	mp_msg(MSGT_DEMUX, MSGL_V, "OPEN_DVB: prog=%s, card=%d, type=%d\n",
		p->prog, priv->card, priv->tuner_type);

	priv->list = priv->config->cards[priv->card].list;

	if((! strcmp(p->prog, "")) && (priv->list != NULL))
		progname = priv->list->channels[0].name;
	else
		progname = p->prog;


	if(! dvb_streaming_start(stream, p, tuner_type, progname))
	{
		free(stream->priv);
		stream->priv = NULL;
		return STREAM_ERROR;
	}

	stream->type = STREAMTYPE_DVB;
	stream->fill_buffer = dvb_streaming_read;
	stream->close = dvbin_close;
	m_struct_free(&stream_opts, opts);

	*file_format = DEMUXER_TYPE_MPEG_TS;

	return STREAM_OK;
}

dvb_config_t *dvb_get_config(char *custom_conf)
{
	int i, fd, type, size;
	char filename[30], *conf_file, *name;
	dvb_channels_list *list;
	dvb_card_config_t *cards = NULL, *tmp;
	dvb_config_t *conf = NULL;
	int j;
  int type_s = 0;


	conf = malloc(sizeof(dvb_config_t));
	if(conf == NULL)
		return NULL;

	conf->priv = NULL;
	conf->count = 0;
	conf->cards = NULL;
	for(i=0; i<MAX_CARDS; i++)
	{
		for(j=0; j<MAX_FE; j++)
		{
		snprintf(filename, sizeof(filename), "/dev/dvb/adapter%d/frontend%d", i, j);
		fd = open(filename, O_RDONLY|O_NONBLOCK);
		if(fd < 0)
		{
			mp_msg(MSGT_DEMUX, MSGL_V, "DVB_CONFIG, can't open device %s, skipping\n", filename);
			continue;
		}

#if DVB_API_VERSION >= 5
		type_s = dvb_get_tuner_type_s2(fd);
#endif
		type = dvb_get_tuner_type(fd);
		close(fd);
#if DVB_API_VERSION >= 5
#define FORCE_S2API 1
#if FORCE_S2API
		if (type_s == 0 && type == TUNER_TER)
			type_s = TUNER_S2API_BASE + SYS_ISDBT;
		else if (type_s == 0 && type == TUNER_SAT)
			type_s = TUNER_S2API_BASE + SYS_ISDBS;
#endif
fallback:
		if (type_s !=0)
			conf_file = get_path("channels.conf.s2");
		else
		{
#endif
		if(type != TUNER_SAT && type != TUNER_TER && type != TUNER_CBL && type != TUNER_ATSC)
		{
			mp_msg(MSGT_DEMUX, MSGL_V, "DVB_CONFIG, can't detect tuner type of card %d, skipping\n", i);
			continue;
		}

		conf_file = get_path("channels.conf");
		switch(type)
		{
			case TUNER_TER:
			conf_file = get_path("channels.conf.ter");
				break;
			case TUNER_CBL:
			conf_file = get_path("channels.conf.cbl");
				break;
			case TUNER_SAT:
			conf_file = get_path("channels.conf.sat");
				break;
			case TUNER_ATSC:
			conf_file = get_path("channels.conf.atsc");
				break;
		}

#if DVB_API_VERSION >= 5
		} // if (type != 0) else {}
#endif
		if((access(conf_file, F_OK | R_OK) != 0))
		{
			free(conf_file);
			conf_file = get_path("channels.conf");
			if((access(conf_file, F_OK | R_OK) != 0))
			{
				free(conf_file);
				conf_file = strdup(MPLAYER_CONFDIR "/channels.conf");
			}
		}
		if (custom_conf)
		{
			free(conf_file);
			if (*custom_conf == '/')
				conf_file = strdup(custom_conf);
			else
				conf_file = get_path(custom_conf);
		}

		if (type_s > 0)
			list = dvb_get_channels(conf_file, type_s);
		else
			list = dvb_get_channels(conf_file, type);
		free(conf_file);
#if DVB_API_VERSION >= 5
		if (list == NULL && type_s > TUNER_S2API_BASE) {
			mp_msg(MSGT_DEMUX, MSGL_INFO, "S2API failed. falling back to the old API.\n");
			type_s = 0;
			goto fallback;
		}
		else
#endif
		if(list == NULL)
			continue;

		size = sizeof(dvb_card_config_t) * (conf->count + 1);
		tmp = realloc(conf->cards, size);

		if(tmp == NULL)
		{
			fprintf(stderr, "DVB_CONFIG, can't realloc %d bytes, skipping\n", size);
			continue;
		}
		cards = tmp;

		name = malloc(32);
		if(name==NULL)
		{
			fprintf(stderr, "DVB_CONFIG, can't realloc 32 bytes, skipping\n");
			continue;
		}

		conf->cards = cards;
		conf->cards[conf->count].devno = i;
		conf->cards[conf->count].feno = j;
		conf->cards[conf->count].dmxno = j; // FIXME: expand with dmxno?
		conf->cards[conf->count].list = list;
		conf->cards[conf->count].type = type;
#if DVB_API_VERSION >= 5
		if (type_s > TUNER_S2API_BASE) {
			conf->cards[conf->count].type = type_s;
			snprintf(name, 32, "%s(S2API) card #. %d",
				dvb_get_sysname(type), conf->count);
		} else
#endif
		snprintf(name, 32, "DVB-%c card n. %d", type==TUNER_TER ? 'T' : (type==TUNER_CBL ? 'C' : 'S'), conf->count);
		conf->cards[conf->count].name = name;
		conf->count++;
    }
	}

	mp_msg(MSGT_DEMUX, MSGL_V, "%d cards found.\n", conf->count);
	if(conf->count == 0)
	{
		free(conf);
		conf = NULL;
	}

	return conf;
}



const stream_info_t stream_info_dvb = {
	"Dvb Input",
	"dvbin",
	"Nico",
	"based on the code from ??? (probably Arpi)",
	dvb_open,
	{ "dvb", NULL },
	&stream_opts,
	1 				// Urls are an option string
};
