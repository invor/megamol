#include "MSMConvexHullMeshDataSource.h"

#include <QuickHull.hpp>

#include "mesh/MeshCalls.h"

#include "ArchVisCalls.h"

megamol::archvis::MSMConvexHullDataSource::MSMConvexHullDataSource()
    : m_MSM_callerSlot("getMSM", "Connects the "), m_version(0) {
    this->m_MSM_callerSlot.SetCompatibleCall<CallMSMDataDescription>();
    this->MakeSlotAvailable(&this->m_MSM_callerSlot);
}

megamol::archvis::MSMConvexHullDataSource::~MSMConvexHullDataSource() {}

bool megamol::archvis::MSMConvexHullDataSource::getDataCallback(core::Call& caller) {
    mesh::CallGPUMeshData* mc = dynamic_cast<mesh::CallGPUMeshData*>(&caller);
    if (mc == NULL) return false;

    CallMSMData* msm_call = this->m_MSM_callerSlot.CallAs<megamol::archvis::CallMSMData>();
    if (msm_call == NULL) return false;

    if (!(*msm_call)(0)) return false;


    if (msm_call->hasUpdate())
    {
        ++m_version;

        // TODO create mesh
        quickhull::QuickHull<float> qh;
        std::vector<quickhull::Vector3<float>> point_cloud;

        auto msm = msm_call->getData();

        size_t node_cnt = msm->getNodeCount();
        point_cloud.reserve(node_cnt);

        for (int i = 0; i < node_cnt; ++i) {
            point_cloud.push_back(quickhull::Vector3<float>(msm->accessNodePositions()[i].X(),
                msm->accessNodePositions()[i].Y(), msm->accessNodePositions()[i].Z()));
        }

        auto hull = qh.getConvexHull(point_cloud, true, false);
        auto indexBuffer = hull.getIndexBuffer();
        auto vertexBuffer = hull.getVertexBuffer();
    }

    mc->setData(m_gpu_meshes, m_version);

    return true;
}

bool megamol::archvis::MSMConvexHullDataSource::getMetaDataCallback(core::Call& caller) { return false; }
