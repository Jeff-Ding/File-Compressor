# File Compressor
Command line utility for losslessly encoding and decoding input and files into/from compressed codes of user-specified bit-length, based on the Lempel–Ziv–Welch algorithm (http://www.cs.duke.edu/courses/spring03/cps296.5/papers/welch_1984_technique_for.pdf).

### Usage
encode [-m MAXBITS | -o NAME | -i NAME | -p USED]*

decode [-o NAME]*

### Description
encode reads a stream of characters from the standard input, compresses it
using the Lempel-Ziv-Welch algorithm, and writes the stream of codes to the
standard output as a stream of bits packed into 8-bit bytes.  decode reads
from the standard input a byte stream written by encode, decompresses the
stream of codes, and writes the stream of characters to the standard output.

encode writes codes using the smallest number of bits required to specify valid
codes at the time (e.g., 9 bits when there are 512 valid codes, but 10 bits
once the next code is assigned), up to a maximum of 12 bits (or MAXBITS if the
-m flag is specified).  In effect, this limit determines the maximum size of
the string table.

### Flags
With the -o NAME option, the final string table (without usage counts) is
dumped (in some reasonably compact format) to the file NAME after stdin is
processed.

With the -i NAME option, the initial string table is read from the file NAME
(which must have been written during a previous execution of encode or decode)
rather than consisting of only the one-character strings.

When all possible codes have been assigned, the default is to stop assigning
codes.  However, with the -p (pruning) option, encode and decode instead create
a new string table containing:
* every one-character string;
* every string that was found at least USED times (i.e., encountered during
  encode's greedy search or decode's printing of the string)
* every nonempty prefix of a string in the table.
