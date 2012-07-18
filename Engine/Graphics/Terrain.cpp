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

#include "Precompiled.h"
#include "Context.h"
#include "DrawableEvents.h"
#include "Geometry.h"
#include "Image.h"
#include "IndexBuffer.h"
#include "Log.h"
#include "Material.h"
#include "Node.h"
#include "Octree.h"
#include "Profiler.h"
#include "ResourceEvents.h"
#include "Scene.h"
#include "Terrain.h"
#include "TerrainPatch.h"
#include "VertexBuffer.h"

#include "DebugNew.h"

OBJECTTYPESTATIC(Terrain);

static const unsigned DEFAULT_PATCH_SIZE = 16;
static const unsigned DEFAULT_LOD_LEVELS = 3;
static const unsigned MAX_LOD_LEVELS = 4;
static const unsigned MIN_PATCH_SIZE = 4;
static const unsigned MAX_PATCH_SIZE = 128;
static const Vector3 DEFAULT_SPACING(1.0f, 0.25f, 1.0f);

Terrain::Terrain(Context* context) :
    Component(context),
    indexBuffer_(new IndexBuffer(context)),
    patchSize_(DEFAULT_PATCH_SIZE),
    spacing_(DEFAULT_SPACING),
    size_(IntVector2::ZERO),
    patchWorldSize_(Vector2::ZERO),
    patchWorldOrigin_(Vector2::ZERO),
    patchesX_(0),
    patchesZ_(0),
    numLodLevels_(DEFAULT_LOD_LEVELS),
    visible_(true),
    castShadows_(false),
    occluder_(false),
    occludee_(true),
    viewMask_(DEFAULT_VIEWMASK),
    lightMask_(DEFAULT_LIGHTMASK),
    shadowMask_(DEFAULT_SHADOWMASK),
    zoneMask_(DEFAULT_ZONEMASK),
    drawDistance_(0.0f),
    shadowDistance_(0.0f),
    lodBias_(1.0f),
    maxLights_(0),
    terrainDirty_(false)
{
    indexBuffer_->SetShadowed(true);
}

Terrain::~Terrain()
{
}

void Terrain::RegisterObject(Context* context)
{
    context->RegisterFactory<Terrain>();
}

void Terrain::SetSpacing(const Vector3& spacing)
{
    if (spacing != spacing_)
    {
        spacing_ = spacing;
        
        CreateGeometry();
        MarkNetworkUpdate();
    }
}

void Terrain::SetPatchSize(unsigned size)
{
    size = Clamp((int)size, (int)MIN_PATCH_SIZE, (int)MAX_PATCH_SIZE);
    
    if (size != patchSize_ && IsPowerOfTwo(size))
    {
        patchSize_ = size;
        
        unsigned lodSize = size;
        numLodLevels_ = 1;
        while (lodSize > MIN_PATCH_SIZE && numLodLevels_ < MAX_LOD_LEVELS)
        {
            lodSize >>= 1;
            ++numLodLevels_;
        }
        
        CreateGeometry();
        MarkNetworkUpdate();
    }
}

bool Terrain::SetHeightMap(Image* image)
{
    if (!image)
        return false;
    
    if (image->IsCompressed())
    {
        LOGERROR("Can not use a compressed image as a terrain heightmap");
        return false;
    }
    
    // Unsubscribe from the reload event of previous image (if any), then subscribe to the new
    if (heightMap_)
        UnsubscribeFromEvent(heightMap_, E_RELOADFINISHED);
    if (image)
        SubscribeToEvent(image, E_RELOADFINISHED, HANDLER(Terrain, HandleHeightMapReloadFinished));
    
    heightMap_ = image;
    
    CreateGeometry();
    MarkNetworkUpdate();
    return true;
}

void Terrain::SetMaterial(Material* material)
{
    material_ = material;
    for (unsigned i = 0; i < patches_.Size(); ++i)
        patches_[i]->batches_[0].material_ = material;
    
    MarkNetworkUpdate();
}

void Terrain::SetDrawDistance(float distance)
{
    drawDistance_ = distance;
    for (unsigned i = 0; i < patches_.Size(); ++i)
        patches_[i]->SetDrawDistance(distance);
    
    MarkNetworkUpdate();
}

void Terrain::SetShadowDistance(float distance)
{
    shadowDistance_ = distance;
    for (unsigned i = 0; i < patches_.Size(); ++i)
        patches_[i]->SetShadowDistance(distance);
    
    MarkNetworkUpdate();
}

void Terrain::SetLodBias(float bias)
{
    lodBias_ = bias;
    for (unsigned i = 0; i < patches_.Size(); ++i)
        patches_[i]->SetLodBias(bias);
    
    MarkNetworkUpdate();
}

void Terrain::SetViewMask(unsigned mask)
{
    viewMask_ = mask;
    for (unsigned i = 0; i < patches_.Size(); ++i)
        patches_[i]->SetViewMask(mask);
    
    MarkNetworkUpdate();
}

void Terrain::SetLightMask(unsigned mask)
{
    lightMask_ = mask;
    for (unsigned i = 0; i < patches_.Size(); ++i)
        patches_[i]->SetLightMask(mask);
    
    MarkNetworkUpdate();
}

void Terrain::SetShadowMask(unsigned mask)
{
    shadowMask_ = mask;
    for (unsigned i = 0; i < patches_.Size(); ++i)
        patches_[i]->SetShadowMask(mask);
    
    MarkNetworkUpdate();
}

void Terrain::SetZoneMask(unsigned mask)
{
    zoneMask_ = mask;
    for (unsigned i = 0; i < patches_.Size(); ++i)
        patches_[i]->SetZoneMask(mask);
    
    MarkNetworkUpdate();
}

void Terrain::SetMaxLights(unsigned num)
{
    maxLights_ = num;
    for (unsigned i = 0; i < patches_.Size(); ++i)
        patches_[i]->SetMaxLights(num);
    
    MarkNetworkUpdate();
}

void Terrain::SetVisible(bool enable)
{
    visible_ = enable;
    for (unsigned i = 0; i < patches_.Size(); ++i)
        patches_[i]->SetVisible(enable);
    
    MarkNetworkUpdate();
}

void Terrain::SetCastShadows(bool enable)
{
    castShadows_ = enable;
    for (unsigned i = 0; i < patches_.Size(); ++i)
        patches_[i]->SetCastShadows(enable);
    
    MarkNetworkUpdate();
}

void Terrain::SetOccluder(bool enable)
{
    occluder_ = enable;
    for (unsigned i = 0; i < patches_.Size(); ++i)
        patches_[i]->SetOccluder(enable);
    
    MarkNetworkUpdate();
}

void Terrain::SetOccludee(bool enable)
{
    occluder_ = enable;
    for (unsigned i = 0; i < patches_.Size(); ++i)
        patches_[i]->SetOccludee(enable);
    
    MarkNetworkUpdate();
}

Image* Terrain::GetHeightMap() const
{
    return heightMap_;
}

Material* Terrain::GetMaterial() const
{
    return material_;
}

float Terrain::GetHeight(const Vector3& worldPosition) const
{
    if (node_)
    {
        Vector3 position = node_->GetWorldTransform().Inverse() * worldPosition;
        float xPos = (position.x_ - patchWorldOrigin_.x_) / spacing_.x_;
        float zPos = (position.z_ - patchWorldOrigin_.y_) / spacing_.z_;
        float xFrac = xPos - floorf(xPos);
        float zFrac = zPos - floorf(zPos);
        float h1, h2, h3;
        
        if (xFrac + zFrac >= 1.0f)
        {
            h1 = GetRawHeight((unsigned)xPos + 1, (unsigned)zPos + 1);
            h2 = GetRawHeight((unsigned)xPos, (unsigned)zPos + 1);
            h3 = GetRawHeight((unsigned)xPos + 1, (unsigned)zPos);
            xFrac = 1.0f - xFrac;
            zFrac = 1.0f - zFrac;
        }
        else
        {
            h1 = GetRawHeight((unsigned)xPos, (unsigned)zPos);
            h2 = GetRawHeight((unsigned)xPos + 1, (unsigned)zPos);
            h3 = GetRawHeight((unsigned)xPos, (unsigned)zPos + 1);
        }
        
        float h = h1 * (1.0f - xFrac - zFrac) + h2 * xFrac + h3 * zFrac;
        /// \todo This assumes that the terrain scene node is upright
        return node_->GetWorldScale().y_ * h + node_->GetWorldPosition().y_;
    }
    else
        return 0.0f;
}

void Terrain::UpdatePatchGeometry(TerrainPatch* patch)
{
    BoundingBox box;
    unsigned vertexDataRow = patchSize_ + 1;
    VertexBuffer* vertexBuffer = patch->vertexBuffer_;
    if (vertexBuffer->GetVertexCount() != vertexDataRow * vertexDataRow)
        vertexBuffer->SetSize(vertexDataRow * vertexDataRow, MASK_POSITION | MASK_NORMAL | MASK_TEXCOORD1 | MASK_TANGENT);
    patch->cpuVertexData_ = new Vector3[vertexDataRow * vertexDataRow];
    
    float* vertexData = (float*)vertexBuffer->Lock(0, vertexBuffer->GetVertexCount());
    float* cpuVertexData = (float*)patch->cpuVertexData_.Get();
    
    if (vertexData)
    {
        unsigned x = patch->x_;
        unsigned z = patch->z_;
        
        for (unsigned z1 = 0; z1 <= patchSize_; ++z1)
        {
            for (unsigned x1 = 0; x1 <= patchSize_; ++x1)
            {
                int xPos = x * patchSize_ + x1;
                int zPos = z * patchSize_ + z1;
                
                // Position
                Vector3 position((float)x1 * spacing_.x_, GetRawHeight(xPos, zPos), (float)z1 * spacing_.z_);
                *vertexData++ = position.x_;
                *vertexData++ = position.y_;
                *vertexData++ = position.z_;
                *cpuVertexData++ = position.x_;
                *cpuVertexData++ = position.y_;
                *cpuVertexData++ = position.z_;
                
                box.Merge(position);
                
                // Normal
                Vector3 normal = GetNormal(xPos, zPos);
                *vertexData++ = normal.x_;
                *vertexData++ = normal.y_;
                *vertexData++ = normal.z_;
                
                // Texture coordinate
                Vector2 texCoord((float)xPos / (float)size_.x_, 1.0f - (float)zPos / (float)size_.y_);
                *vertexData++ = texCoord.x_;
                *vertexData++ = texCoord.y_;
                
                // Tangent
                Vector3 xyz = (Vector3::RIGHT - normal * normal.DotProduct(Vector3::RIGHT)).Normalized();
                *vertexData++ = xyz.x_;
                *vertexData++ = xyz.y_;
                *vertexData++ = xyz.z_;
                *vertexData++ = 1.0f;
            }
        }
        
        vertexBuffer->Unlock();
        vertexBuffer->ClearDataLost();
    }
    
    patch->boundingBox_ = box;
    patch->geometry_->SetIndexBuffer(indexBuffer_);
    patch->geometry_->SetDrawRange(TRIANGLE_LIST, 0, indexBuffer_->GetIndexCount());
    patch->OnMarkedDirty(patch->GetNode());
}

void Terrain::UpdatePatchLOD(TerrainPatch* patch, unsigned lod, unsigned northLod, unsigned southLod, unsigned westLod,
    unsigned eastLod)
{
}

void Terrain::OnMarkedDirty(Node* node)
{
    const Matrix3x4& worldTransform = node_->GetWorldTransform();
    
    for (Vector<SharedPtr<TerrainPatch> >::Iterator i = patches_.Begin(); i != patches_.End(); ++i)
        SetPatchTransform(*i);
}

void Terrain::CreateGeometry()
{
    Scene* scene = GetScene();
    Octree* octree = scene ? scene->GetComponent<Octree>() : 0;
    if (!heightMap_ || !node_ || !octree)
        return;
    
    PROFILE(CreateTerrainGeometry);
    
    patches_.Clear();
    patchNodes_.Clear();
    
    // Determine total terrain size & copy heightmap data
    patchesX_ = (heightMap_->GetWidth() - 1) / patchSize_;
    patchesZ_ = (heightMap_->GetHeight() - 1) / patchSize_;
    size_ = IntVector2(patchesX_ * patchSize_ + 1, patchesZ_ * patchSize_ + 1);
    patchWorldSize_ = Vector2(spacing_.x_ * (float)patchSize_, spacing_.z_ * (float)patchSize_);
    patchWorldOrigin_ = Vector2(-0.5f * (float)patchesX_ * patchWorldSize_.x_, -0.5f * (float)patchesZ_ * patchWorldSize_.y_);
    heightData_ = new float[size_.x_ * size_.y_];
    
    const unsigned char* src = heightMap_->GetData();
    float* dest = heightData_;
    unsigned imgComps = heightMap_->GetComponents();
    unsigned imgRow = heightMap_->GetWidth() * imgComps;
    
    for (int z = 0; z < size_.y_; ++z)
    {
        for (int x = 0; x < size_.x_; ++x)
            *dest++ = (float)src[imgRow * (size_.y_ - 1 - z) + imgComps * x] * spacing_.y_;
    }
    
    // Create scene nodes for patches
    const Matrix3x4& worldTransform = node_->GetWorldTransform();
    
    for (unsigned z = 0; z < patchesZ_; ++z)
    {
        for (unsigned x = 0; x < patchesX_; ++x)
        {
            // Note: terrain nodes are not created as part of the scene, as it's runtime generated scene hierarchy
            // we never want to save, serialize through network, or show in the editor
            Node* patchNode = new Node(context_);
            patchNode->SetID(FIRST_LOCAL_ID);
            
            TerrainPatch* patch = new TerrainPatch(context_);
            patch->owner_ = this;
            patch->x_ = x;
            patch->z_ = z;
            patchNode->AddComponent(patch, FIRST_LOCAL_ID, LOCAL);
            octree->AddManualDrawable(patch);
            
            // Copy initial drawable parameters
            patch->batches_[0].material_ = material_;
            patch->SetDrawDistance(drawDistance_);
            patch->SetShadowDistance(shadowDistance_);
            patch->SetLodBias(lodBias_);
            patch->SetViewMask(viewMask_);
            patch->SetLightMask(lightMask_);
            patch->SetShadowMask(shadowMask_);
            patch->SetZoneMask(zoneMask_);
            patch->SetMaxLights(maxLights_);
            patch->SetVisible(visible_);
            patch->SetCastShadows(castShadows_);
            patch->SetOccluder(occluder_);
            patch->SetOccludee(occludee_);
            
            patches_.Push(SharedPtr<TerrainPatch>(patch));
            patchNodes_.Push(SharedPtr<Node>(patchNode));
        }
    }
    
    // Create the shared index data
    /// \todo Create LOD levels
    indexBuffer_->SetSize(patchSize_ * patchSize_ * 6, false);
    unsigned vertexDataRow = patchSize_ + 1;
    unsigned short* indexData = (unsigned short*)indexBuffer_->Lock(0, indexBuffer_->GetIndexCount());
    
    if (indexData)
    {
        for (unsigned z = 0; z < patchSize_; ++z)
        {
            for (unsigned x = 0; x < patchSize_; ++x)
            {
                *indexData++ = x + (z + 1) * vertexDataRow;
                *indexData++ = x + z * vertexDataRow + 1;
                *indexData++ = x + z * vertexDataRow;
                
                *indexData++ = x + (z + 1) * vertexDataRow;
                *indexData++ = x + (z + 1) * vertexDataRow + 1;
                *indexData++ = x + z * vertexDataRow + 1;
            }
        }
        
        indexBuffer_->Unlock();
    }
    
    // Create vertex data for patches, and set transform
    for (Vector<SharedPtr<TerrainPatch> >::Iterator i = patches_.Begin(); i != patches_.End(); ++i)
    {
        SetPatchTransform(*i);
        UpdatePatchGeometry(*i);
    }
    
    using namespace TerrainCreated;
    
    VariantMap eventData;
    eventData[P_NODE] = (void*)node_;
    SendEvent(E_TERRAINCREATED, eventData);
}

void Terrain::SetPatchTransform(TerrainPatch* patch)
{
    Node* patchNode = patch->GetNode();
    unsigned x = patch->x_;
    unsigned z = patch->z_;
    
    Matrix3x4 patchTransform(Vector3(patchWorldOrigin_.x_ + (float)x * patchWorldSize_.x_, 0.0f, patchWorldOrigin_.y_ +
        (float)z * patchWorldSize_.y_), Quaternion::IDENTITY, 1.0f);
    Matrix3x4 combinedTransform = node_->GetWorldTransform() * patchTransform;
    patchNode->SetTransform(combinedTransform.Translation(), combinedTransform.Rotation(), combinedTransform.Scale());
}

float Terrain::GetRawHeight(unsigned x, unsigned z) const
{
    if (!heightData_)
        return 0.0f;
    
    x = Clamp((int)x, 0, (int)size_.x_ - 1);
    z = Clamp((int)z, 0, (int)size_.y_ - 1);
    return heightData_[z * size_.x_ + x];
}

Vector3 Terrain::GetNormal(unsigned x, unsigned z) const
{
    float baseHeight = GetRawHeight(x, z);
    float nSlope = GetRawHeight(x, z - 1) - baseHeight;
    float neSlope = GetRawHeight(x + 1, z - 1) - baseHeight;
    float eSlope = GetRawHeight(x + 1, z) - baseHeight;
    float seSlope = GetRawHeight(x + 1, z + 1) - baseHeight;
    float sSlope = GetRawHeight(x, z + 1) - baseHeight;
    float swSlope = GetRawHeight(x - 1, z + 1) - baseHeight;
    float wSlope = GetRawHeight(x - 1, z) - baseHeight;
    float nwSlope = GetRawHeight(x - 1, z - 1) - baseHeight;
    
    return (Vector3(0.0f, 1.0f, nSlope) +
        Vector3(-neSlope, 1.0f, neSlope) +
        Vector3(-eSlope, 1.0f, 0.0f) +
        Vector3(-seSlope, 1.0f, -seSlope) +
        Vector3(0.0f, 1.0f, -sSlope) +
        Vector3(swSlope, 1.0f, -swSlope) + 
        Vector3(wSlope, 1.0f, 0.0f) +
        Vector3(nwSlope, 1.0f, nwSlope)).Normalized();
}

void Terrain::HandleHeightMapReloadFinished(StringHash eventType, VariantMap& eventData)
{
    CreateGeometry();
}