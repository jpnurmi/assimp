/*
Open Asset Import Library (assimp)
----------------------------------------------------------------------

Copyright (c) 2006-2020, assimp team

All rights reserved.

Redistribution and use of this software in source and binary forms,
with or without modification, are permitted provided that the
following conditions are met:

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

----------------------------------------------------------------------
*/

/** @file GcodeLoader.h
 *  Declaration of the G-code importer class.
 */
#ifndef AI_GCODELOADER_H_INCLUDED
#define AI_GCODELOADER_H_INCLUDED

#include <assimp/BaseImporter.h>
#include <assimp/types.h>

struct aiNode;

namespace Assimp {

/**
 * @brief   Importer class for the G-Code file format.
 */
class GcodeImporter : public BaseImporter
{
public:
    /**
     * @brief GcodeImporter, the class default constructor.
     */
    GcodeImporter() = default;

    /**
     * @brief   Returns whether the class can handle the format of the given file.
     *  See BaseImporter::CanRead() for details.
     */
    bool CanRead(const std::string& path, IOSystem* pIOHandler, bool checkSig) const;

protected:
    /**
     * @brief   Return importer meta information.
     *  See #BaseImporter::GetInfo for the details
     */
    const aiImporterDesc* GetInfo() const;

    /**
     * @brief   Imports the given file into the given scene structure.
    * See BaseImporter::InternReadFile() for details
    */
    void InternReadFile(const std::string& path, aiScene* pScene,
        IOSystem* pIOHandler);

private:
    void ReadGcodeFile(const char *sz, aiScene *pScene);

    enum GcodeMove { None, Travel, Extrusion };
    GcodeMove ReadGcodeMove(const char *&sz, aiVector3D &pos);

    aiVector3D ToLogicalPosition(const aiVector3D &pos) const;
    aiVector3D ToAbsolutePosition(const aiVector3D &pos) const;

    bool m_absolute = true;
    aiVector3D m_pos = {0, 0, 0};
    aiVector3D m_offset = {0, 0, 0};
};

} // namespace Assimp

#endif // AI_GCODELOADER_H_INCLUDED
