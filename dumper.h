				/* 5 bytes per 36-bit word */
				/* 518 word logical blocks */
#define TAPEBLK  518*5*15

				/* Checksum is first word */
#define WdoffChecksum      0
#define BtoffChecksum      0
#define BtlenChecksum     36
				/* Page access bits is second word */
#define WdoffAccess        1
#define BtoffAccess        0
#define BtlenAccess       36
				/* SCD, first 3 bits in next word */
#define WdoffSCD           2
#define BtoffSCD           0
#define BtlenSCD           3
				/* Number of saveset on tape */
#define WdoffSaveSetNum    2
#define BtoffSaveSetNum    3
#define BtlenSaveSetNum   15
				/* Tape number of dump */
#define WdoffTapeNum       2
#define BtoffTapeNum      18
#define BtlenTapeNum      18
				/* F1, F2 Flag bits */
#define WdoffF1F2          3
#define BtoffF1F2          0
#define BtlenF1F2          2
				/* File Number in Set (new format only) */
#define WdoffFileNum       3
#define BtoffFileNum       2
#define BtlenFileNum      16
				/* Page Number in file */
#define WdoffPageNum       3
#define BtoffPageNum      18
#define BtlenPageNum      18
				/* Record type (2's complement) */
#define WdoffRectype       4
#define BtoffRectype       0
#define BtlenRectype      36
				/* Record sequence number */
#define WdoffRecseq        5
#define BtoffRecseq        0
#define BtlenRecseq       36


				/* SCD Values */
#define SCDNormal       0
#define SCDCollection   1
#define SCDArchive      2
#define SCDMigration    3

				/* F1, F2 Values */
#define F1F2Old            0
#define F1F2OldContinue    3
#define F1F2New            1
#define F1F2NewContinue    2

				/* Record type values */
#define RectypeData     0
#define RectypeTphd     1
#define RectypeFlhd     2
#define RectypeFltr     3
#define RectypeTptr     4
#define RectypeUsr      5
#define RectypeCtph     6
#define RectypeFill     7

char *rectypes[] = {
    "DATA",
    "ISSH",
    "FLHD",
    "FLTR",
    "TPTR",
    "UDIR",
    "CSSH",
    "FILL",
};

#define BtoffWord       0
#define BtlenWord       36

#define WdoffSSDate        8            /* Saveset date offset (type 1, 6) */
#define WdoffSSName        9            /* Saveset name offset (type 1, 6) */
#define WdoffFLName        6            /* Filename offset (type 2) */
#define WdoffFDB         134            /* FDB offset (type 2) */

#define WdoffFDB_CTL	01+WdoffFDB	/* Control word .FBCTL */

#define BtoffFDB_Arc	11		/* archived */
#define BtlenFDB_Arc	1

#define BtoffFDB_Inv	12		/* invisible */
#define BtlenFDB_Inv	1

#define BtoffFDB_Off	13		/* offline */
#define BtlenFDB_Off	1

#define WdoffFDB_PRT     04+WdoffFDB	/* protection */
#define BtoffFDB_PRT       18
#define BtlenFDB_PRT       18

#define WdoffFDB_BSZ     011+WdoffFDB	/* Number of bits per byte */
#define BtoffFDB_BSZ       6
#define BtlenFDB_BSZ       6

#define WdoffFDB_PGC     011+WdoffFDB	/* Number of pages in the file */
#define BtoffFDB_PGC      18
#define BtlenFDB_PGC      18

#define WdoffFDB_Size    012+WdoffFDB	/* Number of bytes in the file */

#define BtoffFDB_Size      0
#define BtlenFDB_Size     36

#define WdoffFDB_Wrt     014+WdoffFDB	/* Date of last write to file */

#define WdoffFDB_Ref     015+WdoffFDB	/* read time */

#define WdoffFDB_PGC_A	022+WdoffFDB	/* Pagecount before archive */

#define WdoffFDB_TP1	033+WdoffFDB	/* Tape ID for archive run 1 */

#define WdoffFDB_SS1	034+WdoffFDB	/* Saveset # for archive run 1 */
#define BtoffFDB_SS	0
#define BtlenFDB_SS	18
#define WdoffFDB_TF1	034+WdoffFDB	/* Tape file # for archive run 1 */
#define BtoffFDB_TF	18
#define BtlenFDB_TF	18

#define WdoffFDB_TP2	035+WdoffFDB	/* Tape ID for archive run 2 */
#define WdoffFDB_SS2	036+WdoffFDB	/* Saveset # for archive run 2 */
#define WdoffFDB_TF2	036+WdoffFDB	/* Tape file # for archive run 2 */
