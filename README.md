# **Requirements**
- Visual Studio 2022
- Windows 10 SDK 10.0.20348.0
- Vulkan SDK 1.3.236.0

# **Rendering APIs**
- D3D12
- Vulkan (not fully implemented, can only be enabled through -vulkan command argument)

# **Build**
### **Windows:**
- Clone with `git clone --recursive https://github.com/danibma/EdenEngine.git`
- Run `GenerateProjects_vs2022.bat` to generate the Visual Studio 2022 solution, after that, open the solution and just build the project in one of the configurations

# **Configurations**
- **Debug:** Symbols and asserts enabled, tracks memory, worst in performance
- **Profiling:** Symbols disabled, asserts enabled, tracks memory, best performance
- **Release:** Symbols and asserts disabled, doesn't track memory, best performance

# **Features**
A showcase of some of these features can be found in the section below
## **Core**
- GLTF model loading
- ECS
- Custom version of the shared pointer class
- Scene Serialization
- Memory tracking
## **Graphics**
- Blinn-Phong Lighting
- Cubemaps through HDR file format
- HDR Pipeline with Gamma Correction and Tonemapping
- Mip chain generation
- Shader reflection
- Shader hot-reloading through the editor
## **Editor**
- Scene serialization through the editor
- Content browser with viewport interaction

# **Features Showcase**
## **Graphics**
### Blinn-Phong Lighting
![Phong Lighting Image](img/phong_lighting.png)

### Cubemaps through HDR file format
![Cubemap](img/cubemap.png)

### HDR Pipeline with Gamma Correction and Tonemapping
| On | Off |
|----|-----|
| ![Multiple Lights](img/hdr_on.png) | ![Multiple Lights](img/hdr_off.png) |

### Mip chain generation
| On | Off |
|----|-----|
| ![Multiple Lights](img/mips_on.png) | ![Multiple Lights](img/mips_off.png) |


## **Editor**
### Scene serialization through the editor
![Scene Open](img/scene_serialization.gif)

### Content browser with viewport interaction
![Content Browser](img/content_browser.gif)
