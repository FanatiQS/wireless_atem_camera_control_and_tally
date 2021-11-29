## ATEM TlIn length mystery has probably been solved
The TlIn command length missmatch is related to padding.
When padded to a 32bit boundary, the math seem to work.

## ATEM tally command length exploration
The length of the tally command does not match the number of indexes it transmits.
This document is hopefully going to help figure out the correlation between the command length and the number of indexes.

### ATEM television studio
* indexes = 6
* cmdLen = 16

### ATEM television studio HD
* indexes = 8
* cmdLen = 20

### ATEM 2M/E Production Switcher
* indexes = 10 (cant remember if it was hex or base 10)
* cmdLen = 28

### ATEM Constelation
* indexes = 0x28 (40)
* cmdLen = 52



## Theory
It starts making sense to me now.
ATEM television studio has 6 cameras, 8 checksum bytes and 2 length bytes.
That is 6 + 10 = 16. 16 divides evenly by 4.
ATEM television studio HD has 8 cameras.
That is 8 + 10 = 18. 18 does not divide evenly by 4. Closest number above is 20.
ATEM constelation has 40 cameras.
That is 40 + 10 = 50. 50 does not divide evently by 4. Closest number above is 52.
ATEM 2M/E production switcher has 16 cameras.
That is 16 + 10 = 26. 26 does not divide evently by 4. Closest number above is 28.

### Outcome
The TlIn command consists of 2 bytes for length, n bytes for tally indexes, 0-3 bytes for padding and 8 bytes for checksum.
