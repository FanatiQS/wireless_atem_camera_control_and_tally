#include <stdint.h>

// Gets length of string, caluclated at compile time
#define STRLEN(str) (sizeof(str) - 1)

// Gets number as a string
#define INT_TO_STR(number) #number
#define INT_TO_STR2(number) INT_TO_STR(number)



// Core of templating engine to handle fomat text, length and variadic arguments differently
#define _TXT_TEMPLATE(template, fmt, value, maxlen, ...) template(fmt, __VA_ARGS__)
#define _LEN_TEMPLATE(template, fmt, value, maxlen, ...) + STRLEN(template("", __VA_ARGS__)) + maxlen
#define _ARG_TEMPLATE(template, fmt, value, maxlen, ...) , value
#define _TEMPLATE($, ...) $##_TEMPLATE(__VA_ARGS__)

// Addition to template engine core that allows adding plain HTML
#define _TXT_HTML(html) html
#define _LEN_HTML(html) + STRLEN(html)
#define _ARG_HTML(html)
#define HTML($, ...) $##_HTML(__VA_ARGS__)

// Builds printf like arguments from a template group and buffer size required for it
#define HTML_BUILD(templateGroup, ...) templateGroup(_TXT, void) templateGroup(_ARG, __VA_ARGS__)
#define HTML_GET_LEN(templateGroup) templateGroup(_LEN, void)



// HTML style for table row
#define _HTML_ROW($, label, content) HTML($, "<tr><td>" label ":<td>") content



// HTML style for table spacer
#define HTML_SPACER($) HTML($, "<tr>")

// HTML style for info row
#define _INFO_TEMPLATE(fmt, head, tail) head fmt tail
#define HTML_INFO($, label, value, len, fmt, head, tail) _HTML_ROW($, label,\
	_TEMPLATE($, _INFO_TEMPLATE, fmt, value, len, head, tail))

// HTML style for text input
#define _INPUT_TEXT_TEMPLATE(fmt, name, len) "<input required style=width:100%% maxlength="\
	INT_TO_STR2(len) " name=" name " value=" fmt ">"
#define HTML_INPUT_TEXT($, label, value, len, name) _HTML_ROW($, label,\
	_TEMPLATE($, _INPUT_TEXT_TEMPLATE, "%s", value, len, name, len))

// HTML style for number input
#define _INPUT_NUMBER_TEMPLATE(fmt, name, min, max) "<input required type=number min="\
 	min " max=" max " name=" name " value=" fmt ">"
#define HTML_INPUT_NUMBER($, label, value, max, min, name) _HTML_ROW($, label,\
	_TEMPLATE($, _INPUT_NUMBER_TEMPLATE,"%u", value, STRLEN(INT_TO_STR2(max)),\
	name, INT_TO_STR2(min), INT_TO_STR2(max)))

// HTML style for ip address input
#define _INPUT_OCTET_TEMPLATE(fmt, name) "<input required type=number min=0 max="\
	INT_TO_STR2(UINT8_MAX) " name=" name " value=" fmt ">"
#define _HTML_INPUT_OCTET($, value, name) _TEMPLATE($, _INPUT_OCTET_TEMPLATE,\
	"%u", value, STRLEN(INT_TO_STR2(UINT8_MAX)), name)
#define HTML_INPUT_IP($, label, value, name) _HTML_ROW($, label,\
	_HTML_INPUT_OCTET($, value & 0xff, name "1") HTML($, ".")\
	_HTML_INPUT_OCTET($, (value >> 8) & 0xff, name "2") HTML($, ".")\
	_HTML_INPUT_OCTET($, (value >> 16) & 0xff, name "3") HTML($, ".")\
	_HTML_INPUT_OCTET($, value >> 24, name "4"))

// HTML style for checkbox
#define _CHECKBOX_ATTRIBUTE " checked"
#define _INPUT_CHECKBOX_TEMPLATE(fmt, name) "<input type=checkbox name=" name fmt ">"
#define HTML_INPUT_CHECKBOX($, label, value, name) _HTML_ROW($, label,\
	_TEMPLATE($, _INPUT_CHECKBOX_TEMPLATE, "%s", (value) ? _CHECKBOX_ATTRIBUTE : "",\
	STRLEN(_CHECKBOX_ATTRIBUTE), name))
