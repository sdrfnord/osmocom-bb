/* The game Snake as Free Software for Calypso Phone */

/* (C) 2013 by Marcel `sdrfnord` McKinnon <sdrfnord@gmx.de>
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


#include <stdio.h>
#include <string.h>

#include <debug.h>
#include <memory.h>
#include <delay.h>
#include <rffe.h>
#include <keypad.h>
#include <board.h>
#include <abb/twl3025.h>
#include <rf/trf6151.h>
#include <calypso/clock.h>
#include <calypso/tpu.h>
#include <calypso/tsp.h>
#include <calypso/dsp.h>
#include <calypso/irq.h>
#include <calypso/misc.h>
#include <calypso/backlight.h>
#include <comm/sercomm.h>
#include <comm/timer.h>
#include <fb/framebuffer.h>
#include <battery/battery.h>

/* Main Program */
const char *hr = "======================================================================\n";

void key_handler(enum key_codes code, enum key_states state);

static void console_rx_cb(uint8_t dlci, struct msgb *msg)
{
	if (dlci != SC_DLCI_CONSOLE) {
		printf("Message for unknown DLCI %u\n", dlci);
		return;
	}

	printf("Message on console DLCI: '%s'\n", msg->data);
	msgb_free(msg);
}

static void l1a_l23_rx_cb(uint8_t dlci, struct msgb *msg)
{
	int i;
	puts("l1a_l23_rx_cb: ");
	for (i = 0; i < msg->len; i++)
		printf("%02x ", msg->data[i]);
	puts("\n");
}

int main(void)
{
	board_init(1);

	puts("\n\nOsmocomBB Test sdrfnord (revision " GIT_REVISION ")\n");
	puts(hr);

	/* Dump device identification */
	dump_dev_id();
	puts(hr);

	/* Dump clock config before PLL set */
	calypso_clk_dump();
	puts(hr);

	keypad_set_handler(&key_handler);

	/* Dump clock config after PLL set */
	calypso_clk_dump();
	puts(hr);

	fb_clear();
    bl_level(255);

	fb_setfg(FB_COLOR_BLACK);
	fb_setbg(FB_COLOR_WHITE);
	fb_setfont(FB_FONT_HELVB14);

	fb_gotoxy(framebuffer->width/2 - 7 * 3, 15);
	fb_putstr("Snake",framebuffer->width-4);

	fb_gotoxy(14, framebuffer->height-5);
    fb_setfont(FB_FONT_HELVR08);
	fb_putstr("Version: " GIT_SHORTHASH, framebuffer->width-4);
	fb_gotoxy(0, 0);
    fb_boxto(framebuffer->width - 1, 1);
    fb_boxto(framebuffer->width - 2, framebuffer->height-1);
    fb_boxto(0, framebuffer->height-2);
    fb_boxto(1, 1);
    /* set_pixel_r(9,9); */
    /* set_pixel_r(10,10); */
    /* set_pixel_r(10,11); */
    /* set_pixel_r(10,12); */
    /* set_pixel_r(12,10); */
    /* set_pixel_r(0,0); */
    printf("(%u, %u)\n", framebuffer->width, framebuffer->height);
	fb_gotoxy(2, 2);
    fb_lineto(framebuffer->width-3, framebuffer->height-3);
	fb_gotoxy(2, framebuffer->height-3);
    fb_lineto(framebuffer->width-3, 2);

	fb_setfg(FB_COLOR_WHITE);
	fb_setbg(FB_COLOR_BLACK);
    fb_lineto(2, 20);

	fb_flush();



	sercomm_register_rx_cb(SC_DLCI_CONSOLE, console_rx_cb);
	sercomm_register_rx_cb(SC_DLCI_L1A_L23, l1a_l23_rx_cb);

	/* beyond this point we only react to interrupts */
	puts("entering interrupt loop\n");
	while (1) {
		osmo_timers_update();
	}

	twl3025_power_off();

	while (1) {}
}

void key_handler(enum key_codes code, enum key_states state)
{
	if (state != PRESSED)
		return;

	switch (code) {
	case KEY_0: bl_level(0);break;
	case KEY_1: bl_level(10);break;
	case KEY_2: bl_level(20);break;
	case KEY_3: bl_level(30);break;
	case KEY_4: bl_level(40);break;
	case KEY_5: bl_level(50);break;
	case KEY_6: bl_level(100);break;
	case KEY_7: bl_level(150);break;
	case KEY_8: bl_level(200);break;
	case KEY_9: bl_level(255);break;
		// used to be display_puts...
		break;
	case KEY_STAR:
		// used to be display puts...
		break;
	case KEY_HASH:
		// used to be display puts...
		break;
    case KEY_LEFT_SB: bl_mode_pwl(1);break;
    case KEY_RIGHT_SB: bl_mode_pwl(0);break;
    case KEY_POWER: twl3025_power_off_now();break;
	default:
		break;
	}
}
