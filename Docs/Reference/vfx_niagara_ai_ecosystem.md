cd ~/unreal-mcp
mkdir -p Docs/Reference
mv ~/Downloads/vfx_niagara_ai_ecosystem.md Docs/Reference/vfx_niagara_ai_ecosystem.md
git add Docs/Reference/vfx_niagara_ai_ecosystem.md MCPGameProject/MCPGameProject.uproject
git commit -m "Docs: agregar referencia sobre ecosistema IA+VFX+Niagara; habilitar plugin OSC en uproject"
git push origin main
