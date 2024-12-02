// @ts-check

"use strict";
globalThis.displayConfigPage = (() => {

// Creates overlay iframe without rendering it in the DOM just yet
const iframe = document.createElement("iframe");
iframe.style.display = "none";
iframe.style.border = "0";
iframe.style.width = "100%";
iframe.style.height = "100%";
iframe.style.position = "absolute";
iframe.style.top = "0";
iframe.style.left = "0";
iframe.style.zIndex = Number.MAX_SAFE_INTEGER.toString();
window.addEventListener("load", () => document.body.appendChild(iframe), { once: true });

/**
 * Displayes the configuration page from a waccat device on top of the current DOM
 * @param {string} addr The ip address of the waccat device to display configuration for
 * @param {boolean=} alertErrors If errors should be alerted to the user
 * @returns {Promise<FormData|null>} Submitted data on submit or null on cancel/error
 */
return async function displayConfigPage(addr, alertErrors) {
	try {
		// Sets iframe content to device config page without CORS being an issue
		const deviceConfigPageResponse = await fetch(`http://${addr}`);
		if (!deviceConfigPageResponse.ok) {
			throw new Error(deviceConfigPageResponse.statusText);
		}
		iframe.srcdoc = await deviceConfigPageResponse.text();
		await new Promise((resolve) => iframe.addEventListener("load", resolve, { once: true }));
		const contentDocument = /** @type {Document} */(iframe.contentDocument);
		const body = contentDocument.body;

		// Adds additional styles for on and off animations
		const styles = document.createElement("style");
		styles.innerHTML = `
			/* Animate on background blur when loaded */
			body {
				animation: config-page-blur .8s;
				backdrop-filter: blur(1px);
				-webkit-backdrop-filter: blur(1px);
			}
			@keyframes config-page-blur {
				from {
					backdrop-filter: blur(0);
					-webkit-backdrop-filter: blur(0);
				}
				to {
					backdrop-filter: blur(1px);
					-webkit-backdrop-filter: blur(1px);
				}
			}

			/* Animate form and background blur away when canceled or submitted */
			body {
				transition: -webkit-backdrop-filter 0.8s;
			}
			form {
				transition: transform 0.8s;
			}
			.dying {
				backdrop-filter: blur(0);
				-webkit-backdrop-filter: blur(0);
			}
			.dying form {
				transform: translateY(calc(-100% - 2em));
			}
		`;
		contentDocument.head.appendChild(styles);

		// Renders host iframe in DOM
		iframe.style.display = "block";

		// Delays events until after intro animation is finished
		await new Promise((resolve) => body.addEventListener("animationend", resolve, { once: true }));

		let returnValue = null;
		const form = /** @type {HTMLFormElement} */(contentDocument.querySelector("form"));
		await new Promise((/** @type {any} */resolve) => {
			// Disables overlay form when clicked outside form element
			body.onclick = (event) => {
				// Ignores bubbling events
				if (event.target !== body) return;

				// Resolves promise indicating no configuration was sent
				resolve();
			};

			// Interrupts form submit
			form.onsubmit = (event) => {
				// Prevents submitting form since that updates the content of the page
				event.preventDefault();

				// Resolves promise indicating configuration was successfully sent
				resolve();

				// Manually submits form
				const formData = new FormData(form);
				returnValue = fetch(`http://${addr}`, { method: "POST", body: formData }).then((postResponse) => {
					if (!postResponse.ok) throw new Error(postResponse.statusText);
					return formData;
				});
			};
		});

		// Prevent multiple resolves from running at the same time
		form.onsubmit = null;
		body.onclick = null;

		// Animates page off screen
		body.classList.add("dying");

		// Disables host iframe when animation is done without blocking submit and return
		body.addEventListener("transitionend", () => iframe.style.display = "none", { once: true });

		// Returns form data after submit or null if canceled
		return await returnValue;
	}
	catch (err) {
		// Opens alert box with error message if query parameter is set to alert
		if (alertErrors) {
			alert(err.message);
		}

		// Rethrows error up the chain
		throw err;
	}
}

})();
