import { existsSync, readFileSync } from "node:fs";
import { join, relative, sep } from "node:path";

const GROUP_ORDER = [
  "player_clarity",
  "art",
  "gdd",
  "game_design",
  "technical",
  "assets",
];

function normalizeText(value) {
  return value
    .replace(/\r\n/g, "\n")
    .replace(/[ \t]+\n/g, "\n")
    .replace(/\n{3,}/g, "\n\n")
    .trim();
}

function paragraphText(value) {
  return normalizeText(value)
    .replace(/\n(?![-*]\s|\d+\.\s)/g, " ")
    .replace(/[ \t]{2,}/g, " ");
}

function repoPath(root, filePath) {
  return relative(root, filePath).split(sep).join("/");
}

function parseFrontmatter(markdown) {
  if (!markdown.startsWith("---\n")) {
    return { data: {}, body: markdown };
  }

  const end = markdown.indexOf("\n---", 4);
  if (end === -1) {
    return { data: {}, body: markdown };
  }

  const raw = markdown.slice(4, end).trim();
  const data = {};
  for (const line of raw.split(/\r?\n/)) {
    const match = line.match(/^([A-Za-z0-9_-]+):\s*(.*)$/);
    if (match) {
      data[match[1]] = match[2].trim().replace(/^"(.*)"$/, "$1");
    }
  }

  return { data, body: markdown.slice(end + 5).trimStart() };
}

function headingTitle(markdown) {
  const match = markdown.match(/^#\s+(.+)$/m);
  return match ? match[1].trim() : "";
}

function section(markdown, title) {
  const lines = markdown.replace(/\r\n/g, "\n").split("\n");
  const heading = title.toLowerCase();
  const start = lines.findIndex((line) => line.replace(/^##\s+/, "").trim().toLowerCase() === heading);
  if (start === -1 || !lines[start].startsWith("## ")) return "";

  const collected = [];
  for (let index = start + 1; index < lines.length; index += 1) {
    if (lines[index].startsWith("## ")) break;
    collected.push(lines[index]);
  }
  return paragraphText(collected.join("\n"));
}

function extractNumber(id) {
  const match = id.match(/_(\d+)$/);
  return match ? match[1] : "";
}

function extractPrefix(id) {
  const match = id.match(/^([A-Z]+)/);
  return match ? match[1] : "";
}

function readCheck(root, filePath) {
  const markdown = readFileSync(filePath, "utf8");
  const { data, body } = parseFrontmatter(markdown);
  const title = headingTitle(body);
  const id = data.id || title.split(/\s+/)[0] || "";
  const name = data.name || title.replace(id, "").trim();

  return {
    id,
    name,
    group: data.group || "",
    number: extractNumber(id),
    prefix: extractPrefix(id),
    description: data.description || "",
    whatItChecks: section(body, "What It Checks"),
    useWhen: section(body, "Use When"),
    evidence: section(body, "Evidence"),
    doNotUseFor: section(body, "Do Not Use For"),
    notEnough: section(body, "Not Enough"),
    path: repoPath(root, filePath),
    copyText: id,
  };
}

function parseCatalogRows(root, markdown) {
  const rows = [];
  for (const line of markdown.replace(/\r\n/g, "\n").split("\n")) {
    const match = line.match(/^\|\s*([^|]+?)\s*\|\s*\[([A-Z]+_\d+)\]\(([^)]+)\)\s*\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|$/);
    if (!match) continue;
    const check = readCheck(root, join(root, "ai_studio", "quality", match[3].trim()));
    if (check.id !== match[2]) {
      throw new Error(`Quality catalog id ${match[2]} does not match ${check.id} in ${match[3]}`);
    }
    rows.push({
      groupTitle: match[1].trim(),
      useWhen: match[4].trim(),
      doNotUseFor: match[5].trim(),
      check,
    });
  }
  return rows;
}

export function loadQualityCatalog(root) {
  const readmePath = join(root, "ai_studio", "quality", "README.md");
  const rows = existsSync(readmePath) ? parseCatalogRows(root, readFileSync(readmePath, "utf8")) : [];
  const discovered = [...new Set(rows.map((row) => row.check.group))];
  const ordered = [
    ...GROUP_ORDER.filter((slug) => discovered.includes(slug)),
    ...discovered.filter((slug) => !GROUP_ORDER.includes(slug)).sort(),
  ];
  const groups = ordered.map((slug) => {
    const groupRows = rows.filter((row) => row.check.group === slug);
    const checks = groupRows.map((row) => row.check);
    return {
      slug,
      title: `${groupRows[0].groupTitle} Rules`,
      description: groupRows.map((row) => row.useWhen).join("; "),
      prefix: checks[0]?.prefix || "",
      path: `${repoPath(root, readmePath)}#rule-catalog`,
      checks,
    };
  });

  return {
    groups,
    totalGroups: groups.length,
    totalChecks: groups.reduce((total, group) => total + group.checks.length, 0),
  };
}
