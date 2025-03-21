/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>

#include <errno.h>

#include <zephyr/sys/__assert.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include "babblekit/testcase.h"
#include "babblekit/sync.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dut, 4);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

bt_addr_le_t dut_addr = {BT_ADDR_LE_RANDOM, {{0x0A, 0x89, 0x67, 0x45, 0x23, 0xC1}}};

static void set_public_addr(void)
{
	/* dummy irk so we don't get -EINVAL because of BT_PRIVACY=y */
	uint8_t irk[16];
	int err;

	for (uint8_t i = 0; i < 16; i++) {
		irk[i] = i;
	}

	err = bt_id_create(&dut_addr, irk);
	if (err) {
		TEST_FAIL("Failed to override addr %d", err);
	}
}

static void start_advertising(uint32_t options)
{
	int err;

	struct bt_le_adv_param param =
		BT_LE_ADV_PARAM_INIT(0, BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL);
	param.options |= options;

	err = bt_le_adv_start(&param, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		TEST_FAIL("Failed to start advertising (err %d)", err);
	}
}

static void generate_new_rpa(void)
{
	/* This will generate a new RPA and mark it valid */
	struct bt_le_oob oob_local = { 0 };

	bt_le_oob_get_local(BT_ID_DEFAULT, &oob_local);
}

void dut_procedure(void)
{
	int err;

	/* open a backchannel to the peer */
	TEST_ASSERT(bk_sync_init() == 0);

	/* override public address so the scanner can test if we're using it or not */
	set_public_addr();

	LOG_DBG("enable bt");
	err = bt_enable(NULL);
	if (err) {
		TEST_FAIL("Failed to enable bluetooth (err %d)", err);
	}

	LOG_DBG("generate new RPA");
	generate_new_rpa();

	LOG_DBG("start adv with identity");
	start_advertising(BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_IDENTITY);

	/* wait for the tester to validate we're using our identity address */
	LOG_DBG("wait for validation by tester");
	bk_sync_wait();
	LOG_DBG("wait for validation by tester");
	err = bt_le_adv_stop();
	if (err) {
		TEST_FAIL("Failed to stop advertising (err %d)", err);
	}

	LOG_DBG("start adv with RPA");
	start_advertising(BT_LE_ADV_OPT_CONN);

	/* Test pass verdict is decided by the tester */
	TEST_PASS("DUT done");
}
