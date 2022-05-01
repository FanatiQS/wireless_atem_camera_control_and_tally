#include <stdint.h>

// Gets length of string, caluclated at compile time
#define STRLEN(str) (sizeof(str) - 1)

// Gets number as a string
#define INT_TO_STR(number) #number
#define INT_TO_STR2(number) INT_TO_STR(number)



// Core of templating engine to handle fomat text, length and variadic arguments differently
#define _TXT_TEMPLATE(fmt, value, maxlen) fmt
#define _LEN_TEMPLATE(fmt, value, maxlen) + maxlen
#define _ARG_TEMPLATE(fmt, value, maxlen) , value
#define TEMPLATE($, fmt, value, maxlen) $##_TEMPLATE(fmt, value, maxlen)

// Core of templating engine that allows adding plain HTML
#define _TXT_HTML(html) html
#define _LEN_HTML(html) + STRLEN(html)
#define _ARG_HTML(html)
#define HTML($, html) $##_HTML(html)

// Builds printf like arguments from a template group and buffer size required for it
#define HTML_BUILD(templateGroup, ...) templateGroup(_TXT, void) templateGroup(_ARG, __VA_ARGS__)
#define HTML_GET_LEN(templateGroup) templateGroup(_LEN, void)



// Starts an HTML table row
#define HTML_ROW($, label) HTML($, "<tr><td>" label ":<td>")

// Creates an HTML table spacer
#define HTML_SPACER($) HTML($, "<tr>")



// Creates an HTML info row
#define HTML_INFO($, label, value, len, fmt, tail)\
	HTML_ROW($, label)\
	TEMPLATE($, fmt, value, len)\
	HTML($, tail)

// Creates an HTML text input row
#define HTML_INPUT_TEXT($, label, value, len, name)\
	HTML_ROW($, label)\
	HTML($, "<input required style=width:100%% maxlength=" INT_TO_STR2(len) " name=" name " value=")\
	TEMPLATE($, "%s", value, len)\
	HTML($, ">")

// Creates an HTML number input row
#define HTML_INPUT_NUMBER($, label, value, max, min, name)\
	HTML_ROW($, label)\
	HTML($, "<input required type=number min=" INT_TO_STR2(min)\
		" max=" INT_TO_STR2(max) " name=" name " value=")\
	TEMPLATE($,"%u", value, STRLEN(INT_TO_STR2(max)))\
	HTML($, ">")

// Creates an HTML ip address input row
#define _HTML_INPUT_OCTET($, value, name)\
	HTML($, "<input required type=number min=0 max=" INT_TO_STR2(UINT8_MAX) " name=" name " value=")\
	TEMPLATE($, "%u", value, STRLEN(INT_TO_STR2(UINT8_MAX)))\
	HTML($, ">")
#define HTML_INPUT_IP($, label, value, name)\
	HTML_ROW($, label)\
	_HTML_INPUT_OCTET($, value & 0xff, name "1")\
	HTML($, ".")\
	_HTML_INPUT_OCTET($, (value >> 8) & 0xff, name "2")\
	HTML($, ".")\
	_HTML_INPUT_OCTET($, (value >> 16) & 0xff, name "3")\
	HTML($, ".")\
	_HTML_INPUT_OCTET($, value >> 24, name "4")\

// Creates an HTML checkbox input row
#define _CHECKBOX_ATTRIBUTE " checked"
#define HTML_INPUT_CHECKBOX($, label, value, name)\
	HTML_ROW($, label)\
	HTML($, "<input type=checkbox name=" name)\
	TEMPLATE($, "%s", (value) ? _CHECKBOX_ATTRIBUTE : "", STRLEN(_CHECKBOX_ATTRIBUTE))\
	HTML($, ">")

// Creates an HTML info row containing the current time
#define HTML_CURRENT_TIME($, label)\
	HTML_ROW($, label)\
	HTML($, "<script>document.scripts[0].parentElement.innerHTML="\
	"new Date().toLocaleTimeString()</script>")

// Creates an HTML row containing time since device boot
#define HTML_TIME($, label, value)\
	HTML_ROW($, label)\
	TEMPLATE($, "%u", (uint16_t)(value / (60*60)), STRLEN(INT_TO_STR2(UINT16_MAX)))\
	HTML($, "h ")\
	TEMPLATE($, "%.2u", (uint8_t)(value / 60 % 60), 2)\
	HTML($, "m ")\
	TEMPLATE($, "%.2u", (uint8_t)(value % 60), 2)\
	HTML($, "s")

// Creates an HTML info row containing wifi signal strength
#define HTML_RSSI_STR "Not connected<td hidden>"
#define HTML_RSSI($, label, rssi, isConnected)\
	HTML_ROW($, label)\
	TEMPLATE($, "%s", (!isConnected) ? HTML_RSSI_STR : "", STRLEN(HTML_RSSI_STR))\
	TEMPLATE($, "%d", rssi, STRLEN(INT_TO_STR2(UINT8_MAX)))\
	HTML($, " dBm")
