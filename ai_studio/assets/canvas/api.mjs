// Canvas HTTP API adapter.
//
// Studio Shell mounts this handler on the /api/canvas/ prefix. It only marshals
// HTTP <-> the shared ops layer (ops.mjs); it holds no canvas logic. Routes:
//   GET    /api/canvas/projects
//   POST   /api/canvas/projects                    {title}
//   GET    /api/canvas/projects/<id>
//   POST   /api/canvas/projects/<id>/images        {name, bytes_base64}
//   POST   /api/canvas/projects/<id>/detect-regions {elementId, params?}
//   POST   /api/canvas/projects/<id>/slice          {elementId, regionIds?}
//   POST   /api/canvas/projects/<id>/export         {elementIds}
//   POST   /api/canvas/projects/<id>/groups         {name, x?,y?,w?,h?, fromElements?}
//   PATCH  /api/canvas/projects/<id>/groups/<gid>   {name?,x?,y?,w?,h?,visible?}
//   DELETE /api/canvas/projects/<id>/groups/<gid>
//   POST   /api/canvas/projects/<id>/groups/<gid>/render {scale?, background?}
//   POST   /api/canvas/projects/<id>/assign-group   {elementIds, groupId|null}
//   POST   /api/canvas/projects/<id>/undo
//   POST   /api/canvas/projects/<id>/redo
//   GET    /api/canvas/projects/<id>/history
//   PATCH  /api/canvas/projects/<id>/elements/<eid> {x,y,w,h,name,visible}
//   DELETE /api/canvas/projects/<id>/elements/<eid>
//   GET    /api/canvas/projects/<id>/files/<name>  (image bytes, path-confined)
import { existsSync, readFileSync, statSync } from "node:fs";
import { extname } from "node:path";
import {
  addImage,
  assignToGroup,
  createGroup,
  createProject,
  deleteGroup,
  detectRegions,
  exportElements,
  getProject,
  listProjects,
  patchElement,
  patchGroup,
  readHistory,
  redoOp,
  removeElement,
  renderGroup,
  resolveProjectFile,
  sliceRegions,
  undoOp,
} from "./ops.mjs";

// Images are the big payload here; allow up to ~20MB for a base64 upload body.
const maxBodyBytes = 20 * 1024 * 1024;

const mimeByExt = {
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".jpeg": "image/jpeg",
  ".gif": "image/gif",
  ".webp": "image/webp",
};

function sendJson(res, status, data) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(data));
}

function readJsonBody(req) {
  return new Promise((resolveBody, rejectBody) => {
    let size = 0;
    let data = "";
    req.setEncoding("utf8");
    req.on("data", (chunk) => {
      size += Buffer.byteLength(chunk);
      if (size > maxBodyBytes) {
        rejectBody(new Error("request body too large"));
        req.destroy();
        return;
      }
      data += chunk;
    });
    req.on("end", () => {
      try {
        resolveBody(data ? JSON.parse(data) : {});
      } catch {
        rejectBody(new Error("invalid JSON body"));
      }
    });
    req.on("error", rejectBody);
  });
}

function serveFile(res, filePath) {
  if (!existsSync(filePath) || !statSync(filePath).isFile()) {
    sendJson(res, 404, { error: "file not found" });
    return;
  }
  // Local single-user tool: read the confined project image into memory and end.
  // Files here are small canvas assets, so streaming is unnecessary complexity.
  res.writeHead(200, { "content-type": mimeByExt[extname(filePath).toLowerCase()] || "application/octet-stream" });
  res.end(readFileSync(filePath));
}

export function createCanvasApi(root) {
  return async function handleCanvasApi(req, res, url) {
    const parts = url.pathname.split("/").filter(Boolean); // ["api","canvas","projects", id, ...]
    try {
      if (parts[0] !== "api" || parts[1] !== "canvas" || parts[2] !== "projects") {
        sendJson(res, 404, { error: "not found" });
        return true;
      }

      // /api/canvas/projects
      if (parts.length === 3) {
        if (req.method === "GET") {
          sendJson(res, 200, { projects: listProjects(root) });
          return true;
        }
        if (req.method === "POST") {
          const body = await readJsonBody(req);
          sendJson(res, 201, { project: createProject(root, { title: body.title }) });
          return true;
        }
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }

      const id = decodeURIComponent(parts[3]);

      // /api/canvas/projects/<id>
      if (parts.length === 4) {
        if (req.method === "GET") {
          sendJson(res, 200, { project: getProject(root, id) });
          return true;
        }
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }

      const sub = parts[4];

      // /api/canvas/projects/<id>/images
      if (parts.length === 5 && sub === "images" && req.method === "POST") {
        const body = await readJsonBody(req);
        const bytes = Buffer.from(String(body.bytes_base64 || ""), "base64");
        sendJson(res, 201, addImage(root, id, { name: body.name, bytes }));
        return true;
      }

      // /api/canvas/projects/<id>/detect-regions
      if (parts.length === 5 && sub === "detect-regions" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendJson(res, 200, await detectRegions(root, {
          projectId: id,
          elementId: body.elementId,
          params: body.params || {},
        }));
        return true;
      }

      // /api/canvas/projects/<id>/slice
      if (parts.length === 5 && sub === "slice" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendJson(res, 200, await sliceRegions(root, {
          projectId: id,
          elementId: body.elementId,
          regionIds: body.regionIds,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/export
      if (parts.length === 5 && sub === "export" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendJson(res, 200, exportElements(root, {
          projectId: id,
          elementIds: Array.isArray(body.elementIds) ? body.elementIds : [],
          format: body.format,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/groups  (create)
      if (parts.length === 5 && sub === "groups" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendJson(res, 201, createGroup(root, {
          projectId: id,
          name: body.name,
          x: body.x,
          y: body.y,
          w: body.w,
          h: body.h,
          fromElements: body.fromElements,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/assign-group
      if (parts.length === 5 && sub === "assign-group" && req.method === "POST") {
        const body = await readJsonBody(req);
        sendJson(res, 200, assignToGroup(root, {
          projectId: id,
          elementIds: Array.isArray(body.elementIds) ? body.elementIds : [],
          groupId: body.groupId,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/groups/<gid>  (patch/delete)
      if (parts.length === 6 && sub === "groups") {
        const groupId = decodeURIComponent(parts[5]);
        if (req.method === "PATCH") {
          const body = await readJsonBody(req);
          sendJson(res, 200, patchGroup(root, { projectId: id, groupId, ...body }));
          return true;
        }
        if (req.method === "DELETE") {
          sendJson(res, 200, deleteGroup(root, { projectId: id, groupId }));
          return true;
        }
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }

      // /api/canvas/projects/<id>/groups/<gid>/render
      if (parts.length === 7 && sub === "groups" && parts[6] === "render" && req.method === "POST") {
        const groupId = decodeURIComponent(parts[5]);
        const body = await readJsonBody(req);
        sendJson(res, 200, await renderGroup(root, {
          projectId: id,
          groupId,
          scale: body.scale,
          background: body.background,
        }));
        return true;
      }

      // /api/canvas/projects/<id>/undo | /redo
      if (parts.length === 5 && sub === "undo" && req.method === "POST") {
        await readJsonBody(req);
        sendJson(res, 200, undoOp(root, { projectId: id }));
        return true;
      }
      if (parts.length === 5 && sub === "redo" && req.method === "POST") {
        await readJsonBody(req);
        sendJson(res, 200, redoOp(root, { projectId: id }));
        return true;
      }

      // /api/canvas/projects/<id>/history
      if (parts.length === 5 && sub === "history" && req.method === "GET") {
        sendJson(res, 200, readHistory(root, { projectId: id }));
        return true;
      }

      // /api/canvas/projects/<id>/files/<name>
      if (parts.length === 6 && sub === "files" && req.method === "GET") {
        const filePath = resolveProjectFile(root, id, decodeURIComponent(parts[5]));
        serveFile(res, filePath);
        return true;
      }

      // /api/canvas/projects/<id>/elements/<eid>
      if (parts.length === 6 && sub === "elements") {
        const elementId = decodeURIComponent(parts[5]);
        if (req.method === "PATCH") {
          const body = await readJsonBody(req);
          sendJson(res, 200, patchElement(root, id, elementId, body));
          return true;
        }
        if (req.method === "DELETE") {
          sendJson(res, 200, removeElement(root, id, elementId));
          return true;
        }
        sendJson(res, 405, { error: "method not allowed" });
        return true;
      }

      sendJson(res, 404, { error: "not found" });
      return true;
    } catch (error) {
      sendJson(res, 400, { error: error && error.message ? error.message : String(error) });
      return true;
    }
  };
}
