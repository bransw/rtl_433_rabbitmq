/** @file
    ASN.1 output for rtl_433 events using binary encoding

    Copyright (C) 2025 RTL_433 Remote Project

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef INCLUDE_OUTPUT_ASN1_H_
#define INCLUDE_OUTPUT_ASN1_H_

#include "data.h"
#include "mongoose.h"
#include "r_api.h"

#ifdef __cplusplus
extern "C" {
#endif

struct r_cfg;

/**
 * @brief Add ASN.1 output to configuration
 * 
 * @param cfg RTL433 configuration
 * @param param ASN.1 URL parameter (e.g., "asn1://guest:guest@localhost:5672/rtl_433")
 */
void add_asn1_output(struct r_cfg *cfg, char *param);

/**
 * @brief Create ASN.1 data output
 * 
 * @param mgr Mongoose manager (unused)
 * @param param ASN.1 URL parameter
 * @param dev_hint Device hint (unused)
 * @return Pointer to data output structure or NULL on error
 */
struct data_output *data_output_asn1_create(struct mg_mgr *mgr, char *param, char const *dev_hint);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_OUTPUT_ASN1_H_ */
