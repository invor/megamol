/*
 * CartoonTessellationRenderer.cpp
 *
 * Copyright (C) 2014 by VISUS (Universitaet Stuttgart)
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "CartoonTessellationRenderer.h"
#include <inttypes.h>
#include <stdint.h>
#include "mmcore/CoreInstance.h"
#include "mmcore/param/BoolParam.h"
#include "mmcore/param/FloatParam.h"
#include "mmcore/param/Vector4fParam.h"
#include "mmcore/utility/ShaderFactory.h"
#include "mmcore/utility/ShaderSourceFactory.h"
#include "mmcore/view/CallClipPlane.h"
#include "mmcore/view/CallGetTransferFunction.h"
#include "mmcore/view/CallRender3DGL.h"
#include "mmcore/view/light/PointLight.h"
#include "protein_calls/MolecularDataCall.h"
#include "vislib/assert.h"
#include "vislib/math/Matrix.h"
#include "vislib/math/ShallowMatrix.h"
#include "vislib/math/mathfunctions.h"

using namespace megamol::core;
using namespace megamol::core::view;
using namespace megamol::protein;
using namespace megamol::protein_calls;
using namespace megamol::core::utility::log;

const GLuint SSBObindingPoint = 2;

/*
 * moldyn::CartoonTessellationRenderer::CartoonTessellationRenderer
 */
CartoonTessellationRenderer::CartoonTessellationRenderer(void)
        : view::Renderer3DModuleGL()
        , getDataSlot("getdata", "Connects to the data source")
        , getLightsSlot("lights", "Lights are retrieved over this slot.")
        , fences()
        , currBuf(0)
        , bufSize(32 * 1024 * 1024)
        , numBuffers(3)
        , scalingParam("scaling", "scaling factor for particle radii")
        , sphereParam("spheres", "render atoms as spheres")
        , lineParam("lines", "render backbone as GL_LINE")
        , backboneParam("backbone", "render backbone as tubes")
        , backboneWidthParam("backbone width", "the width of the backbone")
        , materialParam("material", "ambient, diffuse, specular components + exponent")
        , lineDebugParam("wireframe", "render in wireframe mode")
        , colorInterpolationParam("color interpolation", "should the colors be interpolated?")
        ,
        // this variant should not need the fence
        singleBufferCreationBits(GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT)
        , singleBufferMappingBits(GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT)
        , cartoonShader_(nullptr)
        , lineShader_(nullptr) {

    this->getDataSlot.SetCompatibleCall<MolecularDataCallDescription>();
    this->MakeSlotAvailable(&this->getDataSlot);

    this->getLightsSlot.SetCompatibleCall<core::view::light::CallLightDescription>();
    this->MakeSlotAvailable(&this->getLightsSlot);

    this->sphereParam << new core::param::BoolParam(false);
    this->MakeSlotAvailable(&this->sphereParam);

    this->lineParam << new core::param::BoolParam(true);
    this->MakeSlotAvailable(&this->lineParam);

    this->backboneParam << new core::param::BoolParam(true);
    this->MakeSlotAvailable(&this->backboneParam);

    this->scalingParam << new core::param::FloatParam(1.0f);
    this->MakeSlotAvailable(&this->scalingParam);

    this->backboneWidthParam << new core::param::FloatParam(0.25f);
    this->MakeSlotAvailable(&this->backboneWidthParam);

    this->lineDebugParam << new core::param::BoolParam(false);
    this->MakeSlotAvailable(&this->lineDebugParam);

    this->colorInterpolationParam << new core::param::BoolParam(false);
    this->MakeSlotAvailable(&this->colorInterpolationParam);

    /*float components[4] = { 0.2f, 0.8f, 0.4f, 10.0f };
    vislib::math::Vector<float, 4U> myvec(components);
    this->materialParam << new core::param::Vector4fParam(myvec);
    this->MakeSlotAvailable(&this->materialParam);*/

    fences.resize(numBuffers);
}


/*
 * moldyn::CartoonTessellationRenderer::~CartoonTessellationRenderer
 */
CartoonTessellationRenderer::~CartoonTessellationRenderer(void) {
    this->Release();
}

void CartoonTessellationRenderer::queueSignal(GLsync& syncObj) {
    if (syncObj) {
        glDeleteSync(syncObj);
    }
    syncObj = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void CartoonTessellationRenderer::waitSignal(GLsync& syncObj) {
    if (syncObj) {
        while (1) {
            GLenum wait = glClientWaitSync(syncObj, GL_SYNC_FLUSH_COMMANDS_BIT, 1);
            if (wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED) {
                return;
            }
        }
    }
}


/*
 * moldyn::SimpleSphereRenderer::create
 */
bool CartoonTessellationRenderer::create(void) {
    using namespace vislib::graphics::gl;

    if (!vislib::graphics::gl::GLSLShader::InitialiseExtensions())
        return false;
    if (!vislib::graphics::gl::GLSLTesselationShader::InitialiseExtensions())
        return false;

    try {
        auto const shdr_options = msf::ShaderFactoryOptionsOpenGL(this->GetCoreInstance()->GetShaderPaths());

        cartoonShader_ = core::utility::make_shared_glowl_shader("cartoon", shdr_options,
            std::filesystem::path("cartoontessellation/ctess_cartoon.vert.glsl"),
            std::filesystem::path("cartoontessellation/ctess_cartoon.tesc.glsl"),
            std::filesystem::path("cartoontessellation/ctess_cartoon.tese.glsl"),
            std::filesystem::path("cartoontessellation/ctess_cartoon.geom.glsl"),
            std::filesystem::path("cartoontessellation/ctess_cartoon.frag.glsl"));

        lineShader_ = core::utility::make_shared_glowl_shader("line", shdr_options,
            std::filesystem::path("cartoontessellation/ctess_splineline.vert.glsl"),
            std::filesystem::path("cartoontessellation/ctess_splineline.tesc.glsl"),
            std::filesystem::path("cartoontessellation/ctess_splineline.tese.glsl"),
            std::filesystem::path("cartoontessellation/ctess_splineline.geom.glsl"),
            std::filesystem::path("cartoontessellation/ctess_splineline.frag.glsl"));

    } catch (glowl::GLSLProgramException const& ex) {
        megamol::core::utility::log::Log::DefaultLog.WriteMsg(
            megamol::core::utility::log::Log::LEVEL_ERROR, "[CartoonTessellationRenderer] %s", ex.what());
    } catch (std::exception const& ex) {
        megamol::core::utility::log::Log::DefaultLog.WriteMsg(megamol::core::utility::log::Log::LEVEL_ERROR,
            "[CartoonTessellationRenderer] Unable to compile shader: Unknown exception: %s", ex.what());
    } catch (...) {
        megamol::core::utility::log::Log::DefaultLog.WriteMsg(megamol::core::utility::log::Log::LEVEL_ERROR,
            "[CartoonTessellationRenderer] Unable to compile shader: Unknown exception.");
    }

    glGenVertexArrays(1, &this->vertArray);
    glBindVertexArray(this->vertArray);
    glGenBuffers(1, &this->theSingleBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->theSingleBuffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, this->bufSize * this->numBuffers, nullptr, singleBufferCreationBits);
    this->theSingleMappedMem =
        glMapNamedBufferRangeEXT(this->theSingleBuffer, 0, this->bufSize * this->numBuffers, singleBufferMappingBits);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

/*
 * moldyn::SimpleSphereRenderer::release
 */
void CartoonTessellationRenderer::release(void) {
    glUnmapNamedBufferEXT(this->theSingleBuffer);
    for (auto& x : fences) {
        if (x) {
            glDeleteSync(x);
        }
    }
    // this->sphereShader.Release();
    // TODO release all shaders!
    glDeleteVertexArrays(1, &this->vertArray);
    glDeleteBuffers(1, &this->theSingleBuffer);
}


void CartoonTessellationRenderer::getBytesAndStride(MolecularDataCall& mol, unsigned int& colBytes,
    unsigned int& vertBytes, unsigned int& colStride, unsigned int& vertStride) {
    vertBytes = 0;
    colBytes = 0;
    // colBytes = vislib::math::Max(colBytes, 3 * 4U);
    vertBytes = vislib::math::Max(vertBytes, (unsigned int) sizeof(CAlpha));

    colStride = 0;
    colStride = colStride < colBytes ? colBytes : colStride;
    vertStride = 0;
    vertStride = vertStride < vertBytes ? vertBytes : vertStride;
}

void CartoonTessellationRenderer::getBytesAndStrideLines(MolecularDataCall& mol, unsigned int& colBytes,
    unsigned int& vertBytes, unsigned int& colStride, unsigned int& vertStride) {
    vertBytes = 0;
    colBytes = 0;
    // colBytes = vislib::math::Max(colBytes, 3 * 4U);
    vertBytes = vislib::math::Max(vertBytes, 4 * 4U);

    colStride = 0;
    colStride = colStride < colBytes ? colBytes : colStride;
    vertStride = 0;
    vertStride = vertStride < vertBytes ? vertBytes : vertStride;
}

/*
 * GetExtents
 */
bool CartoonTessellationRenderer::GetExtents(view::CallRender3DGL& call) {
    view::CallRender3DGL* cr = dynamic_cast<view::CallRender3DGL*>(&call);
    if (cr == NULL)
        return false;

    MolecularDataCall* mol = this->getDataSlot.CallAs<MolecularDataCall>();
    if ((mol != NULL) && ((*mol)(MolecularDataCall::CallForGetExtent))) {
        cr->SetTimeFramesCount(mol->FrameCount());
        cr->AccessBoundingBoxes() = mol->AccessBoundingBoxes();
    } else {
        cr->SetTimeFramesCount(1);
        cr->AccessBoundingBoxes().Clear();
    }

    return true;
}

/*
 *  getData
 */
MolecularDataCall* CartoonTessellationRenderer::getData(unsigned int t, float& outScaling) {
    MolecularDataCall* mol = this->getDataSlot.CallAs<MolecularDataCall>();
    outScaling = 1.0f;
    if (mol != NULL) {
        mol->SetFrameID(t);
        if (!(*mol)(MolecularDataCall::CallForGetExtent))
            return NULL;

        // calculate scaling
        outScaling = mol->AccessBoundingBoxes().ObjectSpaceBBox().LongestEdge();
        if (outScaling > 0.0000001) {
            outScaling = 10.0f / outScaling;
        } else {
            outScaling = 1.0f;
        }

        mol->SetFrameID(t);
        if (!(*mol)(MolecularDataCall::CallForGetData))
            return NULL;

        return mol;
    } else {
        return NULL;
    }
}

/*
 * moldyn::SimpleSphereRenderer::Render
 */
bool CartoonTessellationRenderer::Render(view::CallRender3DGL& call) {
    view::CallRender3DGL* cr = dynamic_cast<view::CallRender3DGL*>(&call);
    if (cr == NULL)
        return false;

    float scaling = 1.0f;
    MolecularDataCall* mol = this->getData(static_cast<unsigned int>(cr->Time()), scaling);
    if (mol == NULL)
        return false;

    //	timer.BeginFrame();

    glm::vec4 clipDat;
    glm::vec4 clipCol;
    clipDat[0] = clipDat[1] = clipDat[2] = clipDat[3] = 0.0f;
    clipCol[0] = clipCol[1] = clipCol[2] = 0.75f;
    clipCol[3] = 1.0f;

    core::view::Camera cam = cr->GetCamera();
    glm::mat4 view = cam.getViewMatrix();
    glm::mat4 proj = cam.getProjectionMatrix();
    glm::mat4 mvp = proj * view;

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    // TODO make this correct
    glm::vec4 viewportStuff;
    ::glGetFloatv(GL_VIEWPORT, glm::value_ptr(viewportStuff));
    glPointSize(vislib::math::Max(viewportStuff[2], viewportStuff[3]));
    if (viewportStuff[2] < 1.0f)
        viewportStuff[2] = 1.0f;
    if (viewportStuff[3] < 1.0f)
        viewportStuff[3] = 1.0f;
    viewportStuff[2] = 2.0f / viewportStuff[2];
    viewportStuff[3] = 2.0f / viewportStuff[3];

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, theSingleBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBObindingPoint, this->theSingleBuffer);

    // matrices
    auto viewInv = glm::inverse(view);
    auto mvpInv = glm::inverse(mvp);
    auto mvpTrans = glm::transpose(mvp);
    auto mvpInvTrans = glm::transpose(mvpInv);
    auto projInv = glm::transpose(proj);

    // copy data
    if (this->positionsCa.Count() != mol->MoleculeCount()) {
        this->positionsCa.SetCount(mol->MoleculeCount());
        this->positionsO.SetCount(mol->MoleculeCount());
    }
    unsigned int firstResIdx = 0;
    unsigned int lastResIdx = 0;
    unsigned int firstAtomIdx = 0;
    unsigned int lastAtomIdx = 0;
    unsigned int atomTypeIdx = 0;
    unsigned int firstSecIdx = 0;
    unsigned int lastSecIdx = 0;
    unsigned int firstAAIdx = 0;
    unsigned int lastAAIdx = 0;

    unsigned int cIndex = 0;
    unsigned int oIndex = 0;

    mainchain.clear();

    CAlpha lastCalpha;

    int molCount = mol->MoleculeCount();
    std::vector<int> molSizes;

    // loop over all molecules of the protein
    for (unsigned int molIdx = 0; molIdx < mol->MoleculeCount(); molIdx++) {

        MolecularDataCall::Molecule chain = mol->Molecules()[molIdx];
        molSizes.push_back(0);

        bool firstset = false;

        // is the first residue an aminoacid?
        if (mol->Residues()[chain.FirstResidueIndex()]->Identifier() != MolecularDataCall::Residue::AMINOACID) {
            continue;
        }

        firstSecIdx = chain.FirstSecStructIndex();
        lastSecIdx = firstSecIdx + chain.SecStructCount();

        // loop over all secondary structures of the molecule
        for (unsigned int secIdx = firstSecIdx; secIdx < lastSecIdx; secIdx++) {
            firstAAIdx = mol->SecondaryStructures()[secIdx].FirstAminoAcidIndex();
            lastAAIdx = firstAAIdx + mol->SecondaryStructures()[secIdx].AminoAcidCount();

            // loop over all aminoacids inside the secondary structure
            for (unsigned int aaIdx = firstAAIdx; aaIdx < lastAAIdx; aaIdx++) {

                MolecularDataCall::AminoAcid* acid;

                // is the current residue really an aminoacid?
                if (mol->Residues()[aaIdx]->Identifier() == MolecularDataCall::Residue::AMINOACID)
                    acid = (MolecularDataCall::AminoAcid*) (mol->Residues()[aaIdx]);
                else
                    continue;

                // extract relevant positions and other values
                CAlpha calpha;
                calpha.pos[0] = mol->AtomPositions()[3 * acid->CAlphaIndex()];
                calpha.pos[1] = mol->AtomPositions()[3 * acid->CAlphaIndex() + 1];
                calpha.pos[2] = mol->AtomPositions()[3 * acid->CAlphaIndex() + 2];
                calpha.pos[3] = 1.0f;

                calpha.dir[0] = mol->AtomPositions()[3 * acid->OIndex()] - calpha.pos[0];
                calpha.dir[1] = mol->AtomPositions()[3 * acid->OIndex() + 1] - calpha.pos[1];
                calpha.dir[2] = mol->AtomPositions()[3 * acid->OIndex() + 2] - calpha.pos[2];

                auto type = mol->SecondaryStructures()[secIdx].Type();
                calpha.type = (int) type;
                molSizes[molIdx]++;

                // TODO do this on GPU?
                // orientation check for the direction
                if (mainchain.size() != 0) {
                    CAlpha before = mainchain[mainchain.size() - 1];
                    float dotProd =
                        calpha.dir[0] * before.dir[0] + calpha.dir[1] * before.dir[1] + calpha.dir[2] * before.dir[2];

                    if (dotProd < 0) // flip direction if the orientation is wrong
                    {
                        calpha.dir[0] = -calpha.dir[0];
                        calpha.dir[1] = -calpha.dir[1];
                        calpha.dir[2] = -calpha.dir[2];
                    }
                }

                mainchain.push_back(calpha);

                lastCalpha = calpha;

                // add the first atom 3 times
                if (!firstset) {
                    mainchain.push_back(calpha);
                    mainchain.push_back(calpha);
                    molSizes[molIdx] += 2;
                    firstset = true;
                }
            }
        }

        // add the last atom 3 times
        mainchain.push_back(lastCalpha);
        mainchain.push_back(lastCalpha);
        molSizes[molIdx] += 2;
    }

#ifdef FIRSTFRAME_CHECK
    if (firstFrame) {
        for (int i = 0; i < mainchain.size(); i++) {
            std::cout << mainchain[i].type << std::endl;
        }
        firstFrame = false;
    }
#endif

    firstResIdx = 0;
    lastResIdx = 0;
    firstAtomIdx = 0;
    lastAtomIdx = 0;
    atomTypeIdx = 0;

    for (unsigned int molIdx = 0; molIdx < mol->MoleculeCount(); molIdx++) {
        this->positionsCa[molIdx].Clear();
        this->positionsCa[molIdx].AssertCapacity(mol->Molecules()[molIdx].ResidueCount() * 4 + 16);
        this->positionsO[molIdx].Clear();
        this->positionsO[molIdx].AssertCapacity(mol->Molecules()[molIdx].ResidueCount() * 4 + 16);

        // bool first;
        firstResIdx = mol->Molecules()[molIdx].FirstResidueIndex();
        lastResIdx = firstResIdx + mol->Molecules()[molIdx].ResidueCount();
        for (unsigned int resIdx = firstResIdx; resIdx < lastResIdx; resIdx++) {
            firstAtomIdx = mol->Residues()[resIdx]->FirstAtomIndex();
            lastAtomIdx = firstAtomIdx + mol->Residues()[resIdx]->AtomCount();
            for (unsigned int atomIdx = firstAtomIdx; atomIdx < lastAtomIdx; atomIdx++) {
                unsigned int atomTypeIdx = mol->AtomTypeIndices()[atomIdx];
                if (mol->AtomTypes()[atomTypeIdx].Name().Equals("CA")) {
                    this->positionsCa[molIdx].Add(mol->AtomPositions()[3 * atomIdx]);
                    this->positionsCa[molIdx].Add(mol->AtomPositions()[3 * atomIdx + 1]);
                    this->positionsCa[molIdx].Add(mol->AtomPositions()[3 * atomIdx + 2]);
                    this->positionsCa[molIdx].Add(1.0f);
                    // write first and last Ca position three times
                    if ((resIdx == firstResIdx) || (resIdx == (lastResIdx - 1))) {
                        this->positionsCa[molIdx].Add(mol->AtomPositions()[3 * atomIdx]);
                        this->positionsCa[molIdx].Add(mol->AtomPositions()[3 * atomIdx + 1]);
                        this->positionsCa[molIdx].Add(mol->AtomPositions()[3 * atomIdx + 2]);
                        this->positionsCa[molIdx].Add(1.0f);
                        this->positionsCa[molIdx].Add(mol->AtomPositions()[3 * atomIdx]);
                        this->positionsCa[molIdx].Add(mol->AtomPositions()[3 * atomIdx + 1]);
                        this->positionsCa[molIdx].Add(mol->AtomPositions()[3 * atomIdx + 2]);
                        this->positionsCa[molIdx].Add(1.0f);
                    }
                }
                if (mol->AtomTypes()[atomTypeIdx].Name().Equals("O")) {
                    this->positionsO[molIdx].Add(mol->AtomPositions()[3 * atomIdx]);
                    this->positionsO[molIdx].Add(mol->AtomPositions()[3 * atomIdx + 1]);
                    this->positionsO[molIdx].Add(mol->AtomPositions()[3 * atomIdx + 2]);
                    this->positionsO[molIdx].Add(1.0f);
                    // write first and last Ca position three times
                    if ((resIdx == firstResIdx) || (resIdx == (lastResIdx - 1))) {
                        this->positionsO[molIdx].Add(mol->AtomPositions()[3 * atomIdx]);
                        this->positionsO[molIdx].Add(mol->AtomPositions()[3 * atomIdx + 1]);
                        this->positionsO[molIdx].Add(mol->AtomPositions()[3 * atomIdx + 2]);
                        this->positionsO[molIdx].Add(1.0f);
                        this->positionsO[molIdx].Add(mol->AtomPositions()[3 * atomIdx]);
                        this->positionsO[molIdx].Add(mol->AtomPositions()[3 * atomIdx + 1]);
                        this->positionsO[molIdx].Add(mol->AtomPositions()[3 * atomIdx + 2]);
                        this->positionsO[molIdx].Add(1.0f);
                    }
                }
            }
        }
    }
    // std::cout << "cIndex " << cIndex << " oIndex " << oIndex << " molCount " << mol->MoleculeCount() << std::endl;

    if (lineParam.Param<param::BoolParam>()->Value()) {
        // currBuf = 0;
        for (unsigned int i = 0; i < this->positionsCa.Count(); i++) {
            unsigned int colBytes, vertBytes, colStride, vertStride;
            this->getBytesAndStrideLines(*mol, colBytes, vertBytes, colStride, vertStride);

            lineShader_->use();

            lineShader_->setUniform("viewAttr", viewportStuff);
            lineShader_->setUniform("clipDat", clipDat);
            lineShader_->setUniform("clipCol", clipCol);
            lineShader_->setUniform("MVinv", viewInv);
            lineShader_->setUniform("MVP", mvp);
            lineShader_->setUniform("MVPinv", mvpInv);
            lineShader_->setUniform("MVPtransp", mvpTrans);
            lineShader_->setUniform("scaling", this->scalingParam.Param<param::FloatParam>()->Value());
            float minC = 0.0f, maxC = 0.0f;
            unsigned int colTabSize = 0;
            lineShader_->setUniform("inConsts1", -1.0f, minC, maxC, float(colTabSize));

            UINT64 numVerts, vertCounter;
            numVerts = this->bufSize / vertStride;
            const char* currVert = (const char*) (this->positionsCa[i].PeekElements());
            const char* currCol = 0;
            vertCounter = 0;
            while (vertCounter < this->positionsCa[i].Count() / 4) {
                void* mem = static_cast<char*>(this->theSingleMappedMem) + bufSize * currBuf;
                const char* whence = currVert;
                UINT64 vertsThisTime = vislib::math::Min(this->positionsCa[i].Count() / 4 - vertCounter, numVerts);
                this->waitSignal(fences[currBuf]);
                memcpy(mem, whence, (size_t) vertsThisTime * vertStride);
                glFlushMappedNamedBufferRangeEXT(
                    theSingleBuffer, bufSize * currBuf, (GLsizeiptr) vertsThisTime * vertStride);
                lineShader_->setUniform("instanceOffset", 0);

                glBindBufferRange(
                    GL_SHADER_STORAGE_BUFFER, SSBObindingPoint, this->theSingleBuffer, bufSize * currBuf, bufSize);
                glPatchParameteri(GL_PATCH_VERTICES, 1);
                glDrawArrays(GL_PATCHES, 0, (GLsizei) vertsThisTime - 3);
                this->queueSignal(fences[currBuf]);

                currBuf = (currBuf + 1) % this->numBuffers;
                vertCounter += vertsThisTime;
                currVert += vertsThisTime * vertStride;
                currCol += vertsThisTime * colStride;
            }

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
            glDisable(GL_TEXTURE_1D);
            glUseProgram(0);
        }
    }

    if (this->lineDebugParam.Param<param::BoolParam>()->Value())
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    if (backboneParam.Param<param::BoolParam>()->Value()) {
        // currBuf = 0;
        unsigned int colBytes, vertBytes, colStride, vertStride;
        this->getBytesAndStride(*mol, colBytes, vertBytes, colStride, vertStride);

        // lighting setup
        std::array<float, 3> lightPos = {0.0f, 0.0f, 0.0f};

        auto call_light = getLightsSlot.CallAs<core::view::light::CallLight>();
        if (call_light != nullptr) {
            if (!(*call_light)(0)) {
                return false;
            }

            auto lights = call_light->getData();
            auto point_lights = lights.get<core::view::light::PointLightType>();

            if (point_lights.size() > 1) {
                megamol::core::utility::log::Log::DefaultLog.WriteWarn(
                    "[CartoonTessellationRenderer] Only one single 'Point Light' source is supported by this renderer");
            } else if (point_lights.empty()) {
                megamol::core::utility::log::Log::DefaultLog.WriteWarn(
                    "[CartoonTessellationRenderer] No 'Point Light' found");
            }

            for (auto const& light : point_lights) {
                // light.second.lightColor;
                // light.second.lightIntensity;
                lightPos[0] = light.position[0];
                lightPos[1] = light.position[1];
                lightPos[2] = light.position[2];
                break;
            }
        }

        cartoonShader_->use();
        cartoonShader_->setUniform("viewAttr", viewportStuff);
        cartoonShader_->setUniform("clipDat", clipDat);
        cartoonShader_->setUniform("MV", view);
        cartoonShader_->setUniform("MVinv", viewInv);
        cartoonShader_->setUniform("MVP", mvp);
        cartoonShader_->setUniform("MVPinv", mvpInv);
        cartoonShader_->setUniform("MVPtransp", mvpTrans);
        cartoonShader_->setUniform("MVinvtrans", mvpInvTrans);
        cartoonShader_->setUniform("ProjInv", projInv);
        cartoonShader_->setUniform("scaling", this->scalingParam.Param<param::FloatParam>()->Value());
        cartoonShader_->setUniform("pipeWidth", this->backboneWidthParam.Param<param::FloatParam>()->Value());
        cartoonShader_->setUniform(
            "interpolateColors", this->colorInterpolationParam.Param<param::BoolParam>()->Value());

        float minC = 0.0f, maxC = 0.0f;
        unsigned int colTabSize = 0;
        cartoonShader_->setUniform("inConsts1", -1.0f, minC, maxC, float(colTabSize));
        cartoonShader_->setUniform("lightPos", lightPos[0], lightPos[1], lightPos[2], 1.0f);

        UINT64 numVerts;
        numVerts = this->bufSize / vertStride;
        UINT64 stride = 0;

        for (int i = 0; i < (int) molSizes.size(); i++) {
            UINT64 vertCounter = 0;
            while (vertCounter < molSizes[i]) {
                const char* currVert = (const char*) (&mainchain[(unsigned int) vertCounter + (unsigned int) stride]);
                void* mem = static_cast<char*>(this->theSingleMappedMem) + bufSize * currBuf;
                const char* whence = currVert;
                UINT64 vertsThisTime = vislib::math::Min(molSizes[i] - vertCounter, numVerts);
                this->waitSignal(fences[currBuf]);
                memcpy(mem, whence, (size_t) vertsThisTime * vertStride);
                glFlushMappedNamedBufferRangeEXT(
                    theSingleBuffer, bufSize * currBuf, (GLsizeiptr) vertsThisTime * vertStride);
                cartoonShader_->setUniform("instanceOffset", 0);

                glBindBufferRange(
                    GL_SHADER_STORAGE_BUFFER, SSBObindingPoint, this->theSingleBuffer, bufSize * currBuf, bufSize);
                glPatchParameteri(GL_PATCH_VERTICES, 1);
                glDrawArrays(GL_PATCHES, 0, (GLsizei) (vertsThisTime - 3));
                this->queueSignal(fences[currBuf]);

                currBuf = (currBuf + 1) % this->numBuffers;
                vertCounter += vertsThisTime;
                currVert += vertsThisTime * vertStride;
            }
            stride += molSizes[i];
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        glDisable(GL_TEXTURE_1D);
        glUseProgram(0);
    }

    mol->Unlock();

    glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);

    //	timer.EndFrame();

    return true;
}
