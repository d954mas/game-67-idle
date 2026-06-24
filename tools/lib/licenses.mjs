// Canonical SPDX license id -> human-facing license URL for catalog records.
// Single source so every cataloging tool (promote, accept_incoming, import_*)
// writes the same license_url for a given license id instead of each carrying
// its own map. Pure data leaf, no deps.
export const LICENSE_URLS = {
  "CC0-1.0": "https://creativecommons.org/publicdomain/zero/1.0/",
  "OFL-1.1": "https://openfontlicense.org/open-font-license-official-text/",
  "CC-BY-4.0": "https://creativecommons.org/licenses/by/4.0/",
  "CC-BY-SA-4.0": "https://creativecommons.org/licenses/by-sa/4.0/",
};
