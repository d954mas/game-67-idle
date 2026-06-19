import { join } from "node:path";

export const SKILL_ENTRYPOINT_MAX_CHARS = 2400;
export const DEFAULT_HOT_DOC_MAX_CHARS = 2800;

export const AGENTS_MAX_CHARS = 2950;
export const AI_PIPELINE_MAX_CHARS = 1800;
export const LIVE_STATUS_MAX_CHARS = 2400;
export const TASKS_README_MAX_CHARS = 2400;
export const TOOLS_README_MAX_CHARS = 2400;
export const PIPELINE_REFERENCE_MAX_CHARS = 2200;

export const HOT_DOC_BUDGETS = [
  { path: "AGENTS.md", maxChars: AGENTS_MAX_CHARS },
  { path: "AI_PIPELINE.md", maxChars: AI_PIPELINE_MAX_CHARS },
  { path: join("docs", "ai-pipeline", "agent-workflow.md"), maxChars: PIPELINE_REFERENCE_MAX_CHARS },
  { path: join("docs", "ai-pipeline", "quality-validation.md"), maxChars: PIPELINE_REFERENCE_MAX_CHARS },
  { path: join("docs", "ai-pipeline", "profiling-reuse.md"), maxChars: PIPELINE_REFERENCE_MAX_CHARS },
  { path: join("tools", "README.md"), maxChars: TOOLS_README_MAX_CHARS },
  { path: join("tasks", "STATUS.md"), maxChars: LIVE_STATUS_MAX_CHARS },
  { path: join("tasks", "README.md"), maxChars: TASKS_README_MAX_CHARS },
];
