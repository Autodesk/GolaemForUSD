GolaemHydra
===========

In addition to the Golaem USD plugin, GolaemForUSD provides a second plugin,
GolaemHydra, which is a generative procedural plugin for Hydra.

Like Golaem USD, a GolaemHydra prim contains all the information needed to
render a Golaem cache. But whereas Golaem USD generates USD prims for the
renderer, a GolaemHydra prim is rendered directly by Hydra. That is, it
generates Hydra prims rather than USD prims.

GolaemHydra has been tested with the Storm and RenderMan XPU renderers.


Example
-------

A GolaemHydra prim in a USDA file looks something like this:

    def GenerativeProcedural "glorp" (
        prepend apiSchemas = ["HydraGenerativeProceduralAPI"]
    )
    {
        token primvars:hdGp:proceduralType = "GolaemHydra"
        token primvars:crowdFields = "crowdField1"
        token primvars:cacheName = "hydraTest"
        token primvars:cacheDir = "C:/Users/bill/work/cache/hydraTest"
        token primvars:characterFiles = "C:/Golaem/GolaemCharacterPack-9.2.2/golaem/characters/CasualMan_Light.gcha"
    }

Note that the `primvars:hdGp:proceduralType` attribute is required and must be
set to "GolaemHydra". For a complete list of the other attributes, see
[GolaemHydra Attributes](#golaemhydra-attributes) below.


Transformations and Extents
---------------------------

A GolaemHydra prim can contain xform and extent attributes. For example:

    def GenerativeProcedural "glorp" (
        prepend apiSchemas = ["HydraGenerativeProceduralAPI"]
    )
    {
        double3 xformOp:translate = (13, 3, -4)
        uniform token[] xformOpOrder = ["xformOp:translate"]
        float3[] extent = [(-5.5, -0.1, -4.0), (-1.0, 2.1, 3.5)]
        ...
    }

It is a good idea to provide the prim's extent, because the plugin cannot set it
by itself. The extent allows graphics applications to "frame" the prim, and it
allows renderers to cull the prim if it is not visible.

Note that you can parent a GolaemHydra prim to an xform, but Hydra ignores the
transformations of the parent xform and of all its parents.


Levels of Detail
----------------

GolaemHydra supports levels of detail (LOD) if three conditions are met:

1. The Golaem characters used define multiple LODs.
2. The `primvars:enableLod` attribute is set to true.
3. The rendered scene contains a primary camera.

The third condition means that LODs are ignored in usdview, for example, when
rendering with the "free" camera, not an actual camera.


Motion Blur
-----------

GolaemHydra activates motion blur if the `primvars:enableMotionBlur` attribute
is set to true. In that case, GolaemHydra searches for the motion blur shutter
interval in three places, and in the following order:

- the Hydra render settings;
- the primary camera's `shutter:open` and `shutter:close` attributes;
- the plugin's `primvars:defaultShutterOpen` and `primvars:defaultShutterClose`
  attributes.

That is, if GolaemHydra cannot find any render settings or a primary camera in
the Hydra scene index, it falls back on the default shutter interval. If the
resulting shutter interval is infinitely small (_i.e._ the open and close
attributes are equal), then motion blur is not activated.

So activating motion blur in GolaemHydra is quite simple, but it can be tricky
to get the renderer to follow. The render delegate should look in the render
settings and in the primary camera, but it may prefer one or the other, and it
may not look at either. In our experience with usdview and the RenderMan XPU
render delegate, the render settings are ignored; the camera's shutter settings
are also ignored unless you set them interactively, in usdview's Python
interpreter window.

Note that the Storm render delegate does not support motion blur.


Fur
---

If your Golaem cache contains fur elements, you can render them by setting the
GolaemHydra prim's `primvars:enableFur` attribute to true.

Two other attributes are useful for rendering fur. `primvars:furRenderPercent`
allows you to render only some of the fur, _e.g._ 1% or 10%. And
`primvars:furRefineLevel` allows you to render fur curves with more detail.

Note that, by default, the Storm render delegate ignores fur widths, rendering
curves as infinitely thin lines. To get Storm to render fur curves as tubes, you
must set `primvars:furRefineLevel` to 1 or more.


Instancing of Rigid Crowd Entities
----------------------------------

If a Golaem character contains a rigid mesh (no skinning or blend shapes),
GolaemHydra can theoretically reuse a single instance of that mesh to save
memory. For now, though, that feature is disabled.


GolaemHydra Attributes
----------------------

A complete list of Golaem attributes follows, with their default values, if any.

| Attribute                    | Type  | Default Value    |
| ---------------------------- | ----- | ---------------- |
| primvars:hdGp:proceduralType | token |                  |
| primvars:cacheDir            | token |                  |
| primvars:cacheName           | token |                  |
| primvars:characterFiles      | token |                  |
| primvars:crowdFields         | token |                  |
| primvars:layoutFiles         | token |                  |
| primvars:terrainFile         | token |                  |
| primvars:enableLayout        | bool  | true             |
| primvars:dirmap              | token |                  |
| primvars:displayMode         | token | "mesh"           |
| primvars:entityIds           | token | "*"              |
| primvars:renderPercent       | float | 100              |
| primvars:geometryTag         | int   | 0                |
| primvars:materialPath        | token | "Materials"      |
| primvars:materialAssignMode  | token | "byShadingGroup" |
| primvars:enableMotionBlur    | bool  | false            |
| primvars:defaultShutterOpen  | float | 0                |
| primvars:defaultShutterClose | float | 1                |
| primvars:enableLod           | bool  | false            |
| primvars:enableFur           | bool  | false            |
| primvars:furRenderPercent    | float | 100              |
| primvars:furRefineLevel      | int   | 0                |

**Notes**

- primvars:hdGp:proceduralType

    This *must* be set to "GolaemHydra".

- primvars:cacheDir

    Contains the path of the directory where the Golaem cache can be found. See
    also `primvars:dirmap`.

- primvars:cacheName

    Contains the name of the Golaem cache to be loaded.

- primvars:characterFiles

    Contains the full paths of one or more Golaem character files to be loaded,
    separated by semicolons. See also `primvars:dirmap`.

- primvars:crowdFields

    Contains one or more crowd field names, separated by semicolons.

- primvars:layoutFiles

    Contains the full paths of one or more Golaem layout files to be loaded,
    separated by semicolons. Layout files are ignored if `primvars:enableLayout`
    is false. See also `primvars:dirmap`.

- primvars:terrainFile

    Contains the full path of the Golaem terrain file to which the current
    simulation is to be adapted. This is useful only if you load a layout file
    that uses it. See also `primvars:dirmap`.

- primvars:enableLayout

    If true, and if one or more layout files is given, those layout files are
    applied to the Golaem cache. See also `primvars:layoutFiles`.

- primvars:dirmap

    Contains an even number of paths, separated by semicolons, used to adjust
    the paths of directories found in other attributes (`primvars:cacheDir`,
    `primvars:characterFiles`, etc.).

    The number of paths must be even because each pair of paths defines a dirmap
    "rule". If a directory given in another attribute begins with the first path
    in one of these rules, that part is replaced by the second path in the rule.

    For example:

        "C:/Users/bill;/mnt/c/Users/bill"

    might be used to map a Windows home directory to a Unix path.

- primvars:displayMode

    If "mesh" (the default), characters are displayed as polygonal meshes; if
    "bbox", only their bounding boxes are displayed.

- primvars:entityIds

    Can be used to choose a subset of character instances (entities) to be
    displayed, by specifying their IDs. For example, "1000-10000" displays only
    entities whose IDs are in the given range; "1001, 3001" displays only the
    two given entities. The default ("*") is to display all entities.

- primvars:renderPercent

    Specify a value greater than 0 but less than 100 to display only that
    percentage of the character instances.

- primvars:geometryTag

    If Golaem characters define multiple geometries, this can be used to choose
    the one to be displayed.

- primvars:materialPath

    Contains the USD path of the prim where characters' materials are defined.
    The path can be relative to the GolaemHydra prim (like the default,
    "Materials") or absolute (beginning with a slash).

- primvars:materialAssignMode

    Determines how materials are assigned to polygonal meshes and fur. If
    "byShadingGroup", GolaemHydra looks for a material with the same name as the
    mesh or fur's shading group. If "bySurfaceShader", GolaemHydra looks for a
    surface shader asset with the name of the shading group, then looks for the
    material whose name is given by that asset. If "none", materials are not
    assigned at all.

- primvars:enableMotionBlur

    If true, and if a valid camera shutter interval is defined, motion blur is
    activated. See [Motion Blur](#motion-blur) for details.

- primvars:defaultShutterOpen
- primvars:defaultShutterClose

    Together, these attributes define the default shutter interval used to
    compute motion blur, if that interval is not defined by the render settings
    or the primary camera. They are offsets relative to the current frame. See
    [Motion Blur](#motion-blur) for details.

- primvars:enableLod

    If true, and if a primary camera is known to the renderer, levels of detail
    defined by the character's geometry are used, depending on the distance to
    the camera. See [Levels of Detail](#levels-of-detail) for details.

- primvars:enableFur

    If true, and if `primvars:displayMode` is "mesh", any fur in the Golaem
    cache is rendered in addition to the polygonal meshes. See [Fur](#fur) for
    details.

- primvars:furRenderPercent

    If fur is displayed (see `primvars:enableFur`), specify a value greater than
    0 but less than 100 to display only that percentage of the fur curves.

- primvars:furRefineLevel

    Prim refine level to be applied to the display of fur curves, in the range
    0-8. See [Fur](#fur) for details.
