import { catalogRelPath, listWorkspaceMounts, upsertWorkspaceMount, writeIdentityManifest } from "../../../../workspace/catalog.mjs";

export function listRegisteredGames(root) {
  return listWorkspaceMounts(root, { kinds: ["game"] }).map((mount) => ({
    id: mount.id,
    title: mount.title,
    folder: mount.root,
    assets: mount.assetRoot,
    status: "active",
  }));
}

export function registerGameAssetSource(root, { id, title = "", folder = "", assets = "" }) {
  const expectedFolder = `games/${id}`;
  if (folder && folder !== expectedFolder) throw new Error(`game folder must be ${expectedFolder}`);
  if (assets && assets !== `${expectedFolder}/assets`) throw new Error(`game assets must be ${expectedFolder}/assets`);
  writeIdentityManifest(root, "game", { id, title: title || id });
  const mount = upsertWorkspaceMount(root, {
    kind: "game",
    root: expectedFolder,
    visibility: "public",
    gitRoot: "",
    commitPolicy: "parent-public",
    enabledStores: ["assets"],
    aliases: [],
  });
  return { id: mount.id, title: mount.title, folder: mount.root, assets: mount.assetRoot, status: "active" };
}

export function gameRegistryPath() {
  return catalogRelPath(false);
}
