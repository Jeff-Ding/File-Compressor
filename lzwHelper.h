#include <stdint.h>

#define MINBITS (9)
#define CODE_EMPTY (0)
#define CODE_NOT_FOUND (SIZE_MAX)
#define CHAR_UNKNOWN (-1)
#define MAXBITS_ABSOLUTE (20)

// type to store codes
typedef size_t Code;

// string table (hash table/array hybrid)
typedef struct sTable_t *STable;

// stack for CHARs to output during decode (implemented as array)
typedef struct kStack_t *KStack;

// create new empty string table
// load from input file if specified
// otherwise load with all ASCII values
STable sTableCreate(FILE *in);

// frees memory associated with a string table
void sTableDestroy(STable sTable);

// insert (PREF, CHAR) pair in to a string table
void sTableInsert(Code pref, int k, STable sTable);

// create new string table with entries used at least USED times
void sTablePrune(int used, STable sTable);

// increments usage count of entry in string table
void incrementUsage(Code code, STable sTable);

// replace CHAR associated with last entry in string table
void replaceLastChar(int newK, Code last, STable sTable);

// find associated CODE in a string table, given (PREF, CHAR) pair
// return 0 if not found
Code lookupCode(Code pref, int k, STable sTable);

// find associated PREF in string table, given CODE
// return 0 if not found
Code lookupPref(Code code, STable sTable);

// find associated CHAR in string table, given CODE
Code lookupChar(Code code, STable sTable);

// returns current number of entries stored in string table
size_t numEntries(STable sTable);

// returns current number of bits per code
int bitsPerCode(Code entries);

// dumps contents of string table to file
// encode: boolean that tells if currently in encode or decode
void sTableDump(FILE *out, STable sTable); 

// create CHAR stack
KStack kStackCreate(void);

// push CHAR to CHAR Stack
void kStackPush(char k, KStack kStack);

// pop CHAR off CHAR stack
char kStackPop(KStack kStack);

// returns 1 if CHAR stack empty, 0 otherwise
int kStackEmpty(KStack kStack);

// frees memory associated with a CHAR Stack
void kStackDestroy(KStack kStack);
