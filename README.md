# **Requirements**
Windows 10 or newer
Windows 10 SDK 10.0.20348.0

# **Build**
### **Windows:**
Clone with `git clone --recursive https://github.com/danibma/EdenEngine.git` <br>
Run `GenerateProjects_vs2022.bat` to generate the Visual Studio 2022 solution, after that, open the solution and just build the project in one of the configurations
<br>

# **Configurations**
**Debug:** Symbols and asserts enabled, tracks memory, worst in performance <br>
**Profiling:** Symbols disabled, asserts enabled, tracks memory, best performance <br>
**Release:** Symbols and asserts disabled, doesn't track memory, best performance <br>

# **Features**
## **Graphics**
### Blinn-Phong Lighting
![Phong Lighting Image](img/phong_lighting.png)

### Cubemaps through HDR file format
![Cubemap](img/cubemap.png)

### Multiple Lights support
![Multiple Lights](img/multiple_lights.png)

### HDR Pipeline with Gamma Correction
| On | Off |
|----|-----|
| ![Multiple Lights](img/hdr_on.png) | ![Multiple Lights](img/hdr_off.png) |

### Mip chain generation
| On | Off |
|----|-----|
| ![Multiple Lights](img/mips_on.png) | ![Multiple Lights](img/mips_off.png) |


## **Editor**
### Scene Serialization
![Scene Open](img/scene_serialization.gif)

### Editor UI working with ECS and using D3D12 Render Passes for Viewport
![ECS UI](img/editor_ui.png)

### Content browser with viewport interaction
![Content Browser](img/content_browser.gif)
