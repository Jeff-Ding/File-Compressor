#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "lzwHelper.h"

#define KSTACK_INIT_SIZE (512)
#define KSTACK_GROWTH_FACTOR (2)
#define KSTACK_EMPTY (-1)
#define TRUE (1)
#define FALSE (0)

// array (PREF, CHAR) entry, indexed by CODE
typedef struct arrayEntry_t {
    Code pref;
    int k;
    int used; // number of times encountered
} ArrayEntry;


// hash table (CODE, PREF, CHAR) entry
typedef struct hashEntry_t {
    struct hashEntry_t *next;
    Code code;
    Code pref;
    int k;
} HashEntry;


// string table (hash table/array hybrid)
struct sTable_t {
    size_t entries; // actual number of entries stored
    size_t hSize; // hash table size
    size_t arraySize; // array size
    HashEntry **hTable; // hash table (lookup-by-pair optimized)
    ArrayEntry *array; // array (lookup-by-code optimized)
};


// stack for CHARs to output during decode (implemented as array)
struct kStack_t {
    int top; // array index to top of stack
    int arraySize; // alloc'd size of array
    char *array;
};


// loads string table with previously written file
static void sTableLoad(FILE *in, STable sTable)
{
    Code pref;
    int k;

    while (fscanf(in, "%zu:%d\n", &pref, &k) > 1)
        sTableInsert(pref, k, sTable);

    fclose(in);
}


// create new empty string table
// load from input file if specified
// otherwise load with all ASCII values
STable sTableCreate(FILE *in)
{
    // create and alloc
    STable sTable;
    sTable = malloc(sizeof(struct sTable_t));
    assert(sTable != NULL);

    // init and alloc members
    sTable->entries = 0;
    sTable->hSize = (1 << (MAXBITS_ABSOLUTE - 2)) - 1;
    sTable->arraySize = 1 << MAXBITS_ABSOLUTE;
    sTable->hTable = malloc(sizeof(HashEntry *) * sTable->hSize);
    assert(sTable->hTable != NULL);
    sTable->array = malloc(sizeof(ArrayEntry) * sTable->arraySize);

    // no entries yet, init hash table and array as empty
    for (int i = 0; i < sTable->hSize; i++)
        sTable->hTable[i] = NULL;
    for (int i = 0; i < sTable->arraySize; i++) {
        sTable->array[i].pref = CODE_EMPTY;
        sTable->array[i].k = CHAR_UNKNOWN;
        sTable->array[i].used = 0;
    }

    // initialize with ASCII values
    for (int k = 0; k < (1 << CHAR_BIT); k++)
        sTableInsert(CODE_EMPTY, k, sTable);
    if (in != NULL) // file specified, load
        sTableLoad(in, sTable);

    return sTable;
}


// frees memory associated with a string table
void sTableDestroy(STable sTable)
{
    free(sTable->array);

    HashEntry *itr; // iterator
    HashEntry *next;

    for (int i = 0; i < sTable->hSize; i++) {
        for (itr = sTable->hTable[i]; itr != NULL; itr = next) {
            next = itr->next;
            free(itr);
        }
    }

    free(sTable->hTable);
    free(sTable);
}


// hash function (lookup-by-pair optimized)
static size_t hash(Code pref, int k)
{
    return ((unsigned)(pref) << CHAR_BIT) ^ ((unsigned)(k));
}


// insert (PREF, CHAR) pair in to a string table
void sTableInsert(Code pref, int k, STable sTable)
{
    // increment
    sTable->entries++;
    
    // insert into array
    sTable->array[sTable->entries].pref = pref;
    sTable->array[sTable->entries].k = k;
    
    // insert into hash table
    size_t hIndex = hash(pref, k) % sTable->hSize;
    HashEntry *entry = malloc(sizeof(HashEntry));
    assert(entry != NULL);
    entry->code = sTable->entries;
    entry->pref = pref;
    entry->k = k;
    entry->next = sTable->hTable[hIndex];
    sTable->hTable[hIndex] = entry;
}


// copies string (given by CODE) and its nonempty prefixes from one table to another
// reassigns codes to smallest possible for recipient table
static void copyPrefixes(Code code, STable sTable, STable sTable2)
{
    KStack kStack = kStackCreate();
    Code pref = code;

    while (pref != CODE_EMPTY) {
        kStackPush(lookupChar(pref, sTable), kStack);
        pref = lookupPref(pref, sTable);
    }

    int k = kStackPop(kStack);;
    Code c = lookupCode(CODE_EMPTY, k, sTable2);

    while (!kStackEmpty(kStack)) {
        k = kStackPop(kStack);
        Code tempC;
        if ((tempC = lookupCode(c, k, sTable2)) == CODE_NOT_FOUND) {
            sTableInsert(c, k, sTable2);
            c = lookupCode(c, k, sTable2);
        } else
            c = tempC;
    }

    kStackDestroy(kStack);
}


// create new string table with entries used at least USED times
void sTablePrune(int used, STable sTable)
{
    struct sTable_t temp; // temp structure for swap

    // create new sTable, initialize with ASCII values
    STable sTable2 = sTableCreate(NULL);

    // copy over all w/ more than USED usage, reset to 0
    for (int i = (1 << CHAR_BIT); i <= sTable->entries; i++) {
       if (sTable->array[i].used >= used)
           copyPrefixes(i, sTable, sTable2);
    }

    // swap
    temp = *sTable;
    *sTable = *sTable2;
    *sTable2 = temp;
    sTableDestroy(sTable2);
}


// increments usage count of entry in string table
void incrementUsage(Code code, STable sTable)
{
    sTable->array[code].used++;
}


// replace CHAR associated with last entry in string table
void replaceLastChar(int newK, Code last, STable sTable)
{
    // replace in hash table
    Code pref = lookupPref(last, sTable);
    int k = lookupChar(last, sTable);

    size_t hIndex = hash(pref, k) % sTable->hSize;
    HashEntry *itr; // iterator

    for (itr = sTable->hTable[hIndex]; itr != NULL; itr = itr->next) {
        if (itr->pref == pref && itr->k == k) // found
            itr->k = newK;
    }

    // replace in array
    sTable->array[last].k = newK;
}


// find associated CODE in a string table, given (PREF, CHAR) pair
// return -1 if not found
Code lookupCode(Code pref, int k, STable sTable)
{
    size_t hIndex = hash(pref, k) % sTable->hSize;
    HashEntry *itr; // iterator

    for (itr = sTable->hTable[hIndex]; itr != NULL; itr = itr->next) {
        if (itr->pref == pref && itr->k == k) { // found
            return itr->code;
        }
    }

    return CODE_NOT_FOUND; // not found
}


// find associated PREF in string table, given CODE
Code lookupPref(Code code, STable sTable)
{
    return sTable->array[code].pref;
}


// find associated CHAR in string table, given CODE
Code lookupChar(Code code, STable sTable)
{
    return sTable->array[code].k;
}


// returns current number of entries stored in string table
size_t numEntries(STable sTable)
{
    return sTable->entries;
}


// returns numBits per code, given number of entries stored
int bitsPerCode(Code entries)
{
    int bits = 0;
    while (entries >= 1) {
        entries = entries >> 1;
        bits++;
    }

    if (bits < MINBITS)
        bits = MINBITS;

    return bits;
}


// dumps contents of string table to file
void sTableDump(FILE *out, STable sTable)
{
    // ignore last entry if unknown
    if (sTable->array[sTable->entries].k == CHAR_UNKNOWN)
        sTable->entries--;

    for (int i = (1 << CHAR_BIT) + 1; i <= sTable->entries; i++) {
        fprintf(out, "%zu:%d\n",
                sTable->array[i].pref, sTable->array[i].k);
    }

    fclose(out);
}


static KStack internalKStackCreate(int size)
{
    // create and alloc
    KStack kStack = malloc(sizeof(struct kStack_t));
    assert(kStack != NULL);

    // initialize members
    kStack->top = KSTACK_EMPTY;
    kStack->arraySize = size;
    kStack->array = malloc(sizeof(char) * size);
    assert(kStack->array != NULL);

    return kStack;
}


// create CHAR stack
KStack kStackCreate(void)
{
    return internalKStackCreate(KSTACK_INIT_SIZE);
}


static void kStackGrow(KStack kStack)
{
    // new stack to return
    KStack kStack2 = internalKStackCreate(kStack->arraySize * KSTACK_GROWTH_FACTOR);
    kStack2->top = kStack->top;

    // copy contents
    for (int i = 0; i < kStack->arraySize; i++)
        kStack2->array[i] = kStack->array[i];

    // swap
    kStackDestroy(kStack);
    kStack = kStack2;
}


// push CHAR to CHAR Stack
void kStackPush(char k, KStack kStack)
{
    kStack->top++;
    if (kStack->top >= kStack->arraySize)
        kStackGrow(kStack);
    kStack->array[kStack->top] = k;
}


// pop CHAR off CHAR stack
char kStackPop(KStack kStack)
{
    assert(!kStackEmpty(kStack));
    char k = kStack->array[kStack->top];
    kStack->top--;
    return k;
}


// returns 1 if CHAR stack empty, 0 otherwise
int kStackEmpty(KStack kStack)
{
    return kStack->top == KSTACK_EMPTY;
}


// frees memory associated with a CHAR Stack
void kStackDestroy(KStack kStack)
{
    free(kStack->array);
    free(kStack);
}
