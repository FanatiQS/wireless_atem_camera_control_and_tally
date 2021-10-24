# Camera Control payload from ATEM command CCdP
The camera control data contained in the ATEM protocol is not structured the same as the "Blackmagic SDI Camera Control Protocol"

```
     0          1          2          3          4          5          6          7
+----------+----------+----------+----------+----------+----------+----------+----------+
|   dest   | category |   param  |   type   |     ?    |  8bits   |     ?    |  16bits  |
+----------+----------+----------+----------+----------+----------+----------+----------+
|     ?    |  32bits  |     ?    |     ?    |     ?    |     ?    |     ?    |     ?    |
+----------+----------+----------+----------+----------+----------+----------+----------+
|                                         data                                          |
+----------+----------+----------+----------+----------+----------+----------+----------+
|     ?    |     ?    |     ?    |     ?    |     ?    |     ?    |     ?    |     ?    |
+----------+----------+----------+----------+----------+----------+----------+----------+
```

Categories, parameters and data types are documented in the Blackamagic SDI Camera Control Protocol

### dest
The camera id

### category
The category containing the parameter to edit

##### Categories
	0: Lens
	1: Video
	4: Display
	8: Color Correction
	11: PTZ Control

### param
The parameter in the category to edit

##### Parameters
	Lens:
		0: Focus
		1: Instantaneous autofocus
		2: Aperture (f-stop)
		5: Instantaneous auto aperture
		9: Set continuous zoom (speed)
	Video:
		1: Gain
		2: Manual White Balance
		5: Exposure (us)
		8: Video sharpening level
		13: Gain
		16: ND (not specified in SDI protocol document version 1.3)
	Display:
		4: Color bars display time (seconds)
	Color Correction:
		0: Lift Adjust
		1: Gamma Adjust
		2: Gain Adjust
		3: Offset Adjust (cant seem to edit value from control software)
		4: Contrast Adjust
		5: Luma mix
		6: Color Adjust
	PTZ Control:
		0: Pan/Tilt Velocity (cant edit value from control software)

### type
The data type used

##### Data types
	0x00: void / boolean
	0x01: 8bit signed byte
	0x02: 16bit signed integer
	0x03: 32bit signed integer
	0x04: 64bit signed ingeger
	0x05: UTF-8 string
	0x80: 16bit 5.11 fixed point decimal

### 8bits
Number of 8bit values defined

### 16bits
Number of 16bit values defined

### 32bits
Number of 32bit values defined

### data
An array of integers with the length defined by the data type



# Notes
Byte 4,6,8,10,11,12 seem to always be 0
Byte 13, 14, 15 seem to be random (might be data left from previous packet in switcher?)

ATEM software control from 2020 and earlier did not support tint. This is strange since white balance and tint is the same parameter.
