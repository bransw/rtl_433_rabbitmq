/** @file
    RabbitMQ output for rtl_433 events

    Copyright (C) 2025 RTL_433 Remote Project

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef INCLUDE_OUTPUT_RABBITMQ_H_
#define INCLUDE_OUTPUT_RABBITMQ_H_

#include "data.h"

struct mg_mgr;
struct r_cfg;

void add_rabbitmq_output(struct r_cfg *cfg, char *param);
struct data_output *data_output_rabbitmq_create(struct mg_mgr *mgr, char *param, char const *dev_hint);

#endif /* INCLUDE_OUTPUT_RABBITMQ_H_ */

