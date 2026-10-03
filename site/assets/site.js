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

  // The latest release: version and direct links, from GitHub's API. Without
  // it (offline, rate limited) the buttons keep pointing at the releases page.
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
          var wanted = platform.dataset.match;
          var asset = (release.assets || []).find(function (a) { return a.name.indexOf(wanted) >= 0; });
          if (!asset) return;
          var link = platform.querySelector(".asset");
          link.href = asset.browser_download_url;
          var size = asset.size ? " · " + (asset.size / 1048576).toFixed(0) + " MB" : "";
          platform.querySelector(".asset-name").textContent = asset.name + size;
        });
      })
      .catch(function () { /* The static links stay. */ });
  }

  // The visitor's own system, highlighted.
  var agent = navigator.userAgent;
  var here = /Windows/.test(agent) ? "windows" : /Mac OS X|Macintosh/.test(agent) ? "macos" : /Linux/.test(agent) ? "amd64.deb" : "";
  if (here) {
    document.querySelectorAll(".platform").forEach(function (platform) {
      if (platform.dataset.match.indexOf(here) >= 0) platform.classList.add("here");
    });
  }
})();
