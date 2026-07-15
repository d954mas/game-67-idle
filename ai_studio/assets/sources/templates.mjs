import { listWorkspaceMounts, writeIdentityManifest } from "../../workspace/catalog.mjs";

export function listRegisteredTemplates(root) {
  return listWorkspaceMounts(root, { kinds: ["template"] }).map((mount) => ({
    id: mount.id,
    title: mount.title,
    folder: mount.root,
    assets: mount.assetRoot,
    status: "active",
  }));
}

export function registerTemplateAssetSource(root, { id, title = "", folder = "", assets = "" }) {
  const expectedFolder = `templates/${id}`;
  if (folder && folder !== expectedFolder) throw new Error(`template folder must be ${expectedFolder}`);
  if (assets && assets !== `${expectedFolder}/assets`) throw new Error(`template assets must be ${expectedFolder}/assets`);
  writeIdentityManifest(root, "template", { id, title: title || id });
  const mount = listWorkspaceMounts(root, { kinds: ["template"] }).find((entry) => entry.id === id);
  if (!mount) throw new Error(`template scanner did not discover ${expectedFolder}`);
  return { id: mount.id, title: mount.title, folder: mount.root, assets: mount.assetRoot, status: "active" };
}

export function templateRegistryPath() {
  return "templates/";
}
