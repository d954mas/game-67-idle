// License decision registry for game assets.
//
// This module answers one question consistently for every asset workflow:
// may this binary be committed to the public repository, and what metadata must
// travel with it?

const TRUE_TOKENS = new Set(["true", "1", "yes", "y"]);
const FALSE_TOKENS = new Set(["false", "0", "no", "n"]);

export const KNOWN_LICENSES = {
  "CC0-1.0": {
    aliases: ["cc0", "cc0-1.0", "creativecommons.org/publicdomain/zero"],
    url: "https://creativecommons.org/publicdomain/zero/1.0/",
    licenseKind: "cc",
    publishable: true,
    attributionRequired: false,
    noticeRequired: false,
    commercialUse: true,
    modificationAllowed: true,
    redistributionAllowed: true,
  },
  "CC-BY-4.0": {
    aliases: ["cc-by", "cc by", "cc-by-4.0", "creativecommons.org/licenses/by/4.0"],
    url: "https://creativecommons.org/licenses/by/4.0/",
    licenseKind: "cc",
    publishable: true,
    attributionRequired: true,
    noticeRequired: true,
    commercialUse: true,
    modificationAllowed: true,
    redistributionAllowed: true,
  },
  "CC-BY-SA-4.0": {
    aliases: ["cc-by-sa", "cc by sa", "cc-by-sa-4.0", "creativecommons.org/licenses/by-sa/4.0"],
    url: "https://creativecommons.org/licenses/by-sa/4.0/",
    licenseKind: "cc",
    publishable: true,
    attributionRequired: true,
    noticeRequired: true,
    commercialUse: true,
    modificationAllowed: true,
    redistributionAllowed: true,
    shareAlike: true,
  },
  "OFL-1.1": {
    aliases: ["ofl", "ofl-1.1", "open font license", "openfontlicense.org"],
    url: "https://openfontlicense.org/open-font-license-official-text/",
    licenseKind: "spdx",
    publishable: true,
    attributionRequired: false,
    noticeRequired: true,
    commercialUse: true,
    modificationAllowed: true,
    redistributionAllowed: true,
  },
  "MIT": {
    aliases: ["mit", "opensource.org/license/mit"],
    url: "https://opensource.org/license/mit/",
    licenseKind: "spdx",
    publishable: true,
    attributionRequired: false,
    noticeRequired: true,
    commercialUse: true,
    modificationAllowed: true,
    redistributionAllowed: true,
  },
  "Apache-2.0": {
    aliases: ["apache-2.0", "apache 2.0", "apache.org/licenses/license-2.0"],
    url: "https://www.apache.org/licenses/LICENSE-2.0",
    licenseKind: "spdx",
    publishable: true,
    attributionRequired: false,
    noticeRequired: true,
    commercialUse: true,
    modificationAllowed: true,
    redistributionAllowed: true,
  },
  "BSD-3-Clause": {
    aliases: ["bsd-3-clause", "bsd 3-clause", "bsd"],
    url: "https://opensource.org/license/bsd-3-clause/",
    licenseKind: "spdx",
    publishable: true,
    attributionRequired: false,
    noticeRequired: true,
    commercialUse: true,
    modificationAllowed: true,
    redistributionAllowed: true,
  },
  "Zlib": {
    aliases: ["zlib"],
    url: "https://opensource.org/license/zlib/",
    licenseKind: "spdx",
    publishable: true,
    attributionRequired: false,
    noticeRequired: true,
    commercialUse: true,
    modificationAllowed: true,
    redistributionAllowed: true,
  },
};

export const LICENSE_URLS = Object.fromEntries(
  Object.entries(KNOWN_LICENSES).map(([id, info]) => [id, info.url]),
);

const RESTRICTED_WORD_RE =
  /\b(paid|private|restricted|non[-_ ]?redistributable|no[-_ ]?redistribution|cgtrader|unity\s+asset\s+store|unreal\s+marketplace|marketplace|proprietary)\b/i;

export function asBool(value) {
  if (value === true) return true;
  if (value === false) return false;
  if (typeof value === "string") {
    const s = value.trim().toLowerCase();
    if (TRUE_TOKENS.has(s)) return true;
    if (FALSE_TOKENS.has(s)) return false;
  }
  return undefined;
}

function textOf(record = {}) {
  return [
    record.license,
    record.license_url,
    record.licenseUrl,
    record.license_file,
    record.licenseFile,
  ].filter(Boolean).join(" ").toLowerCase();
}

export function findKnownLicense(record = {}) {
  const text = textOf(record);
  if (!text) return null;
  for (const [id, info] of Object.entries(KNOWN_LICENSES)) {
    if (info.aliases.some((alias) => text.includes(alias.toLowerCase()))) {
      return { id, ...info };
    }
  }
  return null;
}

export function hasLicenseEvidence(record = {}) {
  return [
    record.license_url,
    record.licenseUrl,
    record.license_file,
    record.licenseFile,
  ].some((value) => String(value || "").trim());
}

export function hasAttributionInfo(record = {}) {
  const credit = [
    record.credit_text,
    record.creditText,
    record.author,
    record.author_vendor,
    record.authorVendor,
    record.vendor,
    record.creator,
    record.attribution,
    record.credit,
  ].some((value) => {
    const s = String(value || "").trim();
    return s && s.toLowerCase() !== "unknown";
  });
  const source = [
    record.source_page,
    record.sourcePage,
    record.source_page_url,
    record.sourcePageUrl,
    record.source_url,
    record.origin_url,
    record.url,
  ].some((value) => String(value || "").trim());
  return credit && source;
}

export function hasNoticeInfo(record = {}) {
  const owner = [
    record.credit_text,
    record.creditText,
    record.author,
    record.author_vendor,
    record.authorVendor,
    record.vendor,
    record.creator,
  ].some((value) => String(value || "").trim());
  return owner && hasLicenseEvidence(record);
}

export function decideLicense(record = {}) {
  const known = findKnownLicense(record);
  const explicitPublish = asBool(record.publish);
  const explicitRedistribution = asBool(record.redistribution_allowed ?? record.redistributionAllowed);
  const explicitAttribution = asBool(record.attribution_required ?? record.attributionRequired);
  const explicitNotice = asBool(record.notice_required ?? record.noticeRequired);
  const explicitCommercial = asBool(record.commercial_use ?? record.commercialUse);
  const explicitModification = asBool(record.modification_allowed ?? record.modificationAllowed);
  const licenseText = textOf(record);
  const restrictedByName = RESTRICTED_WORD_RE.test(licenseText);

  const licenseId = known?.id || "";
  const licenseKind = record.license_kind || record.licenseKind || (known?.licenseKind ?? (licenseText ? "custom" : "unknown"));
  const redistributionAllowed = explicitRedistribution ?? known?.redistributionAllowed ?? false;
  const commercialUse = explicitCommercial ?? known?.commercialUse;
  const modificationAllowed = explicitModification ?? known?.modificationAllowed;
  const attributionRequired = explicitAttribution ?? known?.attributionRequired ?? false;
  const noticeRequired = explicitNotice ?? known?.noticeRequired ?? attributionRequired;

  let publishable = false;
  if (restrictedByName) {
    publishable = false;
  } else if (known) {
    publishable = known.publishable && redistributionAllowed !== false;
  } else if (explicitPublish === true) {
    publishable = redistributionAllowed === true && hasLicenseEvidence(record);
  }
  if (explicitPublish === false) publishable = false;
  if (explicitPublish === true && !restrictedByName && known) publishable = redistributionAllowed !== false;

  return {
    licenseId,
    licenseKind,
    licenseUrl: record.license_url || record.licenseUrl || known?.url || "",
    publishable,
    redistributionAllowed,
    commercialUse,
    modificationAllowed,
    attributionRequired,
    noticeRequired,
    restrictedByName,
    known: Boolean(known),
    shareAlike: Boolean(known?.shareAlike),
  };
}

export function boolText(value, fallback = "unknown") {
  if (value === true) return "true";
  if (value === false) return "false";
  return fallback;
}

export function validateLicenseRecord(record = {}, { forPublicBinary = false, forRelease = false } = {}) {
  const issues = [];
  const warnings = [];
  const decision = decideLicense(record);
  const explicitPublish = asBool(record.publish);
  const license = String(record.license || "").trim();
  if (!license) issues.push("missing license");
  if (explicitPublish === true && !decision.publishable) {
    issues.push("publish=true requires redistribution_allowed=true, commercial_use=true, modification_allowed=true, license evidence, and a non-restricted license");
  }
  if (decision.licenseKind === "custom" && decision.publishable && !hasLicenseEvidence(record)) {
    issues.push("custom publishable license needs license_url or license_file");
  }
  if (decision.publishable && decision.redistributionAllowed !== true) {
    issues.push("publishable asset must have redistribution_allowed=true");
  }
  if (decision.publishable && decision.commercialUse !== true) {
    issues.push("publishable game asset must have commercial_use=true or a known license that allows it");
  }
  if (decision.publishable && decision.modificationAllowed !== true) {
    issues.push("publishable game asset must have modification_allowed=true or a known license that allows it");
  }
  if (decision.attributionRequired && !hasAttributionInfo(record)) {
    warnings.push("attribution-required asset needs credit/author and source_page before release");
  }
  if (decision.noticeRequired && !hasNoticeInfo(record)) {
    warnings.push("notice-required asset needs author/vendor or credit_text plus license_url/license_file before release");
  }
  if (forRelease) issues.push(...warnings);
  if (forPublicBinary && !decision.publishable) {
    issues.push("asset binary is not publishable in a public git repo");
  }
  return { ok: issues.length === 0, issues, warnings, decision };
}
