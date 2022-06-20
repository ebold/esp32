/*
 * ntp_client.h
 *
 * Derived from the standard LwIP SNTP example.
 *  Created on: Jun 19, 2022
 *      Author: ebold
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
