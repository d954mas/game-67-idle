#!/usr/bin/env python3
"""Generate / edit a real raster image with OpenAI gpt-image, headless.

Credential auto-detection (the credential decides the transport):
  1. $OPENAI_API_KEY = sk-...   -> official REST  POST /v1/images/generations  (billed; transparency OK)
  2. $OPENAI_API_KEY = eyJ...   -> ChatGPT/Codex OAuth JWT -> codex backend Responses API
  3. else read ~/.codex/auth.json -> tokens.access_token (+ account_id)         -> codex backend

The codex backend path reuses the SAME endpoint the official codex CLI calls
(https://chatgpt.com/backend-api/codex/responses) with an image_generation tool.
It is covered by a ChatGPT Plus/Pro subscription (no per-image billing) but is a
ToS gray zone — keep volume modest; for heavy/automated use set a real sk- key.

Transport is ALWAYS Windows curl.exe, because an HTTPS-scanning antivirus
(Avast Web Shield on this machine) MITMs TLS with a non-RFC CA that Python's
strict OpenSSL rejects (CERTIFICATE_VERIFY_FAILED) and SChannel only accepts with
--ssl-no-revoke. curl --ssl-no-revoke works behind the MITM and on a clean box.
See ../SKILL.md for the full story.
"""
import argparse, base64, hashlib, json, mimetypes, os, subprocess, sys, tempfile, time

CODEX_RESPONSES = "https://chatgpt.com/backend-api/codex/responses"
REST_IMAGES = "https://api.openai.com/v1/images/generations"
AUTH_JSON = os.path.expanduser("~/.codex/auth.json")
QUALITY_TIMEOUTS = {"low": 180, "medium": 300, "high": 480, "auto": 300}


def jwt_payload(tok):
    seg = tok.split(".")[1]; seg += "=" * (-len(seg) % 4)
    return json.loads(base64.urlsafe_b64decode(seg))


def resolve_credential():
    """-> (token, account_id_or_None, kind) ; kind in {'apikey','oauth'}."""
    env = os.environ.get("OPENAI_API_KEY", "").strip()
    if env.startswith("sk-"):
        return env, None, "apikey"
    if env.startswith("eyJ"):
        acct = (jwt_payload(env).get("https://api.openai.com/auth") or {}).get("chatgpt_account_id")
        return env, acct, "oauth"
    d = json.load(open(AUTH_JSON, encoding="utf-8"))
    t = d.get("tokens", {})
    tok = t.get("access_token", "")
    acct = t.get("account_id")
    if not acct and tok:
        try:
            acct = (jwt_payload(tok).get("https://api.openai.com/auth") or {}).get("chatgpt_account_id")
        except Exception:
            pass
    return tok, acct, "oauth"


def data_url(path):
    mime = mimetypes.guess_type(path)[0] or "image/png"
    return f"data:{mime};base64," + base64.b64encode(open(path, "rb").read()).decode()


def curl_post(url, headers, body_path, timeout):
    cmd = ["curl", "-sS", "-N", "--ssl-no-revoke", "--max-time", str(timeout),
           "-X", "POST", url, "--data-binary", "@" + body_path]
    for k, v in headers.items():
        cmd += ["-H", f"{k}: {v}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"curl exit {r.returncode}: {r.stderr.strip()[:400]}")
    return r.stdout


def post_json(url, headers, body, timeout):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False, encoding="utf-8") as f:
        json.dump(body, f); path = f.name
    try:
        return curl_post(url, headers, path, timeout)
    finally:
        os.unlink(path)


def parse_sse_images(raw):
    imgs, errs = [], []
    for line in raw.splitlines():
        line = line.strip()
        if not line.startswith("data:"):
            continue
        d = line[5:].strip()
        if not d or d == "[DONE]":
            continue
        try:
            ev = json.loads(d)
        except Exception:
            continue
        if ev.get("type") in ("response.failed", "error"):
            errs.append((ev.get("error") or {}).get("message") or ev.get("message") or "backend error")
        item = ev.get("item") or {}
        if item.get("type") == "image_generation_call" and item.get("result"):
            imgs.append(item["result"])
        for entry in ((ev.get("response") or {}).get("output") or []):
            if entry.get("type") == "image_generation_call" and entry.get("result"):
                imgs.append(entry["result"])
    return imgs, errs


def gen_codex(tok, acct, a, timeout):
    if not acct:
        raise SystemExit("ERROR: no ChatGPT-Account-Id (token/auth.json)")
    if a.background == "transparent":
        raise SystemExit("ERROR: the codex backend rejects transparent background on every model. "
                         "Generate on a flat key colour and chroma-key in post, or use an sk- API key (REST).")
    content = [{"type": "input_text", "text": a.prompt}]
    for img in a.input_image or []:
        content.append({"type": "input_image", "image_url": data_url(img), "detail": "auto"})
    tool = {"type": "image_generation", "model": a.model, "size": a.size, "output_format": a.format}
    if a.quality:
        tool["quality"] = a.quality
    body = {
        "model": a.responses_model,
        "input": [{"role": "user", "content": content}],
        "instructions": "You are an image generation assistant.",
        "tools": [tool],
        "tool_choice": {"type": "image_generation"},
        "stream": True,
        "store": False,
    }
    headers = {
        "Authorization": f"Bearer {tok}", "ChatGPT-Account-Id": acct,
        "Accept": "text/event-stream", "Content-Type": "application/json",
        "OpenAI-Beta": "responses=experimental", "originator": "codex_cli_rs",
    }
    raw = post_json(CODEX_RESPONSES, headers, body, timeout)
    imgs, errs = parse_sse_images(raw)
    if not imgs:
        raise SystemExit(f"ERROR: no image. backend errs={errs}\n--- head ---\n{raw.strip()[:600]}")
    return imgs


def gen_rest(tok, a, timeout):
    # sk- API key only; transparency works here. gpt-image-2 -> 1.5 for transparent.
    model = a.model
    fmt = a.format
    if a.background == "transparent":
        if model == "gpt-image-2":
            model = "gpt-image-1.5"
        if fmt == "jpeg":
            fmt = "png"
    body = {"model": model, "prompt": a.prompt, "size": a.size, "n": 1, "output_format": fmt}
    if a.quality:
        body["quality"] = a.quality
    if a.background:
        body["background"] = a.background
    raw = post_json(REST_IMAGES, {"Authorization": f"Bearer {tok}", "Content-Type": "application/json"},
                    body, timeout)
    try:
        data = json.loads(raw)
    except Exception:
        raise SystemExit(f"ERROR: non-JSON REST reply\n{raw[:600]}")
    if data.get("error"):
        raise SystemExit(f"REST error: {data['error'].get('message')}")
    imgs = [i["b64_json"] for i in data.get("data", []) if i.get("b64_json")]
    if not imgs:
        raise SystemExit(f"ERROR: no image in REST reply\n{raw[:600]}")
    return imgs


def gen_hash(a):
    """Stable key over everything that determines the output: prompt, size,
    quality, format, model, background, and the CONTENT of any input image (the
    source of an edit). A re-run with a matching sidecar skips the API call. For
    a dual-plate black plate the input is the white plate, so editing the white
    plate changes this hash and forces the black plate to regenerate."""
    h = hashlib.sha256()
    for part in (a.prompt, a.size, a.quality or "", a.format, a.model, a.background or ""):
        h.update(b"\x00" + str(part).encode("utf-8"))
    for img in a.input_image or []:
        try:
            with open(img, "rb") as f:
                h.update(f.read())
        except OSError:
            h.update(b"\x00MISSING:" + str(img).encode("utf-8"))
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description="Headless gpt-image generation (codex backend or sk- REST).")
    ap.add_argument("--prompt", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", default="1024x1024")
    ap.add_argument("--quality", default="high", choices=["low", "medium", "high", "auto"])
    ap.add_argument("--format", default="png", choices=["png", "jpeg", "webp"])
    ap.add_argument("--model", default="gpt-image-2")
    ap.add_argument("--background", choices=["transparent", "opaque", "auto"])
    ap.add_argument("--input-image", action="append", dest="input_image",
                    help="reference image path (repeatable, <=5) -> edit / style-match")
    ap.add_argument("--responses-model", default="gpt-5.5", help="outer model for codex backend path")
    ap.add_argument("--timeout", type=int, help="override seconds (default by --quality)")
    ap.add_argument("--force", action="store_true", help="regenerate even if an unchanged output already exists")
    a = ap.parse_args()

    sidecar = a.out + ".gen.json"
    key = gen_hash(a)
    if not a.force and os.path.exists(a.out) and os.path.exists(sidecar):
        try:
            prev = json.load(open(sidecar, encoding="utf-8")).get("hash")
        except Exception:
            prev = None
        if prev == key:
            print(f"SKIP {a.out} (unchanged; pass --force to regenerate)")
            return

    tok, acct, kind = resolve_credential()
    if not tok:
        raise SystemExit("ERROR: no credential ($OPENAI_API_KEY sk-/eyJ, or ~/.codex/auth.json)")
    timeout = a.timeout or QUALITY_TIMEOUTS.get(a.quality or "auto", 300)
    sys.stderr.write(f"[cred={kind}{' acct='+acct[:8]+'...' if acct else ''} transport=curl]\n")

    t0 = time.time()
    imgs = gen_rest(tok, a, timeout) if kind == "apikey" else gen_codex(tok, acct, a, timeout)
    data = base64.b64decode(imgs[0])
    with open(a.out, "wb") as f:
        f.write(data)
    try:
        with open(sidecar, "w", encoding="utf-8") as f:
            json.dump({"hash": key, "bytes": len(data), "model": a.model, "size": a.size,
                       "quality": a.quality, "ts": int(time.time())}, f)
    except OSError:
        pass
    print(f"OK wrote {a.out} ({len(data)} bytes) via {kind} in {time.time()-t0:.1f}s")


if __name__ == "__main__":
    main()
