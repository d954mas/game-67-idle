import { join } from "node:path";

export const SKILL_ENTRYPOINT_MAX_CHARS = 2800;
export const REVIEW_SKILL_ENTRYPOINT_MAX_CHARS = 2400;
export const DEFAULT_HOT_DOC_MAX_CHARS = 4000;
export const REVIEW_DEFAULT_HOT_DOC_MAX_CHARS = 2800;

export const AGENTS_MAX_CHARS = 3400;
export const AI_PIPELINE_MAX_CHARS = 2200;
export const LIVE_STATUS_MAX_CHARS = 2400;
export const TASKS_README_MAX_CHARS = 3000;
export const TOOLS_README_MAX_CHARS = 3000;
export const PIPELINE_REFERENCE_MAX_CHARS = 2600;

export const AGENTS_REVIEW_MAX_CHARS = 2800;
export const AI_PIPELINE_REVIEW_MAX_CHARS = 1800;
export const TASKS_README_REVIEW_MAX_CHARS = 2400;
export const TOOLS_README_REVIEW_MAX_CHARS = 2400;
export const PIPELINE_REFERENCE_REVIEW_MAX_CHARS = 2200;

export const HOT_DOC_TOTAL_MAX_CHARS = 26000;
export const REVIEW_HOT_DOC_TOTAL_MAX_CHARS = 17800;
export const SKILL_ENTRYPOINT_TOTAL_MAX_CHARS = 36000;
export const REVIEW_SKILL_ENTRYPOINT_TOTAL_MAX_CHARS = 27000;

export const HOT_DOC_BUDGETS = [
  { path: "AGENTS.md", maxChars: AGENTS_MAX_CHARS, reviewMaxChars: AGENTS_REVIEW_MAX_CHARS },
  { path: "AI_PIPELINE.md", maxChars: AI_PIPELINE_MAX_CHARS, reviewMaxChars: AI_PIPELINE_REVIEW_MAX_CHARS },
  { path: join("docs", "ai-pipeline", "agent-workflow.md"), maxChars: PIPELINE_REFERENCE_MAX_CHARS, reviewMaxChars: PIPELINE_REFERENCE_REVIEW_MAX_CHARS },
  { path: join("docs", "ai-pipeline", "quality-validation.md"), maxChars: PIPELINE_REFERENCE_MAX_CHARS, reviewMaxChars: PIPELINE_REFERENCE_REVIEW_MAX_CHARS },
  { path: join("docs", "ai-pipeline", "profiling-reuse.md"), maxChars: PIPELINE_REFERENCE_MAX_CHARS, reviewMaxChars: PIPELINE_REFERENCE_REVIEW_MAX_CHARS },
  { path: join("tools", "README.md"), maxChars: TOOLS_README_MAX_CHARS, reviewMaxChars: TOOLS_README_REVIEW_MAX_CHARS },
  { path: join("tasks", "STATUS.md"), maxChars: LIVE_STATUS_MAX_CHARS, reviewMaxChars: LIVE_STATUS_MAX_CHARS },
  { path: join("tasks", "README.md"), maxChars: TASKS_README_MAX_CHARS, reviewMaxChars: TASKS_README_REVIEW_MAX_CHARS },
];
