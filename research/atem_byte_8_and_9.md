## Trying to figure out the 8th and 9th byte in the ATEM header
No one seem to know that the 8th and 9th byte are for in the ATEM protocol.



### Failing connection by ATEM software control (version 8.6.3)
 1. 10 14 16 0b 00 00 00 00 00 9a 00 00 01 00 00 00 00 00 00 00
 2. 30 14 16 0b 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
 3. 30 14 16 0b 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
 4. 30 14 16 0b 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
 5. 30 14 16 0b 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
 6. 30 14 16 0b 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
 7. 30 14 16 0b 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
 8. 30 14 16 0b 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
 9. 30 14 16 0b 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
10. 30 14 16 0b 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
11. 30 14 16 0b 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
12. 10 14 5e c2 00 00 00 00 00 9a 00 00 01 00 00 00 00 00 00 00
13. 30 14 5e c2 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
14. 30 14 5e c2 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
15. 30 14 5e c2 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
16. 30 14 5e c2 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
17. 30 14 5e c2 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
18. 30 14 5e c2 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
19. 30 14 5e c2 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
20. 30 14 5e c2 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
21. 30 14 5e c2 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
22. 30 14 5e c2 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
23. 10 14 53 43 00 00 00 00 00 9a 00 00 01 00 00 00 00 00 00 00
24. 30 14 53 43 00 00 00 00 00 fd 00 00 01 00 00 00 00 00 00 00
25. and so on



### Data from previous version of ATEM software control
repeat
	10 14 xx xx 00 00 00 00 00 28 00 00 01 00 00 00 00 00 00 00
	repeat 10 times
		wait 200ms
		30 14 xx xx 00 00 00 00 00 0a 00 00 01 00 00 00 00 00 00 00
	wait 500ms
	10 14 xx xx 00 00 00 00 00 28 00 00 01 00 00 00 00 00 00 00
	repeat 10 times
		wait 200ms
		30 14 xx xx 00 00 00 00 00 8a 00 00 01 00 00 00 00 00 00 00
	wait 500ms