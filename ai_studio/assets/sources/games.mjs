import { listWorkspaceMounts } from "../../workspace/catalog.mjs";

export function listRegisteredGames(root) {
  return listWorkspaceMounts(root, { kinds: ["game"] }).map((mount) => ({
    id: mount.id,
    title: mount.title,
    folder: mount.root,
    assets: mount.assetRoot,
    status: "active",
  }));
}
