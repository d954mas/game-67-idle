# Source Sheet Prep

Tools for checking generated or sourced sheets before runtime crops are planned.

Use these when a source image contains multiple isolated UI, icon, sprite, or
decor components on a chroma/transparent background.

```powershell
py -3.12 ai_studio/assets/prep/source_sheets/normalize_chroma.py --source <raw-sheet> --output <clean-sheet>
py -3.12 ai_studio/assets/prep/source_sheets/audit_intake.py --source <source-sheet> --json-output <audit.json> --report <audit.md>
```

