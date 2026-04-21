#pragma once

#include "glmUSD.h"

USD_INCLUDES_START
#include <pxr/pxr.h>
USD_INCLUDES_END
#if PXR_VERSION > 2505

USD_INCLUDES_START
#include <pxr/imaging/hd/dataSourceLocator.h>
#include <pxr/imaging/hd/retainedDataSource.h>
USD_INCLUDES_END

namespace glm
{
    namespace hydra
    {
        /*
         * Interface implemented by the FileMeshInstance and FbxMeshAdapter classes, so
         * that the plugin need not know whether the mesh prims they generate come from
         * a GCG or FBX character.
         */
        class MeshDataSourceBase
        {
        public:
            MeshDataSourceBase() = default;
            virtual ~MeshDataSourceBase() = default;

            virtual PXR_NS::HdContainerDataSourceHandle GetDataSource() const = 0;
            virtual PXR_NS::HdDataSourceLocatorSet GetVariableDataSources() const = 0;
        };
    } // namespace hydra
} // namespace glm

#endif // PXR_VERSION > 2505
