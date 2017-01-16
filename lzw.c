#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "lzwHelper.h"
#include "/c/cs323/Hwk4/code.h"

#define NAME_LEN (6) // "encode" or "decode"
#define MAXBITS_DEFAULT (12)
#define USAGE_ERR (1)
#define FILE_ERR (2)
#define ENC_USE_ERR_LEN (58)
#define DEC_USE_ERR_LEN (25)


static void encode(long maxBits, FILE *out, FILE *in, char *inFile, int used)
{
    if (in == NULL)
        printf("%ld:%d:0:", maxBits, used);
    else
        printf("%ld:%d:%zu:%s", maxBits, used, strlen(inFile), inFile);

    // initialize string table to contain all ASCII values
    STable sTable = sTableCreate(in);

    Code code = CODE_EMPTY; // current CODE
    int k; // current CHAR
    while ((k = getchar()) != EOF) {
        Code tempCode; // stores lookup result

        // new word, output code and add new entry to string table
        if ((tempCode = lookupCode(code, k, sTable)) == CODE_NOT_FOUND) {
            putBits(bitsPerCode(numEntries(sTable)), code-1);

            // only assign codes if still codes left
            if (bitsPerCode(numEntries(sTable) + 1) <= maxBits) {
                sTableInsert(code, k, sTable);

                // check for overflow
                if (numEntries(sTable) == SIZE_MAX) {
                    fprintf(stderr, "encode: Overflow error\n");
                }

                if (used > 0) {
                    // current insert was good, but next one exceeds maxBits
                    if (bitsPerCode(numEntries(sTable) + 1) > maxBits)
                        sTablePrune(used, sTable);
                }
            }
                
            code = lookupCode(CODE_EMPTY, k, sTable); // next round
        } else // word (c, k) know, grow word
            code = tempCode; // set to code associated with (c, k)

        incrementUsage(code, sTable);
    }

    if (code != CODE_EMPTY)
        putBits(bitsPerCode(numEntries(sTable)), code-1);

    flushBits();

    // dump string table to file if specified
    if (out != NULL)
        sTableDump(out, sTable);

    sTableDestroy(sTable);
}


static void decode(FILE *out)
{
    long maxBits;
    int used;
    int inFileLen;
    FILE *in = NULL;
    int numRead; // number of values read in by scanf
    numRead = scanf("%ld:%d:%d:", &maxBits, &used, &inFileLen);

    if (numRead != 3 || maxBits < MINBITS || maxBits > MAXBITS_ABSOLUTE) {
        fprintf(stderr, "decode: Invalid header\n");
        return;
    }

    // read in input file
    if (inFileLen > 0) {
        char inFile[inFileLen + 1];
        assert(fgets(inFile, inFileLen + 1, stdin) != NULL);
        in = fopen(inFile, "r");
        if (in == NULL) {
            fprintf(stderr, "decode: Unable to load file %s\n", inFile);
            return;
        }
    }


    // initialize string table to contain all ASCII values
    STable sTable = sTableCreate(in);

    Code last = CODE_EMPTY; // CODE associated with last (PREF, CHAR)  added to string table
    Code newCode; // most recently read CODE from stdin
    Code code; // current CODE
    int finalK = CHAR_UNKNOWN; // CHAR of last CODE
    KStack kStack = kStackCreate(); // CHARs to output
    while ((newCode = code = getBits(bitsPerCode(numEntries(sTable)))) != EOF) {
        code++; newCode++;

        // check for corrupt input
        if (code != CODE_NOT_FOUND && code > numEntries(sTable)) {
            fprintf(stderr, "decode: Invalid code: %zu\n", code);
            return;
        }

        // KwKwK case
        if (lookupChar(code, sTable) == CHAR_UNKNOWN) {
            incrementUsage(code, sTable);
            assert(finalK != CHAR_UNKNOWN);
            kStackPush(finalK, kStack);
            code = lookupPref(code, sTable); // keep unpacking
        }

        // unpack (PREF, CHAR)
        Code tempPref;
        while ((tempPref = lookupPref(code, sTable)) != CODE_EMPTY) {
            incrementUsage(code, sTable);
            int tempChar = lookupChar(code, sTable); // associated CHAR

            kStackPush(tempChar, kStack);
            code = tempPref;
        }

        // ouput last CHAR from lookup (first of word)
        incrementUsage(code, sTable);
        finalK = lookupChar(code, sTable);
        putchar(finalK);

        // output rest of CHARs in word
        while (!kStackEmpty(kStack))
            putchar(kStackPop(kStack));

        // CHAR of last now known, update if unknown
        if (last != CODE_EMPTY && lookupChar(last, sTable) == CHAR_UNKNOWN)
            replaceLastChar(finalK, last, sTable);

        // add new entry to string table if still codes left
        if (bitsPerCode(numEntries(sTable) + 1) <= maxBits) {
            sTableInsert(newCode, CHAR_UNKNOWN, sTable);
            
            // check for overflow
            if (numEntries(sTable) == SIZE_MAX)
                fprintf(stderr, "decode: Overflow error\n");

            if (used > 0) {
                // current insert was good, but next one exceeds maxBits
                if (bitsPerCode(numEntries(sTable) + 1) > maxBits)
                    sTablePrune(used, sTable);
            }

            last = lookupCode(newCode, CHAR_UNKNOWN, sTable);
        }
    }

    // dump string table to file if specified
    if (out != NULL)
        sTableDump(out, sTable);

    kStackDestroy(kStack);
    sTableDestroy(sTable);
}


int main(int argc, char **argv)
{
    int argv0Len = strlen(argv[0]);
    char encUseErr[ENC_USE_ERR_LEN] =
        "Usage: encode [-m MAXBITS | -o NAME | -i NAME | -p USED]*\0";
    char decUseErr[DEC_USE_ERR_LEN] =
        "Usage: decode [-o NAME]*\0";

    // cmp to last 6 chars in argv[0] by pointer arithmetic
    if (strcmp(argv[0] + argv0Len - NAME_LEN, "encode") == 0) {
        long maxBits = MAXBITS_DEFAULT;
        int used = 0;
        char *outFile = NULL;
        char *inFile = NULL;
        FILE *out = NULL;
        FILE *in = NULL;

        if (argc > 1) { // options specified
            if (argc % 2 != 1) { // option args occur in pairs
                fprintf(stderr, "%s\n", encUseErr);
                return USAGE_ERR;
            }

            for (int i = 1; i < argc; i += 2) {
                if (strcmp(argv[i], "-m") == 0) {
                    char *end; // stores character after number read in strtol call
                    long temp = strtol(argv[i+1], &end, 10);
                    if (temp == 0 || temp == LONG_MIN || temp == LONG_MAX ||
                            argv[i+1][0] == '+' || argv[i+1][0] == '-' ||
                            end[0] != '\0') {
                        fprintf(stderr, "encode: Invalid MAXBITS: %s\n", argv[i+1]);
                        return USAGE_ERR;
                    } else {
                          if (temp >= MINBITS && temp <= MAXBITS_ABSOLUTE)
                            maxBits = temp;
                    }     
                } else if (strcmp(argv[i], "-o") == 0) {
                    outFile = argv[i+1];
                    out = fopen(outFile, "w");
                    if (out == NULL) {
                        fprintf(stderr, "encode: Unable to open or create file %s\n",
                                outFile);
                        return FILE_ERR;
                    }
                } else if (strcmp(argv[i], "-i") == 0) {
                    inFile = argv[i+1];
                    in = fopen(inFile, "r");
                    if (in == NULL) {
                        fprintf(stderr, "encode: Unable to open file %s\n",
                                inFile);
                        return FILE_ERR;
                    }
                } else if (strcmp(argv[i], "-p") == 0) {
                    char *end; // stores character after number read in strtol call
                    long temp = strtol(argv[i+1], &end, 10);
                    if (temp == 0 || temp == LONG_MIN || temp == LONG_MAX ||
                            argv[i+1][0] == '+' || argv[i+1][0] == '-' ||
                            end[0] != '\0') {
                            fprintf(stderr, "encode: Invalid USED: %s\n", argv[i+1]);
                            return USAGE_ERR;
                    } else
                        used = temp;
                } else
                    fprintf(stderr, "%s\n", encUseErr);
            }
        }

        encode(maxBits, out, in, inFile, used);
    } else if (strcmp(argv[0] + argv0Len - NAME_LEN, "decode") == 0) {
        char *outFile = NULL;
        FILE *out = NULL;

        if (argc > 1) { // options specified
            if (argc % 2 != 1) { // option args occur in pairs
                fprintf(stderr, "%s\n", decUseErr);
                return USAGE_ERR;
            }

            for (int i = 1; i < argc; i += 2) {
                if (strcmp(argv[i], "-o") == 0) {
                    outFile = argv[i+1];
                    out = fopen(outFile, "w");
                    if (out == NULL) {
                        fprintf(stderr, "decode: Unable to open or create file %s\n",
                                outFile);
                        return FILE_ERR;
                    }
                }
                else
                    fprintf(stderr, "%s\n", decUseErr);
            }
        }

        decode(out);
    }
}
