#ifndef	ASTRIBANK_ALLOW_H
#define	ASTRIBANK_ALLOW_H

#include "mpp.h"

enum license_markers {
	LICENSE_MARKER_NONE = 0,
	LICENSE_MARKER_XORCOM = 1,
	LICENSE_MARKER_GENERIC = 2,
};

int license_marker_valid(unsigned int which);
void license_markers_help(const char *prefix, FILE *fp);

int write_to_file(
	struct eeprom_table *eeprom_table,
	struct capabilities *caps,
	struct capkey *key,
	unsigned int marker,
	FILE *f);

int read_from_file(
	struct eeprom_table *eeprom_table,
	struct capabilities *caps,
	struct capkey *capkey,
	unsigned int *used_marker,
	FILE *f);

#endif	/* ASTRIBANK_ALLOW_H */
