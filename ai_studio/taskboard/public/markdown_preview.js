(function initTaskboardMarkdown(global) {
  function escapeHtml(value) {
    return String(value).replace(/[&<>"]/g, (char) => ({
      "&": "&amp;",
      "<": "&lt;",
      ">": "&gt;",
      '"': "&quot;",
    }[char]));
  }

  function renderInlineMarkdown(text) {
    return escapeHtml(text)
      .replace(/`([^`]+)`/g, "<code>$1</code>")
      .replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>");
  }

  function renderMarkdown(text) {
    const lines = String(text || "").replace(/\r\n/g, "\n").split("\n");
    const out = [];
    let inList = false;
    let inCode = false;
    let code = [];

    function closeList() {
      if (inList) {
        out.push("</ul>");
        inList = false;
      }
    }

    function openList() {
      if (!inList) {
        out.push("<ul>");
        inList = true;
      }
    }

    for (const line of lines) {
      if (line.startsWith("```")) {
        if (inCode) {
          out.push(`<pre><code>${escapeHtml(code.join("\n"))}</code></pre>`);
          code = [];
          inCode = false;
        } else {
          closeList();
          inCode = true;
        }
        continue;
      }
      if (inCode) {
        code.push(line);
        continue;
      }

      const heading = line.match(/^(#{1,3})\s+(.+)$/);
      if (heading) {
        closeList();
        const level = heading[1].length;
        out.push(`<h${level}>${renderInlineMarkdown(heading[2])}</h${level}>`);
        continue;
      }

      const check = line.match(/^- \[([ xX])\]\s+(.+)$/);
      if (check) {
        openList();
        const checked = check[1].toLowerCase() === "x" ? " checked" : "";
        out.push(`<li class="task-check"><input type="checkbox" disabled${checked}> <span>${renderInlineMarkdown(check[2])}</span></li>`);
        continue;
      }

      const bullet = line.match(/^[-*]\s+(.+)$/);
      if (bullet) {
        openList();
        out.push(`<li>${renderInlineMarkdown(bullet[1])}</li>`);
        continue;
      }

      if (!line.trim()) {
        closeList();
        continue;
      }

      closeList();
      out.push(`<p>${renderInlineMarkdown(line)}</p>`);
    }

    closeList();
    if (inCode) {
      out.push(`<pre><code>${escapeHtml(code.join("\n"))}</code></pre>`);
    }
    return out.join("");
  }

  global.TaskboardMarkdown = { renderMarkdown };
})(globalThis);
