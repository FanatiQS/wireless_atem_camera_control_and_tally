// @ts-check

// Creates overlay iframe without rendering it in the DOM
const iframe = document.createElement("iframe");
iframe.style.display = "none";
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
	iframe.src = `${import.meta.url.slice(0, import.meta.url.lastIndexOf("."))}.html?addr=${addr}`;

	// Returns promise resolved when configuration is submitted or canceled
	await iframeLoad;
	// @ts-ignore
	return iframe.contentWindow.promise;
}
