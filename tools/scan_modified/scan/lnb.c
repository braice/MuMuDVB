#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lnb.h"

static char *univ_desc[] = {
		"Europe",
		"10800 to 11800 MHz and 11600 to 12700 Mhz",
		"Dual LO, loband 9750, hiband 10600 MHz",
		(char *)NULL };

static char *dbs_desc[] = {
		"Expressvu, North America",
		"12200 to 12700 MHz",
		"Single LO, 11250 MHz",
		(char *)NULL };

static char *standard_desc[] = {
		"10945 to 11450 Mhz",
		"Single LO, 10000 Mhz",
		(char *)NULL };

static char *enhan_desc[] = {
		"Astra",
		"10700 to 11700 MHz",
		"Single LO, 9750 MHz",
		(char *)NULL };

static char *cband_desc[] = {
		"Big Dish - Monopoint LNBf",
		"3700 to 4200 MHz",
		"Single LO, 5150 Mhz",
		(char *)NULL };

static char *cmulti_desc[] = {
		"Big Dish - Multipoint LNBf",
		"3700 to 4200 MHz",
		"Dual LO, 5150/5750 Mhz",
		(char *)NULL };

static struct lnb_types_st lnbs[] = {
	{"UNIVERSAL",	univ_desc,		9750, 10600, 11700 },
 	{"DBS",		dbs_desc, 		11250, 0, 0 },
	{"STANDARD",	standard_desc,		10000, 0, 0 },
	{"ENHANCED",	enhan_desc,		9750, 0, 0 },
	{"C-BAND",	cband_desc,		5150, 0, 0 },
	{"C-MULTI",	cmulti_desc,		5150, 5750, 0 }
};

/* Enumerate through standard types of LNB's until NULL returned.
 * Increment curno each time
 */

struct lnb_types_st *
lnb_enum(int curno)
{
	if (curno >= (int) (sizeof(lnbs) / sizeof(lnbs[0])))
		return (struct lnb_types_st *)NULL;
	return &lnbs[curno];
}

/* Decode an lnb type, for example given on a command line
 * If alpha and standard type, e.g. "Universal" then match that
 * otherwise low[,high[,switch]]
 */

int
lnb_decode(char *str, struct lnb_types_st *lnbp)
{
int i;
char *cp, *np;

	memset(lnbp, 0, sizeof(*lnbp));
	cp = str;
	while(*cp && isspace(*cp))
		cp++;
	if (isalpha(*cp)) {
		for (i = 0; i < (int) (sizeof(lnbs) / sizeof(lnbs[0])); i++) {
			if (!strcasecmp(lnbs[i].name, cp)) {
				*lnbp = lnbs[i];
				return 1;
			}
		}
		return -1;
	}
	if (*cp == '\0' || !isdigit(*cp))
		return -1;
	lnbp->low_val = strtoul(cp, &np, 0);
	if (lnbp->low_val == 0)
		return -1;
	cp = np;
	while(*cp && (isspace(*cp) || *cp == ','))
		cp++;
	if (*cp == '\0')
		return 1;
	if (!isdigit(*cp))
		return -1;
	lnbp->high_val = strtoul(cp, &np, 0);
	cp = np;
	while(*cp && (isspace(*cp) || *cp == ','))
		cp++;
	if (*cp == '\0')
		return 1;
	if (!isdigit(*cp))
		return -1;
	lnbp->switch_val = strtoul(cp, NULL, 0);
	return 1;
}
