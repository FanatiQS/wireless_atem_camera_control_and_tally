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
