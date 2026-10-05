// The little the site needs: the gallery's lightbox, the latest release
// filled in from GitHub, the visitor's own system put first.
(function () {
  "use strict";

  // Gallery.
  var lightbox = document.getElementById("lightbox");
  if (lightbox) {
    var image = lightbox.querySelector("img");
    var caption = lightbox.querySelector("p");
    var close = function () { lightbox.hidden = true; document.body.style.overflow = ""; };
    document.querySelectorAll(".shot").forEach(function (shot) {
      shot.addEventListener("click", function (event) {
        event.preventDefault();
        image.src = shot.getAttribute("href");
        image.alt = shot.dataset.caption || "";
        caption.textContent = shot.dataset.caption || "";
        lightbox.hidden = false;
        document.body.style.overflow = "hidden";
      });
    });
    lightbox.addEventListener("click", function (event) { if (event.target !== image) close(); });
    document.addEventListener("keydown", function (event) { if (event.key === "Escape") close(); });
  }

  // The buttons are the permanent links to the latest release
  // (releases/latest/download/<file>), the same at every release. GitHub's
  // API only adds the version and the sizes; without it (offline, rate
  // limited) the links work all the same.
  var repo = document.querySelector('a[href*="github.com/"][href*="/releases/latest"]');
  var match = repo && repo.href.match(/github\.com\/([^/]+\/[^/]+)\/releases/);
  if (match) {
    fetch("https://api.github.com/repos/" + match[1] + "/releases/latest", { headers: { Accept: "application/vnd.github+json" } })
      .then(function (response) { return response.ok ? response.json() : null; })
      .then(function (release) {
        if (!release) return;
        var version = document.getElementById("release-version");
        if (version) version.textContent = (release.tag_name || "").replace(/^v/, "") + (release.published_at ? " · " + new Date(release.published_at).toLocaleDateString() : "");
        document.querySelectorAll(".platform").forEach(function (platform) {
          var asset = (release.assets || []).find(function (a) { return a.name === platform.dataset.file; });
          if (asset && asset.size)
            platform.querySelector(".asset-name").textContent = asset.name + " · " + (asset.size / 1048576).toFixed(0) + " MB";
        });
      })
      .catch(function () { /* The links work without it. */ });
  }

  // The visitor's own system, highlighted.
  var agent = navigator.userAgent;
  // Android says Linux too: it is asked first.
  var here = /Android/.test(agent) ? "android" : /Windows/.test(agent) ? "windows" : /Mac OS X|Macintosh/.test(agent) ? "macos" : /Fedora/.test(agent) ? "linux-rpm" : /Linux/.test(agent) ? "linux-deb" : "";
  if (here) {
    document.querySelectorAll(".platform").forEach(function (platform) {
      if (platform.dataset.system === here) platform.classList.add("here");
    });
  }
})();
