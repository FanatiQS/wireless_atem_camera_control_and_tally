# Issue
The camera has 6db lower gain on all positive gain values.
Tested with URSA Broadcast that only displays gain in db and not iso.

# Gain is updated in 2 ways
* The `0x01 0x0d` does not update the camera on all values, but when updated, the value in the camera matches what is in the switcher.
* The `0x01 0x01` is only sent when `0x01 0x0d` sends 6 12 and 18 db gain and it updates it to 0 6 and 12, so 6db to low.

# Gain table
db 		db hex 	iso hex	iso
 0db	0x00 	0x02	200
 6db 	0x06 	0x04	400
12db 	0x0c 	0x08	800
18db 	0x12 	0x10	1600
none	0x18 	0x20	none

# Ways to fix issue?
* Currently, `0x01 0x01` is filtered out completely and that seems to work.

* Test putting both gain data blocks in the same packet and see if the second one is filtered out by the camera then?
	* Unable to test since message grouping did not seem to work at all. Only the first command was registered no matter what came after.

* Might be that the switcher does not send that data over SDI anyway. Try checking that with SDI shield when possible.
