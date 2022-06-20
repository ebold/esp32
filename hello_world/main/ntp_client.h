/*
 * ntp_client.h
 *
 * Derived from the standard LwIP SNTP example [1].
 *
 *  Created on: Jun 19, 2022
 *      Author: ebold
 *
 * [1] LwIP SNTP example, https://github.com/espressif/esp-idf/tree/v4.4.1/examples/protocols/sntp
 */

#ifndef MAIN_NTP_CLIENT_H_
#define MAIN_NTP_CLIENT_H_

#include "esp_log.h"
#include "esp_sntp.h"
#include "time.h"

/* Configuration of a NTP server */
#define MY_NTP_SERVER "de.pool.ntp.org"

void nc_init(sntp_sync_time_cb_t callback);

#endif /* MAIN_NTP_CLIENT_H_ */
