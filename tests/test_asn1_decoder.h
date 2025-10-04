/*
 * Test utility header for ASN.1 signal encoding/decoding
 * Contains helper functions specific to testing
 */

#ifndef TEST_ASN1_DECODER_H
#define TEST_ASN1_DECODER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read file contents into a string buffer
 * @param filename Path to file to read
 * @return Allocated string with file contents (caller must free), or NULL on error
 */
char* read_file_to_string(const char *filename);

/**
 * Show usage information for the test utility
 * @param program_name Name of the program (argv[0])
 */
void show_usage(const char *program_name);

/**
 * Decode hex ASN.1 data
 * @param hex_string Hex string to decode
 * @param use_rtl433_wrapper Whether to use RTL433Message wrapper
 */
void decode_hex_asn1(const char *hex_string, bool use_rtl433_wrapper);

#ifdef __cplusplus
}
#endif

#endif // TEST_ASN1_DECODER_H
