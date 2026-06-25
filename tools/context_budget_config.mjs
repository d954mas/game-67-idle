import { join } from "node:path";

export const SKILL_ENTRYPOINT_MAX_CHARS = 2800;
export const REVIEW_SKILL_ENTRYPOINT_MAX_CHARS = 3200;
export const DEFAULT_HOT_DOC_MAX_CHARS = 4000;
export const REVIEW_DEFAULT_HOT_DOC_MAX_CHARS = 3600;

export const AGENTS_MAX_CHARS = 3400;
export const LIVE_STATUS_MAX_CHARS = 2400;
export const TASKS_README_MAX_CHARS = 3000;
export const TOOLS_README_MAX_CHARS = 3000;
export const PIPELINE_REFERENCE_MAX_CHARS = 2600;

export const AGENTS_REVIEW_MAX_CHARS = 3600;
export const TASKS_README_REVIEW_MAX_CHARS = 3200;
export const TOOLS_README_REVIEW_MAX_CHARS = 3200;
export const PIPELINE_REFERENCE_REVIEW_MAX_CHARS = 3200;

export const HOT_DOC_TOTAL_MAX_CHARS = 26000;
export const REVIEW_HOT_DOC_TOTAL_MAX_CHARS = 24000;
export const SKILL_ENTRYPOINT_TOTAL_MAX_CHARS = 36000;
export const REVIEW_SKILL_ENTRYPOINT_TOTAL_MAX_CHARS = 38000;

export const HOT_DOC_BUDGETS = [
  { path: "AGENTS.md", maxChars: AGENTS_MAX_CHARS, reviewMaxChars: AGENTS_REVIEW_MAX_CHARS },
  { path: join("ai_studio", "README.md"), maxChars: PIPELINE_REFERENCE_MAX_CHARS, reviewMaxChars: PIPELINE_REFERENCE_REVIEW_MAX_CHARS },
  { path: join("ai_studio", "core_harness", "workflow", "README.md"), maxChars: PIPELINE_REFERENCE_MAX_CHARS, reviewMaxChars: PIPELINE_REFERENCE_REVIEW_MAX_CHARS },
  { path: join("ai_studio", "core_harness", "orchestration", "README.md"), maxChars: PIPELINE_REFERENCE_MAX_CHARS, reviewMaxChars: PIPELINE_REFERENCE_REVIEW_MAX_CHARS },
  { path: join("docs", "ai-pipeline", "quality-validation.md"), maxChars: PIPELINE_REFERENCE_MAX_CHARS, reviewMaxChars: PIPELINE_REFERENCE_REVIEW_MAX_CHARS },
  { path: join("docs", "ai-pipeline", "profiling-reuse.md"), maxChars: PIPELINE_REFERENCE_MAX_CHARS, reviewMaxChars: PIPELINE_REFERENCE_REVIEW_MAX_CHARS },
  { path: join("tools", "README.md"), maxChars: TOOLS_README_MAX_CHARS, reviewMaxChars: TOOLS_README_REVIEW_MAX_CHARS },
  { path: join("tasks", "STATUS.md"), maxChars: LIVE_STATUS_MAX_CHARS, reviewMaxChars: LIVE_STATUS_MAX_CHARS },
  { path: join("ai_studio", "taskboard", "README.md"), maxChars: TASKS_README_MAX_CHARS, reviewMaxChars: TASKS_README_REVIEW_MAX_CHARS },
];
