/*
---------------------------------------------------------------------------
Open Asset Import Library (assimp)
---------------------------------------------------------------------------

Copyright (c) 2006-2020, assimp team

All rights reserved.

Redistribution and use of this software in source and binary forms,
with or without modification, are permitted provided that the following
conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the assimp team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the assimp team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
---------------------------------------------------------------------------
*/

/** @file Implementation of the G-code importer class */

#ifndef ASSIMP_BUILD_NO_GCODE_IMPORTER

#include "GcodeLoader.h"
#include <assimp/fast_atof.h>
#include <assimp/importerdesc.h>
#include <assimp/IOSystem.hpp>
#include <assimp/ParsingUtils.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <memory>

using namespace Assimp;

namespace {

static const aiImporterDesc desc = {
    "G-code Importer",
    "",
    "",
    "",
    aiImporterFlags_SupportTextFlavour,
    0,
    0,
    0,
    0,
    "gcode"
};

} // namespace

bool GcodeImporter::CanRead(const std::string &path, IOSystem */*pIOHandler*/, bool /*checkSig*/) const
{
    return SimpleExtensionCheck(path, "gcode");
}

const aiImporterDesc *GcodeImporter::GetInfo() const
{
    return &desc;
}

void GcodeImporter::InternReadFile(const std::string &path, aiScene *pScene, IOSystem *pIOHandler)
{
    std::unique_ptr<IOStream> pFile(pIOHandler->Open(path, "r"));
    if (!pFile.get())
        throw DeadlyImportError("Failed to open G-code file " + path + ".");

    std::vector<char> buffer;
    TextFileToBuffer(pFile.get(), buffer);

    pScene->mRootNode = new aiNode;
    ReadGcodeFile(&buffer[0], pScene);

    aiMaterial *pMaterial = new aiMaterial;
    aiString name(AI_DEFAULT_MATERIAL_NAME);
    pMaterial->AddProperty(&name, AI_MATKEY_NAME);

    aiColor4D diffuse(ai_real(1.0), ai_real(1.0), ai_real(1.0), ai_real(1.0));
    pMaterial->AddProperty(&diffuse, 1, AI_MATKEY_COLOR_DIFFUSE);
    pMaterial->AddProperty(&diffuse, 1, AI_MATKEY_COLOR_SPECULAR);

    aiColor4D ambient = aiColor4D(ai_real(0.05), ai_real(0.05), ai_real(0.05), ai_real(1.0));
    pMaterial->AddProperty(&ambient, 1, AI_MATKEY_COLOR_AMBIENT);

    pScene->mNumMaterials = 1;
    pScene->mMaterials = new aiMaterial *[1];
    pScene->mMaterials[0] = pMaterial;
}

void GcodeImporter::ReadGcodeFile(const char *sz, aiScene *pScene)
{
    std::vector<aiMesh *> meshes;
    std::vector<aiNode *> nodes;

    std::vector<int> indices;
    std::vector<aiVector3D> positions;

    auto CreateMesh = [&]() {
        if (positions.empty())
            return;

        aiMesh *pMesh = new aiMesh;
        pMesh->mName.Set(std::to_string(meshes.size()));
        pMesh->mNumVertices = positions.size();
        pMesh->mVertices = new aiVector3D[pMesh->mNumVertices];
        for (size_t i = 0; i<pMesh->mNumVertices; ++i )
            pMesh->mVertices[i] = ToAbsolutePosition(positions[i]);

        pMesh->mPrimitiveTypes = aiPrimitiveType_LINE;
        pMesh->mNumFaces = indices.size() / 2;
        pMesh->mFaces = new aiFace[pMesh->mNumFaces];
        for (size_t i = 0, p = 0; i < pMesh->mNumFaces; ++i) {
            aiFace &face = pMesh->mFaces[i];
            face.mIndices = new unsigned int[face.mNumIndices = 2];
            for (unsigned int o = 0; o < 2; ++o, ++p)
                face.mIndices[o] = indices[p];
        }

        aiNode *pNode = new aiNode;
        pNode->mParent = pScene->mRootNode;
        pNode->mNumMeshes = 1;
        pNode->mMeshes = new unsigned int[1];
        pNode->mMeshes[0] = meshes.size();

        nodes.push_back(pNode);
        meshes.push_back(pMesh);

        indices.clear();
        positions.clear();
    };

    while (SkipSpacesAndLineEnd(&sz)) {
        if (strncasecmp(sz, "G", 1) == 0) {
            aiVector3D pos = m_pos;
            GcodeMove move = ReadGcodeMove(++sz, pos);
            if (move == Extrusion) {
                positions.push_back(m_pos);
                positions.push_back(pos);
                size_t size = positions.size();
                if (size > 1) {
                    indices.push_back(size - 2);
                    indices.push_back(size - 1);
                }
            } else if (move == Travel) {
                CreateMesh();
            }
            m_pos = pos;
        } else {
            SkipLine(&sz);
        }
    }
    CreateMesh();

    pScene->mNumMeshes = meshes.size();
    pScene->mMeshes = new aiMesh *[meshes.size()];
    for (size_t i = 0; i < meshes.size(); ++i)
        pScene->mMeshes[i] = meshes[i];

    pScene->mRootNode->mName.Set("G");
    pScene->mRootNode->mNumChildren = nodes.size();
    pScene->mRootNode->mChildren = new aiNode *[nodes.size()];
    for (size_t i = 0; i < nodes.size(); ++i)
        pScene->mRootNode->mChildren[i] = nodes[i];
}

struct GcodeLine {
    bool HasX() const { return !std::isnan(x); }
    bool HasY() const { return !std::isnan(y); }
    bool HasZ() const { return !std::isnan(z); }
    bool HasE() const { return !std::isnan(e); }
    ai_real GetX(ai_real f = 0) { return HasX() ? x : f; }
    ai_real GetY(ai_real f = 0) { return HasY() ? y : f; }
    ai_real GetZ(ai_real f = 0) { return HasZ() ? z : f; }
    ai_real GetE(ai_real f = 0) { return HasE() ? e : f; }
    ai_real x = NAN, y = NAN, z = NAN, e = NAN;
};

template <class char_t>
AI_FORCE_INLINE
bool IsComment(char_t in) { return (in==(char_t)';'); }

AI_FORCE_INLINE
void SkipValue(const char *&sz)
{
    while (!IsSpaceOrNewLine(*sz) && !IsComment(*sz))
        ++sz;
}

static GcodeLine ReadGcodeLine(const char *&sz)
{
    GcodeLine v;
    bool comment = false;
    while (!comment && SkipSpaces(&sz)) {
        switch (ToUpper(*sz++)) {
        case 'X':
            v.x = fast_atof(&sz);
            break;
        case 'Y':
            v.y = fast_atof(&sz);
            break;
        case 'Z':
            v.z = fast_atof(&sz);
            break;
        case 'E':
            v.e = fast_atof(&sz);
            break;
        case ';':
            comment = true;
            break;
        default:
            SkipValue(sz);
            break;
        }
    }
    return v;
}

GcodeImporter::GcodeMove GcodeImporter::ReadGcodeMove(const char *&sz, aiVector3D &pos)
{
    int g = strtoul10(sz, &sz);
    GcodeLine v = ReadGcodeLine(sz);
    switch (g) {
    case 0:
    case 1:
        if (m_absolute) {
            pos.x = v.GetX(m_pos.x);
            pos.y = v.GetY(m_pos.y);
            pos.z = v.GetZ(m_pos.z);
            break;
        }
        // fallthrough
    case 7: // relative
        pos.x = m_pos.x + v.GetX();
        pos.y = m_pos.y + v.GetY();
        pos.z = m_pos.z + v.GetZ();
        break;
    case 90:
        m_absolute = true;
        return GcodeImporter::None;
    case 91:
        m_absolute = false;
        return GcodeImporter::None;
    case 92:
        if (!v.HasX() && !v.HasY() && !v.HasZ() && !v.HasE()) {
            m_offset = ToAbsolutePosition(m_pos);
            m_pos = aiVector3D(0, 0, 0);
        } else {
            const aiVector3D abs = ToAbsolutePosition(m_pos);
            if (v.HasX()) {
                m_pos.x = v.GetX();
                m_offset.x = abs.x - m_pos.x;
            }
            if (v.HasY()) {
                m_pos.y = v.GetY();
                m_offset.y = abs.y - m_pos.y;
            }
            if (v.HasZ()) {
                m_pos.z = v.GetZ();
                m_offset.z = abs.z - m_pos.y;
            }
        }
        // fallthrough
    default:
        return GcodeImporter::None;
    }

    if (v.GetE() > 0)
        return GcodeImporter::Extrusion;

    if (v.HasX() || v.HasY() || v.HasZ())
        return GcodeImporter::Travel;

    return GcodeImporter::None;
}

aiVector3D GcodeImporter::ToLogicalPosition(const aiVector3D &pos) const
{
    return pos - m_offset;
}

aiVector3D GcodeImporter::ToAbsolutePosition(const aiVector3D &pos) const
{
    return m_offset + pos;
}

#endif // !! ASSIMP_BUILD_NO_GCODE_IMPORTER
