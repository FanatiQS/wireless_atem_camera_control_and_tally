// @ts-check
const hasValue = window.hasOwnProperty("displayConfigPage");
const previousValue = window["displayConfigPage"];
export default function restoreWindow() {
	if (hasValue) {
		window["displayConfigPage"] = previousValue;
	}
	else {
		delete window["displayConfigPage"];
	}
}
