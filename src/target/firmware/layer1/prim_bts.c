/* Layer 1 - BTS functions */

/* (C) 2012 by Sylvain Munaut <tnt@246tnt.com>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <byteorder.h>

#include <calypso/dsp.h>

#include <layer1/l23_api.h>
#include <layer1/sync.h>
#include <layer1/tdma_sched.h>
#include <layer1/tpu_window.h>
#include <layer1/trx.h>
#include <layer1/rfch.h>


/* ------------------------------------------------------------------------ */
/* DSP extensions API                                                       */
/* ------------------------------------------------------------------------ */

#define BASE_API		0xFFD00000

#define API_DSP2ARM(x)		(BASE_API + ((x) - 0x0800) * sizeof(uint16_t))

#define BASE_API_EXT_DB0	API_DSP2ARM(0x2000)
#define BASE_API_EXT_DB1	API_DSP2ARM(0x2200)
#define BASE_API_EXT_NDB	API_DSP2ARM(0x2400)

struct dsp_ext_ndb {
	uint16_t active;
	uint16_t tsc;
} __attribute__((packed));

struct dsp_ext_db {
	/* TX command and data ptr */
	struct {
#define DSP_EXT_TX_CMD_NONE	0
#define DSP_EXT_TX_CMD_FB	1
#define DSP_EXT_TX_CMD_SB	2
#define DSP_EXT_TX_CMD_DUMMY	3
#define DSP_EXT_TX_CMD_AB	4
#define DSP_EXT_TX_CMD_NB	5
		uint16_t cmd;
		uint16_t data;
	} tx[8];

	/* RX command and data ptr */
	struct {
#define DSP_EXT_RX_CMD_NONE	0
#define DSP_EXT_RX_CMD_AB	1
#define DSP_EXT_RX_CMD_NB	2
		uint16_t cmd;
		uint16_t data;
	} rx[8];

	/* SCH data */
	uint16_t sch[2];

	/* Generic data array */
	uint16_t data[0];
} __attribute__((packed));

struct dsp_ext_api {
	struct dsp_ext_ndb *ndb;
	struct dsp_ext_db *db[2];
};

struct dsp_ext_api dsp_ext_api = {
	.ndb = (struct dsp_ext_ndb *) BASE_API_EXT_NDB,
	.db  = {
		(struct dsp_ext_db *) BASE_API_EXT_DB0,
		(struct dsp_ext_db *) BASE_API_EXT_DB1,
	},
};

static inline struct dsp_ext_db *
dsp_ext_get_db(int r_wn /* 0=W, 1=R */)
{
	int idx = r_wn ? dsp_api.r_page : dsp_api.w_page;
	return dsp_ext_api.db[idx];
}





static uint32_t
sb_build(uint8_t bsic, uint16_t t1, uint8_t t2, uint8_t t3p)
{
	return  ((t1  & 0x001) << 23) |
		((t1  & 0x1fe) <<  7) |
		((t1  & 0x600) >>  9) |
		((t2  &  0x1f) << 18) |
		((t3p &   0x1) << 24) |
		((t3p &   0x6) << 15) |
		((bsic & 0x3f) <<  2);
}


static int bts_gain = -110, max_level = -110;

static int
l1s_bts_resp(uint8_t p1, uint8_t p2, uint16_t p3)
{
	struct dsp_ext_db *db = dsp_ext_get_db(1);
	struct gsm_time rx_time;

	/* Time of the command */
	gsm_fn2gsmtime(&rx_time, l1s.current_time.fn - 2);
		/* We're shifted in time since we RX in the 'next' frame */


	if (rx_time.t3 == 0) {
		if (max_level < bts_gain) {
			bts_gain = max_level;
//			printf("raise gain to match %ddbm\n", bts_gain);
		}
		max_level = -110;
		printf("gain at %ddbm\n", bts_gain);
	}

	/* RX side */
	/* ------- */

	if (rx_time.t3 == 1) {
#if 0
		struct msgb *msg;
		struct l1ctl_bts_burst_nb_ind *bi;

		/* Create message */
		msg = l1ctl_msgb_alloc(L1CTL_BTS_BURST_NB_IND);
		if (!msg)
			goto exit;

		bi = (struct l1ctl_bts_burst_nb_ind *) msgb_put(msg, sizeof(*bi));

		/* Frame number */
		bi->fn = htonl(rx_time.fn);

		/* Timeslot */
		if (l1s.bts.mode == 0)
			bi->tn = 0;
		else
			bi->tn = 1;

		/* no bits */
		memset(bi->data, 0x00, sizeof(bi->data));

		/* Send it ! */
		l1_queue_for_l2(msg);
#endif

		return 0;
	}

	/* Access Burst ? */
	if (db->rx[0].cmd == DSP_EXT_RX_CMD_AB)
	{
#if 0
		static int energy_avg = 0x8000;

		if (db->rx[0].data > ((energy_avg * 20) >> 4))
		{
			struct msgb *msg;
			struct l1ctl_bts_burst_ab_ind *bi;
			uint16_t *iq = &db->data[32];
			int i, j;

			printf("RACH %04x %04x - %04x\n",
				db->rx[0].data, energy_avg, db->rx[1].cmd);

			/* Create message */
			msg = l1ctl_msgb_alloc(L1CTL_BTS_BURST_AB_IND);
			if (!msg)
				goto exit;

			bi = (struct l1ctl_bts_burst_ab_ind *) msgb_put(msg, sizeof(*bi));

			/* Frame number */
			bi->fn = htonl(rx_time.fn);

			/* Data (cut to 8 bits */
			bi->toa = db->rx[1].cmd;
			if (bi->toa > 68)
				goto exit;
			for (i=0,j=(db->rx[1].cmd)<<1; i<2*88; i++,j++)
				bi->iq[i] = iq[j] >> 8;

			/* Send it ! */
			l1_queue_for_l2(msg);
		} else
			energy_avg = ((energy_avg * 7) + db->rx[0].data) >> 3;
#else
		static struct l1ctl_bts_burst_ab_ind _bi[51];
		static int energy[51];
		struct l1ctl_bts_burst_ab_ind *bi = &_bi[rx_time.t3];
		int i, j;
		uint16_t *iq = &db->data[32];

		energy[rx_time.t3] = 0;

		/* Frame number */
		bi->fn = htonl(rx_time.fn);

		/* Data (cut to 8 bits */
		bi->toa = db->rx[1].cmd;
		if (bi->toa > 68)
			goto exit;
		for (i=0,j=(db->rx[1].cmd)<<1; i<2*88; i++,j++)
			bi->iq[i] = iq[j] >> 8;

		/* energy */
		energy[rx_time.t3] = db->rx[0].data;
			
		if (rx_time.t3 == 46) {
			struct msgb *msg;
			int max_energy = 0;

			/* find hottest burst */
			j = 0;
			for (i = 0; i < 51; i++) {
				if (energy[i] > max_energy) {
					max_energy = energy[i];
					j = i;
				}
				energy[i] = 0;
			}

			/* Create message */
			msg = l1ctl_msgb_alloc(L1CTL_BTS_BURST_AB_IND);
			if (!msg)
				goto exit;

			memcpy(msgb_put(msg, sizeof(*bi)), &_bi[j], sizeof(*bi));

			/* Send it ! */
			l1_queue_for_l2(msg);
		}
#endif
	}

	/* Normal Burst ? */
	else if (db->rx[0].cmd == DSP_EXT_RX_CMD_NB)
	{
		uint16_t *d = &db->data[32];
		int gain = agc_inp_dbm8_by_pm(d[1] >> 3) / 8;

		if (gain > bts_gain) {
			bts_gain = gain;
//			printf("lower gain to match %ddbm\n", bts_gain);
		}
		if (gain > max_level)
			max_level = gain;

		if (d[3] > 0x1000) {
//		if (d[3] > 0x0800) {
//		if (1) {
			struct msgb *msg;
			struct l1ctl_bts_burst_nb_ind *bi;
			int i;

//			printf("NB %04x %04x\n", d[1], d[3]);

			/* Create message */
			msg = l1ctl_msgb_alloc(L1CTL_BTS_BURST_NB_IND);
			if (!msg)
				goto exit;

			bi = (struct l1ctl_bts_burst_nb_ind *) msgb_put(msg, sizeof(*bi));

			/* Frame number */
			bi->fn = htonl(rx_time.fn);

			/* Timeslot */
			if (l1s.bts.mode == 0)
				bi->tn = 0;
			else
				bi->tn = 1;

			/* TOA */
			bi->toa = d[0];

			/* Pack bits */
			memset(bi->data, 0x00, sizeof(bi->data));

			for (i=0; i<116; i++)
			{
				int sbit = 0x0008 << ((3 - (i & 3)) << 2);
				int sword = i >> 2;
				int dbit = 1 << (7 - (i & 7));
				int dbyte = i >> 3;

				if (d[5+sword] & sbit)
					bi->data[dbyte] |= dbit;
			}

			/* Send it ! */
			l1_queue_for_l2(msg);
		}
	}

exit:
	/* We're done with this */
	dsp_api.r_page_used = 1;

	return 0;
}

static int
l1s_bts_cmd(uint8_t p1, uint8_t p2, uint16_t p3)
{
	struct dsp_ext_db *db = dsp_ext_get_db(0);

	uint32_t sb;
	uint8_t data[15];
	int type, i, t3;

	t3 = l1s.next_time.t3;

	/* RX side */
	/* ------- */

	/* Enable extensions */
	dsp_ext_api.ndb->active = 1;

//	if (t3 != 3 && t3 != 2) {
		/* Force the TSC in all cases */
		dsp_ext_api.ndb->tsc = l1s.bts.bsic & 7;
//	}


//printf("fn at bts=%d\n", t3);
	if (t3 != 2)
	{
		db->rx[0].cmd = 0;
		db->rx[0].data = 0x000;

		/* We're really a frame in advance since we RX in the next frame ! */
		t3 = t3 - 1;

		/* Select which type of burst */
		if (l1s.bts.mode == 1) /* TS 1 */
			db->rx[0].cmd = DSP_EXT_RX_CMD_NB;
		else if ((t3 >= 14) && (t3 <= 36))
			db->rx[0].cmd = DSP_EXT_RX_CMD_AB;
		else if ((t3 == 4) || (t3 == 5))
			db->rx[0].cmd = DSP_EXT_RX_CMD_AB;
		else if ((t3 == 45) || (t3 == 46))
			db->rx[0].cmd = DSP_EXT_RX_CMD_AB;
		else
			db->rx[0].cmd = DSP_EXT_RX_CMD_NB;

		/* Enable dummy bursts detection */
		dsp_api.db_w->d_ctrl_system |= (1 << B_BCCH_FREQ_IND);

		/* Enable task */
		dsp_api.db_w->d_task_d = 23;

		/* store current gain */
		uint8_t last_gain = rffe_get_gain();

		rffe_compute_gain(bts_gain, CAL_DSP_TGT_BB_LVL);

		/* Open RX window */
		if (l1s.bts.mode == 0)
			l1s_rx_win_ctrl(l1s.bts.arfcn | ARFCN_UPLINK, L1_RXWIN_NB, 0);
		else
			l1s_rx_win_ctrl(l1s.bts.arfcn | ARFCN_UPLINK, L1_RXWIN_NB, 1);

		/* restore last gain */
		rffe_set_gain(last_gain);
	}


	/* TX side */
	/* ------- */

	static int SLOT;
	if (l1s.bts.mode == 0)
		SLOT = 3;
	else
		SLOT = 0;

	/* Reset all commands to dummy burst */
	for (i=0; i<8; i++)
		db->tx[i].cmd = DSP_EXT_TX_CMD_DUMMY;

	/* Get the next burst */
	if (l1s.bts.mode == 0)
		type = trx_get_burst(l1s.next_time.fn, 0, data);
	else
		type = trx_get_burst(l1s.next_time.fn, 1, data);

	/* Program the TX commands */
	if (t3 == 2 && l1s.bts.mode == 1) {
#warning HACK: not dual tsc yet
		db->tx[SLOT].cmd = DSP_EXT_TX_CMD_DUMMY;
	} else
	switch (type) {
	case BURST_FB:
		db->tx[SLOT].cmd = DSP_EXT_TX_CMD_FB;
		break;

	case BURST_SB:
		db->tx[SLOT].cmd = DSP_EXT_TX_CMD_SB;

		sb = sb_build(
			l1s.bts.bsic,                   /* BSIC */
			l1s.next_time.t1,               /* T1   */
			l1s.next_time.t2,               /* T2   */
			(l1s.next_time.t3 - 1) / 10     /* T3'  */
		);

		db->sch[0] = sb;
		db->sch[1] = sb >> 16;

		break;

	case BURST_DUMMY:
		db->tx[SLOT].cmd = DSP_EXT_TX_CMD_DUMMY;
		break;

	case BURST_NB:
		db->tx[SLOT].cmd = DSP_EXT_TX_CMD_NB;
		db->tx[SLOT].data = sizeof(struct dsp_ext_db) / sizeof(uint16_t);

		for (i=0; i<7; i++)
			db->data[i] = (data[i<<1] << 8) | data[(i<<1)+1];
		db->data[7] = data[14] << 8;
		break;

	default:
		db->tx[SLOT].cmd = DSP_EXT_TX_CMD_DUMMY;
		break;
	}

	/* Enable the task */
	dsp_api.db_w->d_task_ra = RACH_DSP_TASK;

	/* Open TX window */
	l1s_tx_apc_helper(l1s.bts.arfcn);
//	l1s_tx_multi_win_ctrl(l1s.bts.arfcn, 0, 2, 5);
	if (l1s.bts.mode == 0)
		l1s_tx_multi_win_ctrl(l1s.bts.arfcn, 0, 2, 4);
	else
		l1s_tx_win_ctrl(l1s.bts.arfcn, L1_TXWIN_NB, 0, 6);

	return 0;
}

const struct tdma_sched_item bts_sched_set[] = {
	SCHED_ITEM_DT(l1s_bts_cmd, 2, 0, 0),	SCHED_END_FRAME(),
						SCHED_END_FRAME(),
	SCHED_ITEM(l1s_bts_resp, -5, 0, 0),	SCHED_END_FRAME(),
	SCHED_END_SET()
};
