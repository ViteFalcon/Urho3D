//
// Urho3D Engine
// Copyright (c) 2008-2012 Lasse ��rni
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "Component.h"

class Image;
class Material;
class Node;
class TerrainPatch;

class Terrain : public Component
{
    OBJECT(Terrain);
    
public:
    /// Construct.
    Terrain(Context* context);
    /// Destruct.
    ~Terrain();
    /// Register object factory.
    static void RegisterObject(Context* context);
    
    /// Set patch quads per side. Must be a power of two.
    void SetPatchSize(unsigned size);
    /// Set vertex (XZ) and height (Y) spacing.
    void SetSpacing(const Vector3& spacing);
    /// Set heightmap image. Dimensions should be a power of two + 1. Return true if successful.
    bool SetHeightMap(Image* image);
    /// Set material.
    void SetMaterial(Material* material);
    /// %Set draw distance for patches.
    void SetDrawDistance(float distance);
    /// %Set shadow draw distance for patches.
    void SetShadowDistance(float distance);
    /// %Set LOD bias for patches. Affects which terrain LOD to display.
    void SetLodBias(float bias);
    /// %Set view mask for patches. Is and'ed with camera's view mask to see if the object should be rendered.
    void SetViewMask(unsigned mask);
    /// %Set light mask for patches. Is and'ed with light's and zone's light mask to see if the object should be lit.
    void SetLightMask(unsigned mask);
    /// %Set shadow mask for patches. Is and'ed with light's light mask and zone's shadow mask to see if the object should be rendered to a shadow map.
    void SetShadowMask(unsigned mask);
    /// %Set zone mask for patches. Is and'ed with zone's zone mask to see if the object should belong to the zone.
    void SetZoneMask(unsigned mask);
    /// %Set maximum number of per-pixel lights for patches. Default 0 is unlimited.
    void SetMaxLights(unsigned num);
    /// %Set visible flag for patches.
    void SetVisible(bool enable);
    /// %Set shadowcaster flag for patches.
    void SetCastShadows(bool enable);
    /// %Set occlusion flag for patches.
    void SetOccluder(bool enable);
    /// %Set occludee flag for patches.
    void SetOccludee(bool enable);
    
    /// Return patch quads per side.
    unsigned GetPatchSize() const { return patchSize_; }
    /// Return vertex and height spacing.
    const Vector3& GetSpacing() const { return spacing_; }
    /// Return heightmap size.
    const IntVector2& GetSize() const { return size_; }
    /// Return heightmap image.
    Image* GetHeightMap() const;
    /// Return material.
    Material* GetMaterial() const;
    /// Return height at world coordinates.
    float GetHeight(const Vector3& worldPosition) const;
    /// Return raw height data.
    SharedArrayPtr<float> GetHeightData() const { return heightData_; }
    /// Return number of terrain patches.
    unsigned GetNumPatches() const { return patches_.Size(); }
    /// Return terrain patch.
    TerrainPatch* GetPatch(unsigned index) const;
    /// Return scene node of terrain patch.
    Node* GetPatchNode(unsigned index);
    /// Return draw distance.
    float GetDrawDistance() const { return drawDistance_; }
    /// Return shadow draw distance.
    float GetShadowDistance() const { return shadowDistance_; }
    /// Return LOD bias.
    float GetLodBias() const { return lodBias_; }
    /// Return view mask.
    unsigned GetViewMask() const { return viewMask_; }
    /// Return light mask.
    unsigned GetLightMask() const { return lightMask_; }
    /// Return shadow mask.
    unsigned GetShadowMask() const { return shadowMask_; }
    /// Return zone mask.
    unsigned GetZoneMask() const { return zoneMask_; }
    /// Return maximum number of per-pixel lights.
    unsigned GetMaxLights() const { return maxLights_; }
    /// Return visible flag.
    bool IsVisible() const { return visible_; }
    /// Return shadowcaster flag.
    bool GetCastShadows() const { return castShadows_; }
    /// Return occluder flag.
    bool IsOccluder() const { return occluder_; }
    /// Return occludee flag.
    bool IsOccludee() const { return occludee_; }
    
    /// Regenerate patch geometry.
    void UpdatePatchGeometry(TerrainPatch* patch);
    /// Update patch based on LOD and neighbor LOD.
    void UpdatePatchLOD(TerrainPatch* patch, unsigned lod, unsigned northLod, unsigned southLod, unsigned westLod, unsigned eastLod);
    
protected:
    /// Handle node transform being dirtied.
    virtual void OnMarkedDirty(Node* node);
    
private:
    /// Fully regenerate terrain geometry.
    void CreateGeometry();
    /// Set the scene node transform for a patch.
    void SetPatchTransform(TerrainPatch* patch);
    /// Return an uninterpolated terrain height value, clamping to edges.
    float GetRawHeight(unsigned x, unsigned z) const;
    /// Get terrain normal at position.
    Vector3 GetNormal(unsigned x, unsigned z) const;
    /// Handle heightmap image reload finished.
    void HandleHeightMapReloadFinished(StringHash eventType, VariantMap& eventData);
    
    /// Shared index buffer.
    SharedPtr<IndexBuffer> indexBuffer_;
    /// Heightmap image.
    SharedPtr<Image> heightMap_;
    /// Height data.
    SharedArrayPtr<float> heightData_;
    /// Material.
    SharedPtr<Material> material_;
    /// Terrain patches.
    Vector<SharedPtr<TerrainPatch> > patches_;
    /// Terrain patch scene nodes.
    Vector<SharedPtr<Node> > patchNodes_;
    /// Patch size, quads per side.
    unsigned patchSize_;
    /// Number of terrain LOD levles.
    unsigned numLodLevels_;
    /// Vertex and height spacing.
    Vector3 spacing_;
    /// Heightmap size.
    IntVector2 size_;
    /// Origin of patches on the XZ-plane.
    Vector2 patchWorldOrigin_;
    /// Size of a patch on the XZ-plane.
    Vector2 patchWorldSize_;
    /// Number of patches on the X-axis.
    unsigned patchesX_;
    /// Number of patches on the Z-axis.
    unsigned patchesZ_;
    /// Visible flag.
    bool visible_;
    /// Shadowcaster flag.
    bool castShadows_;
    /// Occluder flag.
    bool occluder_;
    /// Occludee flag.
    bool occludee_;
    /// View mask.
    unsigned viewMask_;
    /// Light mask.
    unsigned lightMask_;
    /// Shadow mask.
    unsigned shadowMask_;
    /// Zone mask.
    unsigned zoneMask_;
    /// Draw distance.
    float drawDistance_;
    /// Shadow distance.
    float shadowDistance_;
    /// LOD bias.
    float lodBias_;
    /// Maximum lights.
    unsigned maxLights_;
    /// Terrain needs regeneration flag.
    bool terrainDirty_;
};