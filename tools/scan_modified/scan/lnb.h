
struct lnb_types_st {
	char	*name;
	char	**desc;
	unsigned long	low_val;
	unsigned long	high_val;	/* zero indicates no hiband */
	unsigned long	switch_val;	/* zero indicates no hiband */
};

/* Enumerate through standard types of LNB's until NULL returned.
 * Increment curno each time
 */

struct lnb_types_st *
lnb_enum(int curno);

/* Decode an lnb type, for example given on a command line
 * If alpha and standard type, e.g. "Universal" then match that
 * otherwise low[,high[,switch]]
 */

int
lnb_decode(char *str, struct lnb_types_st *lnbp);

