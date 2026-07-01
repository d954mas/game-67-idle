# Texture Prep

Tools for checking standalone material textures before they are accepted as
tileable/reusable assets.

```powershell
py -3.12 ai_studio/assets/tools/textures/audit_tileable_texture.py --source <texture.png> --preview <texture_2x2.png> --json-output <audit.json> --report <audit.md>
```
