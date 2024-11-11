// @ts-check

// Path to the module without file extension for including other files related to the module
const sourcePath = import.meta.url.slice(0, import.meta.url.lastIndexOf("."));

// Sets styling for overlay iframe to cover entire screen
const style = document.createElement("link");
style.rel = "stylesheet";
style.href = `${sourcePath}.css`;
document.head.appendChild(style);

// Creates overlay iframe without rendering it in the DOM
const iframe = document.createElement("iframe");
iframe.src = `${sourcePath}.html`;
iframe.classList.add("waccat-config");
document.body.appendChild(iframe);
if (!iframe.contentWindow) {
	throw new Error("Unable to access iframe contentWindow");
}
const iframeLoad = new Promise((resolve) => iframe.addEventListener("load", resolve));

/**
 * Displayes the configuration page from a waccat device over the current DOM
 * @param {string} addr The ip address of the waccat device to display configuration for
 * @returns {Promise<boolean>} Indicates if configuration was submitted (true) or canceled (false)
 */
export async function displayConfigPage(addr) {
	// Displays config page using address argument to capture and display configuration page from
	iframe.setAttribute("data-addr", addr);

	// Returns promise resolved when configuration is submitted or canceled
	await iframeLoad;
	// @ts-ignore
	return iframe.contentWindow.promise;
}
