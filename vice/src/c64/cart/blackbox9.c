/*
 * blackbox9.c - Cartridge handling, Black BOX 9 cart.
 *
 * Written by
 *  Kamil Zbróg <kamil.zbrog@gmail.com>
 * Based on code written by
 *  Artur Sidor <ff8@poczta.wp.pl>
 * Based on code from VICE written by
 *  ALeX Kazik <alx@kazik.de>
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
 *  Andreas Boose <viceteam@t-online.de>
 *  groepaz <groepaz@gmx.net>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "vice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CARTRIDGE_INCLUDE_SLOTMAIN_API
#include "c64cartsystem.h"
#undef CARTRIDGE_INCLUDE_SLOTMAIN_API
#include "c64mem.h"
#include "cartio.h"
#include "cartridge.h"
#include "export.h"
#include "blackbox9.h"
#include "monitor.h"
#include "snapshot.h"
#include "types.h"
#include "util.h"
#include "crt.h"
#include "log.h"

/*
    Black BOX 9

   -- 16k 2 banks start from bank 1
   - cart ROM mirror is visible in io1
*/

/* #define BB9DEBUG */

#ifdef BB9DEBUG
#define DBG(x) printf x
#else
#define DBG(x)
#endif

/* some prototypes are needed */
static uint8_t blackbox_v9_io1_read(uint16_t addr);
static void blackbox9_io1_store(uint16_t addr, uint8_t);
static int blackbox_v9_dump(void);

static io_source_t io1_device = {
    CARTRIDGE_NAME_BLACKBOX9,
    IO_DETACH_CART,
    NULL,
    0xde00, 0xdeff, 0xff,
    1, /* read is always valid */
    blackbox9_io1_store,
    blackbox_v9_io1_read,
    NULL,
    blackbox_v9_dump,
    CARTRIDGE_BLACKBOX_9,
    0,
    0
};

static io_source_list_t *io1_list_item = NULL;

static const export_resource_t export_res = {
    CARTRIDGE_NAME_BLACKBOX9, 1, 1, &io1_device, NULL, CARTRIDGE_BLACKBOX_9
};

/* ---------------------------------------------------------------------*/

static int blackbox_v9_active = 0;

uint8_t blackbox_v9_rom_read(uint16_t addr)
{
    return roml_banks[(addr & 0x3fff) | (roml_bank << 14)];
}

static uint8_t blackbox_v9_io1_read(uint16_t addr)
{
  unsigned char v1;
  unsigned char dflop;
  unsigned char dflopset;
  unsigned char v6;
  unsigned char v7;

  v1 = 3;
  dflop = addr >> 6;
  dflopset = addr >> 7;
  v7 = dflopset & 1;

  v6 = (addr & 1u & (dflop & 1 ^ 1)) < 1 ? 2 : 0;
  if ( !((addr | dflop) & 1) )
    v6 = 1;
  if ( !(dflop & 1 & (addr & 1 ^ 1)) )
    v1 = v6;

  io1_device.io_source_valid = 1;

  log_message(LOG_DEFAULT, "blackbox9_io1_read addr %02x, v %02x", addr & 0xff, roml_banks[addr & 0x1FFF | (v7 << 13)]);
  cart_config_changed_slotmain((v7 << CMODE_BANK_SHIFT| v1), (v7 << CMODE_BANK_SHIFT| v1), 0);
  return roml_banks[0x1e00 + (roml_bank << 13) + (addr & 0xff)];
}

void blackbox9_io1_store(uint16_t addr, uint8_t val)
{
    unsigned char dflopclk;
    unsigned char dflop;
    unsigned char dflopset;

    dflopclk = addr >> 6 & 1;
    dflop = addr >> 7 & 1;
    dflopset = addr >> 0 & 1;

    unsigned char v4 = (addr & 1u & (dflopclk ^ 1)) < 1 ? 2 : 0;

    if ( !(((addr >> 6) | addr) & 1) )
        v4 = 1;

  unsigned char v5 = 3;
  if ( !(dflopclk & (dflopset ^ 1)) )
    v5 = v4;

  unsigned char v = ((~dflop & 1) << CMODE_BANK_SHIFT| v5);
  log_message(LOG_DEFAULT, "blackbox9_io1_store addr %04x, v %u", addr, v);
  cart_config_changed_slotmain(v, v, CMODE_READ);
}

static int blackbox_v9_dump(void)
{
    mon_out("ROM at $8000-$BFFF: %s\n", (blackbox_v9_active) ? "enabled" : "disabled");

    return 0;
}

/* ---------------------------------------------------------------------*/
void blackbox_v9_config_init(void)
{
    log_message(LOG_DEFAULT, "blackbox9_config_init");
    cart_config_changed_slotmain(CMODE_ULTIMAX | 1 << CMODE_BANK_SHIFT, CMODE_ULTIMAX | 1 << CMODE_BANK_SHIFT, CMODE_READ);
    blackbox_v9_active = 1;
}

void blackbox_v9_config_setup(uint8_t *rawcart)
{
    int i;
    for (i = 0; i <= 2; i++) {
        memcpy(&roml_banks[0x2000 * i], &rawcart[0x0000 + (0x4000 * i)], 0x2000);
        memcpy(&romh_banks[0x2000 * i], &rawcart[0x2000 + (0x4000 * i)], 0x2000);
    }

   log_message(LOG_DEFAULT, "blackbox9_config_setup %02x, %02x, %02x, %02x", roml_banks[0], roml_banks[1], roml_banks[2], roml_banks[3]);
   log_message(LOG_DEFAULT, "blackbox9_config_setup %02x, %02x, %02x, %02x", romh_banks[0], romh_banks[1], romh_banks[2], romh_banks[3]);
    cart_config_changed_slotmain(3, 3, CMODE_READ);
    blackbox_v9_active = 1;
}

/* ---------------------------------------------------------------------*/

static int blackbox_v9_common_attach(void)
{
    if (export_add(&export_res) < 0) {
        return -1;
    }

    io1_list_item = io_source_register(&io1_device);

    log_message(LOG_DEFAULT, "blackbox9_common_attach");
    return 0;
}

int blackbox_v9_bin_attach(const char *filename, uint8_t *rawcart)
{
    if (util_file_load(filename, rawcart, 0x4000, UTIL_FILE_LOAD_SKIP_ADDRESS) < 0) {
        return -1;
    }

    return blackbox_v9_common_attach();
}

int blackbox_v9_crt_attach(FILE *fd, uint8_t *rawcart)
{
    crt_chip_header_t chip;
    int i, banks = 0;

    for (i = 0; i <= 2; i++) {
        if (crt_read_chip_header(&chip, fd)) {
            break;
        }

        if (chip.bank > 2 || chip.size != 0x4000) {
            break;
        }

        if (crt_read_chip(rawcart, chip.bank << 14, &chip, fd)) {
            break;
        }
        ++banks;
    }

    if (banks != 2) {
        return -1;
    }

    return blackbox_v9_common_attach();
}

void blackbox_v9_detach(void)
{
    export_remove(&export_res);
    io_source_unregister(io1_list_item);
    io1_list_item = NULL;
}

/* ---------------------------------------------------------------------*/

static char snap_module_name[] = "CARTBLACKBOX9";
#define SNAP_MAJOR   0
#define SNAP_MINOR   1

int blackbox_v9_snapshot_write_module(snapshot_t *s)
{
    snapshot_module_t *m;

    m = snapshot_module_create(s, snap_module_name, SNAP_MAJOR, SNAP_MINOR);

    if (m == NULL) {
        return -1;
    }

    if (0
        || (SMW_B(m, (uint8_t)blackbox_v9_active) < 0)
        || (SMW_BA(m, roml_banks, 0x2000) < 0)
        || (SMW_BA(m, romh_banks, 0x2000) < 0)) {
        snapshot_module_close(m);
        return -1;
    }

    return snapshot_module_close(m);
}

int blackbox_v9_snapshot_read_module(snapshot_t *s)
{
    uint8_t vmajor, vminor;
    snapshot_module_t *m;

    m = snapshot_module_open(s, snap_module_name, &vmajor, &vminor);

    if (m == NULL) {
        return -1;
    }

    /* Do not accept versions higher than current */
    if (vmajor > SNAP_MAJOR || vminor > SNAP_MINOR) {
        snapshot_set_error(SNAPSHOT_MODULE_HIGHER_VERSION);
        goto fail;
    }

    /* new in 0.1 */
    if (SNAPVAL(vmajor, vminor, 0, 1)) {
        if (SMR_B_INT(m, &blackbox_v9_active) < 0) {
            goto fail;
        }
    } else {
        blackbox_v9_active = 0;
    }

    if (0
        || (SMR_BA(m, roml_banks, 0x2000) < 0)
        || (SMR_BA(m, romh_banks, 0x2000) < 0)) {
        goto fail;
    }

    snapshot_module_close(m);

    return blackbox_v9_common_attach();

fail:
    snapshot_module_close(m);
    return -1;
}
