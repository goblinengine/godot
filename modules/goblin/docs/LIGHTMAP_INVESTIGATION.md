# Godot Lightmap Injection & Baking - Thorough Technical Investigation

## Executive Summary

Baked lightmaps in Godot are injected into meshes through a shader flag-based system at the renderer level. The process is separated into baking (compute phase) and injection (shader binding phase), with **shadow casting being optional for baking** (only for light shadowing information), but **lightmaps themselves work independently**.

---

## 1. HOW LIGHTMAPS ARE INJECTED INTO MESHES

### 1.1 High-Level Injection Flow

**The injection happens at the RenderingServer level** after baking completes:

```
BakedLightmap Textures Created
    ↓
LightmapGI::_assign_lightmaps() called
    ↓
RS::instance_geometry_set_lightmap() invoked for each mesh
    ↓
Geometry Instance receives lightmap UV scale & texture slice info
    ↓
Shader reads lightmap via LIGHTMAP flag
    ↓
Fragment shader applies pre-baked lighting
```

### 1.2 Key Injection API

**File**: [scene/3d/lightmap_gi.cpp](scene/3d/lightmap_gi.cpp#L1534)

```cpp
void LightmapGI::_assign_lightmaps() {
    for (int i = 0; i < light_data->get_user_count(); i++) {
        NodePath user_path = light_data->get_user_path(i);
        Node *node = get_node_or_null(user_path);
        
        if (instance_idx >= 0) {
            RID instance_id = node->call("get_bake_mesh_instance", instance_idx);
            if (instance_id.is_valid()) {
                // KEY INJECTION POINT:
                RS::get_singleton()->instance_geometry_set_lightmap(
                    instance_id,                              // The mesh instance
                    get_instance(),                           // The lightmap GI resource  
                    light_data->get_user_lightmap_uv_scale(i),// UV scale in lightmap atlas
                    light_data->get_user_lightmap_slice_index(i)// Texture slice/layer
                );
            }
        }
    }
}
```

**File**: [servers/rendering/renderer_scene_cull.cpp](servers/rendering/renderer_scene_cull.cpp#L1513)

The actual implementation in the renderer:
```cpp
void RendererSceneCull::instance_geometry_set_lightmap(
    RID p_instance, 
    RID p_lightmap, 
    const Rect2 &p_lightmap_uv_scale, 
    int p_slice_index
) {
    // This associates the geometry instance with a lightmap RID
    // Stores the UV scale and texture slice information
    // The geometry_instance will now read from the lightmap texture
}
```

### 1.3 Data Associated with Each Mesh

Each baked mesh stores:
- **Lightmap UV coordinates** (UV2 channel) - unique per-mesh mapping into the atlas
- **UV scale rectangle** - the Rect2 bounds within the atlas texture
- **Texture slice index** - which layer of the texture array to read from
- **Lightmap SH data** (if using spherical harmonics for indirect light)

---

## 2. SHADER REQUIREMENTS FOR LIGHTMAP SUPPORT

### 2.1 Minimum Shader Requirements

**YES, you NEED shader support to receive lightmaps.** The shader must:

1. **Include UV2 in vertex format**
2. **Have the LIGHTMAP flag enabled** in the geometry instance
3. **Sample the lightmap texture** in the fragment shader

### 2.2 Shader Implementation Details

**File**: [servers/rendering/renderer_rd/shaders/forward_clustered/scene_forward_clustered_inc.glsl](servers/rendering/renderer_rd/shaders/forward_clustered/scene_forward_clustered_inc.glsl)

```glsl
// Instance flags for shader detection
#define INSTANCE_FLAGS_USE_LIGHTMAP_CAPTURE (1 << 7)
#define INSTANCE_FLAGS_USE_LIGHTMAP (1 << 8)
#define INSTANCE_FLAGS_USE_SH_LIGHTMAP (1 << 9)

// Lightmap sampling modes
#define LIGHTMAP_FLAG_USE_DIRECTION 1
#define LIGHTMAP_FLAG_USE_SPECULAR_DIRECTION 2

// Shadowmask modes
#define LIGHTMAP_SHADOWMASK_MODE_NONE 0
#define LIGHTMAP_SHADOWMASK_MODE_REPLACE 1
#define LIGHTMAP_SHADOWMASK_MODE_OVERLAY 2
#define LIGHTMAP_SHADOWMASK_MODE_ONLY 3

// The shader accesses lightmap textures like this:
layout(set = 1, binding = 7) uniform texture2DArray lightmap_textures[MAX_LIGHTMAP_TEXTURES * 2];

// Sampling code in forward_clustered.glsl (line ~1833):
if (instance_flags & INSTANCE_FLAGS_USE_LIGHTMAP) {
    uint ofs = gi_offset >> 16;
    vec3 uvw = vec3(uv_interp * lightmaps.data[ofs].uv_scale + lightmaps.data[ofs].uv_offset, float(slice_index));
    
    // Read L0 (base lightmap color)
    lm_light_l0 = textureArray_bicubic(lightmap_textures[ofs], uvw + vec3(0.0, 0.0, 0.0), 
                                       lightmaps.data[ofs].light_texture_size).rgb;
    
    // If using directional SH lightmaps, also read L1 coefficients
    if (lightmaps.data[ofs].flags & LIGHTMAP_FLAG_USE_DIRECTION) {
        lm_light_l1n1 = (textureArray_bicubic(..., uvw + vec3(0.0, 0.0, 1.0), ...).rgb - vec3(0.5)) * 2.0;
        lm_light_l1_0 = (textureArray_bicubic(..., uvw + vec3(0.0, 0.0, 2.0), ...).rgb - vec3(0.5)) * 2.0;
        lm_light_l1p1 = (textureArray_bicubic(..., uvw + vec3(0.0, 0.0, 3.0), ...).rgb - vec3(0.5)) * 2.0;
    }
}
```

### 2.3 What the Shader Does with Lightmaps

The forward rendering shader automatically:
1. **Checks `INSTANCE_FLAGS_USE_LIGHTMAP`** flag
2. **Computes UV coordinates** using `lightmap_uv_scale + lightmap_uv_offset`
3. **Samples L0 (base color)** from the first layer
4. **Optionally samples L1 coefficients** (directional information) if using SH lightmaps
5. **Applies bicubic filtering** for better quality
6. **Blends lightmap with dynamic lighting** based on shadowmask mode

### 2.4 Built-in Material Handling

**The default materials already support lightmaps.** In Godot 4.x:
- Standard 3D materials automatically sample lightmaps if the `INSTANCE_FLAGS_USE_LIGHTMAP` flag is set
- No custom shader code is needed for basic lightmap support
- Custom shaders need to manually check and apply lightmaps

---

## 3. SHADOW CASTING & LIGHTMAP BAKING

### 3.1 Do You Need Shadow Casting to Bake Lightmaps?

**SHORT ANSWER: NO** - but it's nuanced.

**Shadow Casting Mode affects:**
- ✅ **How shadows are BAKED into the lightmap** (if a mesh casts shadows)
- ❌ **Whether a mesh can RECEIVE a lightmap** (independent)

### 3.2 Light Baking Modes

**File**: [scene/3d/light_3d.h](scene/3d/light_3d.h#L64)

```cpp
enum BakeMode {
    BAKE_DISABLED,    // Light doesn't contribute to bakes
    BAKE_STATIC,      // Light bakes direct + bounced light (but no dynamic shadows)
    BAKE_DYNAMIC      // Light bakes static shadows only, dynamic shadows at runtime
};
```

### 3.3 Baking Process Requirements

**File**: [scene/3d/lightmap_gi.cpp](scene/3d/lightmap_gi.cpp#L1176)

```cpp
// Lights are added to the bake based on bake_mode
for (int i = 0; i < lights_found.size(); i++) {
    Light3D *light = lights_found[i].light;
    if (light->is_editor_only()) {
        continue;  // Skip editor-only lights
    }

    // Light's bake_mode is passed to the lightmapper
    lightmapper->add_directional_light(
        light->get_name(), 
        light->get_bake_mode() == Light3D::BAKE_STATIC,  // p_static parameter
        ...
    );
}
```

### 3.4 What Gets Baked vs. Runtime

**For BAKE_STATIC lights:**
- ✅ Direct lighting
- ✅ Bounced light (if bounces > 0)
- ✅ Shadows cast by static geometry
- ❌ No dynamic shadows at runtime

**For BAKE_DYNAMIC lights:**
- ✅ Baked directly into lightmap for static geometry
- ✅ Dynamic shadows rendered at runtime
- ✅ Hybrid approach (static bake + dynamic shadows)

**For BAKE_DISABLED lights:**
- ❌ No lightmap bake contribution
- ✅ Still renders as dynamic light at runtime

---

## 4. REMOVING/DISABLING LIGHTS AFTER BAKING

### 4.1 Can You Remove the Light After Baking?

**YES, completely safe** - with caveats based on bake mode.

**What happens:**

1. **If light is BAKE_STATIC** → Remove it, the baked result is **frozen in the lightmap**
   - Static shadows stay in the lightmap
   - Bounced light stays in the lightmap  
   - ✅ Safe to delete

2. **If light is BAKE_DYNAMIC** → Keep it or replace with dynamic light
   - Lightmap contains baked base, shadows rendered dynamically
   - Without the light, only the static baked part shows
   - ⚠️ May look incomplete

3. **If light is BAKE_DISABLED** → Irrelevant to lightmaps
   - Never affected baking
   - ✅ Can remove anytime

### 4.2 Practical Example

```cpp
// After lightmap baking is done:
Directional3D light = get_node("DirectionalLight3D");
light->set_bake_mode(Light3D::BAKE_STATIC);
// ... bake happens ...

// After baking completes:
light.queue_free();  // ✅ Safe! The baked lighting persists

// OR replace with cheaper version:
light.set_energy(0);  // Disable but keep for reference
```

---

## 5. STATIC SHADOWCASTING ONLY (Without Dynamic Lights)

### 5.1 How to Enable Static Shadowcasting Only

**This is a common setup for performance:**

```gdscript
# Light configuration for static-only shadows
extends Light3D

func _ready():
    # Set up for static baking
    bake_mode = Light3D.BAKE_STATIC
    
    # OR for more control:
    bake_mode = Light3D.BAKE_DYNAMIC
    
    # Then disable shadows at runtime:
    shadow = false  # No runtime shadows
    energy = 0.0    # Disable dynamic contribution
```

### 5.2 Settings Combinations

**Option A: Pure Static (Most Efficient)**
```
Light: BAKE_STATIC
Shadow: true (for baking only)
Energy: 1.0 (during bake, 0 after or before)
Runtime: Disabled or removed
```

**Option B: Static Bake + Disabled Shadow Rendering**
```
Light: BAKE_STATIC  
Shadow: false (skip shadow rendering pass)
Casts Shadows: yes (for bake purposes)
```

**Option C: Hybrid (Baked Static + Dynamic Shadows)**
```
Light: BAKE_DYNAMIC
Shadow: true (both baked + runtime)
Energy: 1.0 (always on)
Shadowmask Mode: REPLACE or OVERLAY
```

### 5.3 Shadowmask Modes

**File**: [scene/3d/lightmap_gi.h](scene/3d/lightmap_gi.h#L62)

```cpp
enum ShadowmaskMode {
    SHADOWMASK_MODE_NONE = 0,      // No shadowmask texture
    SHADOWMASK_MODE_REPLACE = 1,   // Shadowmask replaces shadows
    SHADOWMASK_MODE_OVERLAY = 2,   // Blend shadowmask over dynamic shadows
};
```

**SHADOWMASK_MODE_REPLACE:**
- Baked shadows completely replace dynamic shadows
- Best for fully static scenes
- Lowest overhead

**SHADOWMASK_MODE_OVERLAY:**
- Blends baked shadows with dynamic shadows
- Hybrid static+dynamic approach
- Higher quality but more expensive

---

## 6. COMPLETE LIGHTMAP INJECTION PIPELINE

### 6.1 Baking Phase

```
1. Find all meshes with GI_MODE_STATIC
2. Verify meshes have UV2 and normals
3. Render UV2-mapped textures (albedo, emission)
4. Create lightmapper instance
5. Add all mesh geometry to lightmapper
6. Add lights based on BAKE_STATIC/BAKE_DYNAMIC modes
7. Lightmapper performs raytracing + bounces
8. Generate light atlas texture(s)
9. Optional: Generate shadowmask texture
10. Save as .exr (HDR) for lightmap
11. Create LightmapGIData resource with texture refs
```

### 6.2 Injection Phase

```
1. Load LightmapGIData from resource
2. For each user mesh:
   a. Get mesh instance RID
   b. Call instance_geometry_set_lightmap()
   c. Pass: instance_id, lightmap_rid, uv_scale, slice_index
3. Geometry instance stores lightmap reference
4. Shader detects INSTANCE_FLAGS_USE_LIGHTMAP
5. On render: sample lightmap texture at UV2 coords
6. Blend with dynamic lighting per shadowmask mode
```

### 6.3 Data Flow Diagram

```
Mesh (with UV2)
    ↓
[Baking] Lightmapper renders to atlas
    ↓
Lightmap Texture Atlas (2D Array)
    ↓
LightmapGIData (stores texture + user mapping)
    ↓
[Injection] instance_geometry_set_lightmap()
    ↓
Geometry Instance (has lightmap reference)
    ↓
Forward Renderer
    ├→ Check INSTANCE_FLAGS_USE_LIGHTMAP
    ├→ Compute UV coordinates (UV2 * scale + offset)
    ├→ Sample lightmap_textures[ofs][uvw]
    ├→ Blend with dynamic lights (if any)
    └→ Output final shaded pixel
```

---

## 7. MESH REQUIREMENTS

### 7.1 What Makes a Mesh Bakeable

**File**: [scene/3d/lightmap_gi.cpp](scene/3d/lightmap_gi.cpp#L402)

```cpp
// Mesh validation for baking
if (mi && mi->get_gi_mode() == GeometryInstance3D::GI_MODE_STATIC 
    && mi->is_visible_in_tree()) {
    
    // Check each surface:
    for (int i = 0; i < mesh->get_surface_count(); i++) {
        // Must be triangles:
        if (mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
            continue;
        }
        
        // MUST have UV2:
        if (!(mesh->surface_get_format(i) & Mesh::ARRAY_FORMAT_TEX_UV2)) {
            all_have_uv2_and_normal = false;
            break;
        }
        
        // MUST have normals:
        if (!(mesh->surface_get_format(i) & Mesh::ARRAY_FORMAT_NORMAL)) {
            all_have_uv2_and_normal = false;
            break;
        }
        
        surfaces_found = true;
    }
}
```

**Mesh Requirements Checklist:**
- ✅ Primitive type: TRIANGLES (no quads/other)
- ✅ Has UV2 channel (lightmap coordinates)
- ✅ Has normal vectors (for light direction)
- ✅ GI mode: GI_MODE_STATIC (for baking)
- ✅ Visible in tree (at bake time)
- ✅ Lightmap size hint set (or default 64x64)

### 7.2 Setting Up a Mesh for Baking

```gdscript
# In an importer or scene setup:
extends MeshInstance3D

func _ready():
    # Set as static for lightmap baking
    gi_mode = GeometryInstance3D.GI_MODE_STATIC
    
    # The mesh must have UV2 (imported or generated)
    # Can be generated via:
    var mesh = get_mesh()
    mesh.lightmap_unwrap()  # Auto-unwrap if needed
    
    # Optionally set lightmap size hint
    mesh.set_lightmap_size_hint(Vector2i(128, 128))
```

---

## 8. ADVANCED: LIGHTMAP TEXTURE STRUCTURE

### 8.1 Texture Atlas Organization

Lightmaps are stored as **2D Array Textures** to fit multiple baked meshes efficiently:

```
Texture2DArray: lightmap_0.exr
├── Layer 0: [Mesh1 L0 (RGB color)]
├── Layer 1: [Mesh1 L1n-1 (SH coefficient)]
├── Layer 2: [Mesh1 L1_0 (SH coefficient)]
├── Layer 3: [Mesh1 L1p1 (SH coefficient)]
├── Layer 4: [Mesh2 L0 (RGB color)]
├── Layer 5: [Mesh2 L1n-1 (SH coefficient)]
└── ...

Texture2DArray: shadowmask_0.png (if enabled)
├── Layer 0: [Mesh1 shadow mask]
├── Layer 1: [Mesh2 shadow mask]
└── ...
```

### 8.2 Spherical Harmonics (SH) Lightmaps

If `directional=true` in bake settings:
- **L0**: Base diffuse color (stored in RGB)
- **L1 (3 layers)**: Directional coefficients
  - L1n-1 (Y)
  - L1_0 (Z)  
  - L1p1 (X)
- Used for normal-dependent lighting

If `directional=false`:
- Only L0 stored (no directional info)
- Cheaper memory/faster sampling

---

## 9. KEY FINDINGS - ANSWERED QUESTIONS

### Question 1: Is there anything I need in the shader to support lightmaps?

**Answer:** 
- ✅ **For built-in materials**: NO, automatically supported
- ✅ **For custom shaders**: Check `INSTANCE_FLAGS_USE_LIGHTMAP` and sample from `lightmap_textures`
- ✅ **UV2 is automatically handled** by the geometry instance

### Question 2: Do I need to enable shadow casting to bake lightmaps?

**Answer:**
- **NO** - Shadow casting is **orthogonal to lightmap existence**
- Shadow casting determines **how shadows appear in the lightmap**
- A mesh without shadow casting can still **receive** a baked lightmap
- A mesh with shadow casting will **cast shadows into other meshes' lightmaps**
- ✅ You can bake lightmaps with or without shadow casting enabled

### Question 3: Can I turn off or remove the light after shadow casting?

**Answer:**
- ✅ **YES, completely safe** if light is set to `BAKE_STATIC`
- ⚠️ **With caution** if light is `BAKE_DYNAMIC` (hybrid approach)
- ❌ **Not recommended** if light is `BAKE_DISABLED` (wasn't baked anyway)

### Question 4: Can I enable static shadowcasting only and disable dynamic lights?

**Answer:**
- ✅ **YES, this is a common optimization**
  ```gdscript
  light.bake_mode = Light3D.BAKE_STATIC  # Bake only
  light.shadow = true                    # Cast shadows while baking
  light.energy = 0.0                     # Disable at runtime (or queue_free)
  ```
- ✅ **Shadowmask mode: REPLACE** for best static-only look
- ✅ **No dynamic light overhead** after baking
- ✅ **Perfect for static scenes** (performance optimal)

---

## 10. REFERENCES

### Key Source Files

1. **Baking Logic**: [scene/3d/lightmap_gi.cpp](scene/3d/lightmap_gi.cpp#L1125)
2. **Injection Logic**: [scene/3d/lightmap_gi.cpp](scene/3d/lightmap_gi.cpp#L1534)
3. **Renderer Integration**: [servers/rendering/renderer_scene_cull.cpp](servers/rendering/renderer_scene_cull.cpp#L1513)
4. **Shader Sampling**: [servers/rendering/renderer_rd/shaders/forward_clustered/scene_forward_clustered.glsl](servers/rendering/renderer_rd/shaders/forward_clustered/scene_forward_clustered.glsl#L1833)
5. **Light Bake Modes**: [scene/3d/light_3d.h](scene/3d/light_3d.h#L64)
6. **Lightmapper RD**: [modules/lightmapper_rd/lightmapper_rd.cpp](modules/lightmapper_rd/lightmapper_rd.cpp)

---

## 11. MOBILE & COMPATIBILITY RENDERER SUPPORT

### 11.1 Mobile Renderer (Forward Mobile) Lightmap Support

**YES, fully supported** - Mobile renderer has identical lightmap handling to Forward Clustered.

**File**: [servers/rendering/renderer_rd/shaders/forward_mobile/scene_forward_mobile_inc.glsl](servers/rendering/renderer_rd/shaders/forward_mobile/scene_forward_mobile_inc.glsl#L247)

Mobile renderer lightmap flags:
```glsl
#define INSTANCE_FLAGS_USE_LIGHTMAP_CAPTURE (1 << 7)
#define INSTANCE_FLAGS_USE_LIGHTMAP (1 << 8)
#define INSTANCE_FLAGS_USE_SH_LIGHTMAP (1 << 9)

// Same structure as Forward Clustered:
layout(set = 0, binding = 7, std140) restrict readonly buffer Lightmaps {
    Lightmap data[];
} lightmaps;

layout(set = 1, binding = 6) uniform texture2DArray lightmap_textures[MAX_LIGHTMAP_TEXTURES * 2];
```

Mobile shader lightmap sampling ([scene_forward_mobile.glsl](servers/rendering/renderer_rd/shaders/forward_mobile/scene_forward_mobile.glsl#L1729)):
```glsl
// Same bicubic or linear sampling based on sc_use_lightmap_bicubic_filter()
if (instance_flags & INSTANCE_FLAGS_USE_LIGHTMAP) {
    uint ofs = gi_offset >> 16;
    vec3 uvw = vec3(uv_interp * lightmaps.data[ofs].uv_scale + lightmaps.data[ofs].uv_offset, float(slice_index));
    
    lm_light_l0 = hvec3(textureArray_bicubic(lightmap_textures[ofs], uvw + vec3(0.0, 0.0, 0.0), 
                                              lightmaps.data[ofs].light_texture_size).rgb);
    
    // SH coefficients if directional:
    if (lightmaps.data[ofs].flags & LIGHTMAP_FLAG_USE_DIRECTION) {
        lm_light_l1n1 = hvec3(...);
        lm_light_l1_0 = hvec3(...);
        lm_light_l1p1 = hvec3(...);
    }
}
```

### 11.2 Compatibility Renderer (GLES3)

**NO native lightmap support** - The Compatibility renderer (GLES3 backend) does **NOT** support baked lightmaps in the same way.

**Why**: 
- Compatibility renderer uses different shader architecture (no texture2DArray support in older GLES3)
- No dynamic GPU buffer for instance data like Forward Mobile/Clustered
- Built for maximum compatibility with older hardware

**Workaround**: If using Compatibility renderer:
1. ❌ Can't use LightmapGI baking
2. ✅ Alternative: Use **Lightprobe data** (spherical harmonics stored per position)
3. ✅ Alternative: Fallback to **real-time lighting** with lower quality shadows
4. ✅ Alternative: Manually apply baked colors as vertex colors or material colors

---

## 12. HOW TO IMPLEMENT LIGHTMAPGIDATA IN YOUR SCENE

### 12.1 Basic Setup (Using the Editor)

**Step 1: Set up your scene**
```gdscript
extends Node3D

# Create a LightmapGI node in your scene
# -> Scene -> New Node -> LightmapGI
# Place it at the root or near your geometry
```

**Step 2: Configure meshes for baking**
```gdscript
# In each MeshInstance3D that should receive lightmaps:

extends MeshInstance3D

func _ready():
    # Required: Set GI mode to static
    gi_mode = GeometryInstance3D.GI_MODE_STATIC
    
    # Mesh must have UV2 (lightmap coordinates)
    # If not auto-imported with UV2:
    var mesh = get_mesh()
    if mesh:
        mesh.lightmap_unwrap()  # Generate UV2
    
    # Optional: Set lightmap size
    mesh.set_lightmap_size_hint(Vector2i(128, 128))
```

**Step 3: Configure lights for baking**
```gdscript
# In each Light3D that should contribute to lightmap:

extends Light3D

func _ready():
    # Set bake mode (IMPORTANT!)
    bake_mode = Light3D.BAKE_STATIC  # or BAKE_DYNAMIC for shadows
    
    # Light will be included in bake
```

**Step 4: Bake the lightmap**
```gdscript
# In the LightmapGI node:
extends LightmapGI

func _ready():
    # Set quality and options
    bake_quality = LightmapGI.BAKE_QUALITY_MEDIUM
    use_denoiser = true
    directional = true  # Use directional SH lightmaps
    
    # Optional: Create/load lightmap data path
    # light_data = preload("res://path/to/lightmap_data.tres")
    
    # Bake (call from code or editor)
    # var error = bake(get_parent(), "res://lightmaps/scene_lightmap.tres")
    # if error != OK:
    #     print("Bake failed: ", error)
```

### 12.2 Programmatic Scene Setup (Non-Editor)

**Complete example: Dynamic lightmap assignment**

```gdscript
extends Node3D

var lightmap_data: LightmapGIData

func _ready():
    # Load pre-baked lightmap data
    lightmap_data = load("res://lightmaps/scene_lightmap.tres")
    
    if lightmap_data.is_null():
        print("ERROR: No lightmap data found!")
        return
    
    # Create LightmapGI node programmatically
    var lightmap_gi = LightmapGI.new()
    lightmap_gi.set_light_data(lightmap_data)
    add_child(lightmap_gi)
    
    # The lightmap will automatically inject to all meshes
    # referenced in lightmap_data.get_user_path(i)
    
    print("Lightmap loaded and injected!")
```

### 12.3 The Injection Process (What Happens Behind the Scenes)

When `LightmapGI._assign_lightmaps()` is called:

```cpp
// From lightmap_gi.cpp line 1534
void LightmapGI::_assign_lightmaps() {
    for (int i = 0; i < light_data->get_user_count(); i++) {
        // 1. Get the node path from baked data
        NodePath user_path = light_data->get_user_path(i);
        Node *node = get_node_or_null(user_path);
        
        if (!node) continue;
        
        // 2. Get mesh instance RID
        int instance_idx = light_data->get_user_sub_instance(i);
        RID instance_id = node->call("get_bake_mesh_instance", instance_idx);
        
        // 3. INJECT THE LIGHTMAP
        RS::get_singleton()->instance_geometry_set_lightmap(
            instance_id,                                    // Mesh RID
            get_instance(),                                 // LightmapGI RID
            light_data->get_user_lightmap_uv_scale(i),     // Rect2 UV bounds
            light_data->get_user_lightmap_slice_index(i)   // Texture layer
        );
    }
}
```

**What this does:**
1. Associates the mesh instance with the lightmap texture
2. Passes UV scale (where in the atlas to sample)
3. Passes texture slice (which layer in the 2D array)
4. Shader automatically detects and samples the lightmap

### 12.4 Step-by-Step: Getting a Baked Lightmap Into Your Scene

#### **Phase 1: Preparation**
```
1. Create scene with meshes (MeshInstance3D)
   ↓
2. Set each mesh: gi_mode = GI_MODE_STATIC
   ↓
3. Ensure meshes have UV2 (run mesh.lightmap_unwrap())
   ↓
4. Set mesh lightmap size hint (optional)
   ↓
5. Create lights with bake_mode = BAKE_STATIC or BAKE_DYNAMIC
```

#### **Phase 2: Baking**
```
6. Create LightmapGI node
   ↓
7. Configure bake settings (quality, directional, denoiser, etc)
   ↓
8. Call bake() with output path
   ↓
9. Baker finds all GI_MODE_STATIC meshes
   ↓
10. Baker adds lights based on bake_mode
   ↓
11. Baker raytraces and creates lightmap atlas
   ↓
12. Saves LightmapGIData resource (.tres file)
```

#### **Phase 3: Injection (Automatic)**
```
13. LightmapGI enters the tree (NOTIFICATION_POST_ENTER_TREE)
    ↓
14. LightmapGI::_assign_lightmaps() called automatically
    ↓
15. For each user mesh in lightmap_data:
    ├→ Find the mesh node
    ├→ Get mesh instance RID
    └→ Call instance_geometry_set_lightmap()
    ↓
16. Renderer stores: mesh → lightmap texture + UV scale + slice
    ↓
17. Next frame: Shader checks INSTANCE_FLAGS_USE_LIGHTMAP
    ↓
18. Shader samples lightmap_textures[ofs][uv2_coords]
    ↓
19. Fragment color = lightmap_color * material_color
```

### 12.5 Data Structure: LightmapGIData

**What it contains:**

```cpp
class LightmapGIData : public Resource {
    // Textures
    TypedArray<TextureLayered> storage_light_textures;      // RGB lightmap layers
    TypedArray<TextureLayered> storage_shadowmask_textures;  // Shadow info
    
    // User mesh mappings
    struct User {
        NodePath path;              // Scene path to mesh
        int32_t sub_instance;       // Index if using multi-mesh
        Rect2 uv_scale;             // UV bounds in atlas
        int slice_index;            // Which texture array layer
    };
    Vector<User> users;             // All baked meshes
    
    // Light probe data (for indirect lighting in unlit areas)
    PackedVector3Array probe_points;
    PackedColorArray probe_sh;       // Spherical harmonics
    PackedInt32Array probe_tetrahedra;
    PackedInt32Array probe_bsp_tree; // For fast spatial queries
    
    // Metadata
    bool uses_spherical_harmonics;   // Directional vs flat
    int shadowmask_mode;             // How to blend shadows
    float baked_exposure;            // EV normalization
};
```

### 12.6 Manually Assigning Lightmaps (Advanced)

If you need custom injection logic:

```gdscript
extends Node3D

var lightmap_gi_data: LightmapGIData
var mesh_instance: MeshInstance3D

func _ready():
    # Load lightmap data
    lightmap_gi_data = load("res://lightmaps/scene_lightmap.tres")
    mesh_instance = $MeshInstance3D
    
    # Create a "fake" LightmapGI node as a container
    var lightmap_container = LightmapGI.new()
    lightmap_container.set_light_data(lightmap_gi_data)
    add_child(lightmap_container)
    
    # The container will auto-inject lightmaps to any mesh
    # that was included in the baked data
    
    # If you need manual control, you can call directly:
    # (though this is rarely needed)
    # var rs = RenderingServer.get_singleton()
    # rs.instance_geometry_set_lightmap(
    #     mesh_instance.get_instance(),
    #     lightmap_gi_data.get_rid(),
    #     lightmap_gi_data.get_user_lightmap_uv_scale(0),
    #     lightmap_gi_data.get_user_lightmap_slice_index(0)
    # )
```

---

## 13. RENDERER COMPATIBILITY SUMMARY

| Feature | Forward Clustered | Forward Mobile | Compatibility |
|---------|---|---|---|
| Lightmap Baking | ✅ Full | ✅ Full | ❌ Not supported |
| Lightmap Injection | ✅ Via instance_geometry_set_lightmap() | ✅ Via instance_geometry_set_lightmap() | ❌ Not supported |
| Shader Support | ✅ Automatic in materials | ✅ Automatic in materials | ❌ No lightmap textures |
| Shadowmask | ✅ Yes | ✅ Yes | ❌ Not supported |
| SH Directional | ✅ Yes | ✅ Yes | ❌ Not supported |
| Bicubic Filtering | ✅ Yes | ✅ Yes | ❌ Not applicable |
| GPU Buffer Update | ✅ Real-time | ✅ Real-time | ❌ Not applicable |

**Recommendation**: Use **Forward Mobile** for mobile devices - it has full lightmap support with optimizations for lower-end hardware.

---

## 14. RUNTIME LIGHTMAP INJECTION - CRITICAL INFORMATION

### 14.1 CAN YOU APPLY LIGHTMAPS AT RUNTIME?

**YES, technically fully supported** - Godot's renderer can inject lightmaps at runtime without limitations.

**BUT there's a critical requirement**: The lightmap textures must be **already loaded and referenced** by the renderer.

### 14.2 The Runtime Injection Flow

When you call `RS::instance_geometry_set_lightmap()` at runtime:

```cpp
// This is what happens internally:
void instance_geometry_set_lightmap(RID p_instance, RID p_lightmap, const Rect2 &p_lightmap_uv_scale, int p_slice_index) {
    Instance *instance = instance_owner.get_or_null(p_instance);
    Instance *lightmap_instance = instance_owner.get_or_null(p_lightmap);  // <-- KEY REQUIREMENT
    
    instance->lightmap = lightmap_instance;                               // Store reference
    instance->lightmap_uv_scale = p_lightmap_uv_scale;
    instance->lightmap_slice_index = p_slice_index;
    
    if (lightmap_instance) {
        // Get the lightmap RID from the renderer
        RID lightmap_instance_rid = lightmap_data->instance;
    }
    
    // CRITICAL: Call the geometry instance with the RID
    geom->geometry_instance->set_use_lightmap(
        lightmap_instance_rid,              // This must be valid!
        p_lightmap_uv_scale, 
        p_slice_index
    );
}
```

### 14.3 WHY YOUR LIGHTMAPS AREN'T SHOWING (COMMON ISSUES)

#### **Issue 1: Missing LightmapGI RID**

```gdscript
# ❌ WRONG - You're passing the LightmapGIData resource RID directly
var lightmap_data = load("res://lightmaps/lightmap_data.tres")
RS.instance_geometry_set_lightmap(
    mesh_instance.get_instance(),
    lightmap_data.get_rid(),  # <-- WRONG! This is not a valid lightmap instance RID
    lightmap_uv_scale,
    slice_index
)
```

**The issue**: `LightmapGIData.get_rid()` returns the **LightmapGIData resource RID**, not a **LightmapGI geometry instance RID**.

The renderer needs an **actual LightmapGI node instance** with:
- A valid `RS::INSTANCE_LIGHTMAP` base type
- Proper `InstanceLightmapData` attached
- Lightmap textures already loaded

#### **Issue 2: Lightmap Node Not in Tree**

```gdscript
# ❌ WRONG
var lightmap_gi_data = load("res://lightmaps/lightmap_data.tres")
var lightmap_gi = LightmapGI.new()
lightmap_gi.set_light_data(lightmap_gi_data)
# Not added to tree! Shader never gets the lightmap textures
```

**The fix:**
```gdscript
# ✅ CORRECT
var lightmap_gi_data = load("res://lightmaps/lightmap_data.tres")
var lightmap_gi = LightmapGI.new()
lightmap_gi.set_light_data(lightmap_gi_data)
add_child(lightmap_gi)  # <-- CRITICAL: Must be in tree!

# Now the textures are loaded and available
yield(get_tree(), "process_frame")
# Now you can safely use the lightmap
```

#### **Issue 3: Textures Not Loaded**

The LightmapGIData must have textures set:

```gdscript
# ❌ WRONG - Empty lightmap data
var lightmap_data = LightmapGIData.new()
# No textures!

# ✅ CORRECT - Lightmap data with textures
var lightmap_data = load("res://lightmaps/lightmap_data.tres")
# Verify textures are loaded:
if lightmap_data.get_lightmap_textures().is_empty():
    print("ERROR: No lightmap textures loaded!")
    return
```

#### **Issue 4: Meshes Don't Have UV2**

```gdscript
# ❌ MESHES WITHOUT UV2 CAN'T RECEIVE LIGHTMAPS
var mesh = MeshInstance3D.new()
mesh.mesh = preload("res://meshes/model.obj")
# UV2 not generated!

# ✅ CORRECT - Generate UV2
mesh.mesh = preload("res://meshes/model.obj")
mesh.mesh.lightmap_unwrap()  # Generate UV2
```

### 14.4 THE CORRECT RUNTIME LIGHTMAP WORKFLOW

```gdscript
extends Node3D

var lightmap_data: LightmapGIData
var lightmap_gi: LightmapGI

func _ready():
    # Step 1: Create the LightmapGI container
    lightmap_gi = LightmapGI.new()
    add_child(lightmap_gi)
    
    # Step 2: Load the pre-baked lightmap data
    lightmap_data = load("res://lightmaps/scene_lightmap.tres")
    
    # Step 3: Assign it to the LightmapGI node
    lightmap_gi.set_light_data(lightmap_data)
    
    # Step 4: Wait for the scene to process
    await get_tree().process_frame
    
    # Step 5: LightmapGI will AUTO-INJECT lightmaps to all meshes
    # that match the node paths in lightmap_data.users
    print("Lightmaps injected!")
```

### 14.5 AUTOMATIC INJECTION (RECOMMENDED)

The **preferred way** is to let `LightmapGI` handle everything:

```gdscript
# This is what LightmapGI::_assign_lightmaps() does for you:
extends LightmapGI

func _assign_lightmaps():
    for i in range(light_data.get_user_count()):
        # Get the node path from baked data
        var user_path = light_data.get_user_path(i)
        var node = get_node_or_null(user_path)
        
        if !node:
            continue
        
        # Get the mesh instance RID
        var instance_id = node.get_instance()
        
        # INJECT the lightmap
        RenderingServer.instance_geometry_set_lightmap(
            instance_id,
            self.get_instance(),  # <-- KEY: Pass the LightmapGI instance RID
            light_data.get_user_lightmap_uv_scale(i),
            light_data.get_user_lightmap_slice_index(i)
        )
```

### 14.6 COMMON MISTAKE: CUSTOM BAKERS

If you **created a custom lightmap baker**, make sure:

```cpp
// Your custom baker must:
// 1. Create the LightmapGIData properly
Ref<LightmapGIData> gi_data = memnew(LightmapGIData);

// 2. Set the lightmap TEXTURES (not just data)
gi_data->set_lightmap_textures(texture_array);

// 3. Register USERS with correct node paths
for (int i = 0; i < baked_meshes.size(); i++) {
    MeshInstance3D *mesh = baked_meshes[i];
    NodePath path = lightmap_gi->get_path_to(mesh);
    Rect2 uv_scale = computed_uv_scales[i];
    int slice = i / slices_per_texture;
    
    gi_data->add_user(path, uv_scale, slice, -1);  // <-- ALL 4 parameters matter
}

// 4. Assign it to the LightmapGI
lightmap_gi->set_light_data(gi_data);
```

### 14.7 DEBUGGING CHECKLIST

If lightmaps aren't showing after calling `set_light_data()`:

```gdscript
var lightmap_data = load("res://lightmaps/lightmap_data.tres")
var lightmap_gi = LightmapGI.new()
add_child(lightmap_gi)
lightmap_gi.set_light_data(lightmap_data)

# Debug: Check what's in the lightmap data
print("User count: ", lightmap_data.get_user_count())
print("Textures: ", lightmap_data.get_lightmap_textures().size())
print("Uses SH: ", lightmap_data.is_using_spherical_harmonics())

# Check each user
for i in range(lightmap_data.get_user_count()):
    print("User %d: %s" % [i, lightmap_data.get_user_path(i)])
    print("  UV Scale: %s" % [lightmap_data.get_user_lightmap_uv_scale(i)])
    print("  Slice: %d" % [lightmap_data.get_user_lightmap_slice_index(i)])

# Verify meshes have UV2
for mesh_inst in get_tree().get_nodes_in_group("meshes"):
    var mesh = mesh_inst.get_mesh()
    for surface_idx in range(mesh.get_surface_count()):
        var fmt = mesh.surface_get_format(surface_idx)
        if not (fmt & Mesh.ARRAY_FORMAT_TEX_UV2):
            print("ERROR: Mesh %s surface %d missing UV2!" % [mesh_inst.name, surface_idx])
```

### 14.8 RENDERER SUPPORT FOR RUNTIME LIGHTMAPPING

| Aspect | Forward Clustered | Forward Mobile | Compatibility |
|--------|---|---|---|
| **Runtime Injection** | ✅ Full support | ✅ Full support | ❌ Not supported |
| **Texture Loading** | ✅ Works at runtime | ✅ Works at runtime | ❌ No lightmap system |
| **Shader Updates** | ✅ Immediate | ✅ Immediate | ❌ N/A |
| **GPU Buffer Updates** | ✅ Automatic | ✅ Automatic | ❌ N/A |
| **Performance** | ✅ Optimal | ✅ Optimized for mobile | ❌ N/A |

---

## 15. SUMMARY

Godot's lightmap system is a **two-phase process**:

1. **Baking Phase**: Lightmapper creates texture atlases with pre-computed lighting
2. **Injection Phase**: Renderer binds textures to geometry and shaders sample them

**Shadow casting is orthogonal** - it controls what gets baked, not whether baking happens.
**Shaders automatically handle lightmaps** in built-in materials via instance flags.
**You can safely remove lights after baking** when using `BAKE_STATIC` mode.
**Static-only shadow rendering is optimal** for performance in mostly-static scenes.
**Mobile renderer fully supports lightmaps** - same as Forward Clustered.
**Compatibility renderer does NOT support lightmaps** - use Forward Mobile instead.
**Runtime lightmap injection is fully supported** - no renderer limitations, only data/setup requirements.
