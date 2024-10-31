// @ts-check

// Creates overlay iframe without rendering it in the DOM
const iframe = document.createElement("iframe");
const parentDir = import.meta.url.slice(0, import.meta.url.lastIndexOf("/") + 1);
iframe.src = `${parentDir}config_page_module.html`;

// Sets styling for overlay iframe to cover entire screen
iframe.style.border = "0";
iframe.style.width = "100%";
iframe.style.height = "100%";
iframe.style.position = "absolute";
iframe.style.top = "0";
iframe.style.left = "0";

// Renders overlay iframe in DOM and execute its internal javascript
document.body.appendChild(iframe);

/**
 * Displayes the configuration page from a waccat device over the current DOM
 * @param {string} addr The ip address of the waccat device to display configuration for
 * @returns {Promise<boolean>} Indicates if configuration was submitted (true) or canceled (false)
*/
export async function displayConfigPage(addr) {
	// Displays config page using address argument to capture and display configuration page from
	iframe.setAttribute("data-addr", addr);
	iframe.style.display = "block";

	// Returns promise resolved when configuration is submitted or canceled
	// @ts-ignore
	await new Promise((resolve) => iframe.contentWindow.addEventListener("load", resolve));
	// @ts-ignore
	return iframe.contentWindow.promise;
}
