// Renderer/Viewports.cpp
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Renderer/Viewports.h"

#include <d3d12.h>
#include <cmath>
#include <algorithm>
#include <cstdlib> // std::abs(int)
#include "Renderer/SceneRenderer.h"
#include "Components/CameraComponent.h"
#include "Scene/Scene.h"
#include "Editor/ImGuiLayer.h"
#include "Editor/EditorContext.h"

using namespace DirectX;

/*
    Viewports
    ----------------------------------------------------------------------------
    �����F
      - Scene �p / Game �p�̃I�t�X�N���[�� RenderTarget ���Ǘ��i����/�Đ���/����ڏ��j
      - ImGui �֕\�����邽�߂� SRV (ImTextureID) ������
      - UI ���́u��]�T�C�Y�v���q�X�e���V�X�t���Ŏ󂯎��A������ RT ���č쐬
      - Scene �� Game �̓��e�E�r���[�̏��������iGame �͌Œ�J�����AScene �͓s�x�j

    �d�v�ȃ|���V�[�F
      - ���T�C�Y�́u�����v�ł͂Ȃ��u���莞�Ԃ𒴂�����m��v(�f�o�E���X�j
      - �č쐬���A�Â� RT �� Detach ���ČĂяo������ GpuGarbageQueue �ցi�x���j���j
      - SRV �� ImGuiLayer ���̃q�[�v�ɍ��iDX12 �� ImTextureID = GPU��SRV�n���h���j
      - Scene �� �g�� FOV �Œ�h �ŃA�X�y�N�g�Ǐ]�iMakeProjConstHFov�j

    �悭���闎�Ƃ����F
      - ImGui �̕\���̈悪�����ɗh��� �� ���t���� RT �Đ��� �� �j���a��/�J�N��
        �� StepSnap + ���莞��臒l�Ŋɘa
      - Detach �����Â� RT �𑦔j�����Ă��܂� GPU �g�p���N���b�V��
        �� �K�� GpuGarbageQueue �� fence �l�ƂƂ��ɓo�^�i�Ăяo�����̐Ӗ��j
*/

// =============================================================================
// Game �p�F���e�č\�z�w���p�[�i�gGame �̊���e�h����Čv�Z����j
//   - ����: ����FOV���iConst H-FOV�j�� newAspect �ɍ��킹�ďcFOV�𒲐�
//   - ����: �������iConst Width�j�� newAspect �ɍ��킹�č����𒲐�
// =============================================================================

// ����/�����̊ȈՔ���iD3D �W���`�F������ _34 �� 0�A������ 0�j
static inline bool VP_IsPerspective(const XMFLOAT4X4& M) {
    return std::fabs(M._34) > 1e-6f;
}

// �����F����FOV���� newAspect �ɍ��킹�čč\�z
static inline XMMATRIX VP_RebuildPerspConstHFov(const XMFLOAT4X4& base, float newAspect)
{
    // near/far ���o�i����n�����j
    const float A = base._33;  // far/(far - near)
    const float B = base._43;  // -near*far/(far - near)
    const float nearZ = -B / A;
    const float farZ = (A * nearZ) / (A - 1.0f);

    // tan(hFov/2) = 1 / m00,  tan(vFov/2) = tan(hFov/2) / aspect
    const float tanHalfH = 1.0f / base._11;
    const float tanHalfV = tanHalfH / std::max(newAspect, 1e-6f);
    const float vFovNew = 2.0f * std::atan(tanHalfV);

    return XMMatrixPerspectiveFovLH(vFovNew, newAspect, nearZ, farZ);
}

// �����F�������� newAspect �ɍ��킹�ďc�����č\�z
static inline XMMATRIX VP_RebuildOrthoConstWidth(const XMFLOAT4X4& base, float newAspect)
{
    // �����̕W���`�Fm00 = 2/width, m11 = 2/height
    const float width = 2.0f / base._11;
    const float height0 = 2.0f / base._22;

    const float height = (newAspect > 1e-6f) ? (width / newAspect) : height0;

    // �߉��Fm22 = 1/(far-near), m43 = -near/(far-near)
    const float m22 = base._33;
    const float m43 = base._43;
    const float nearZ = -m43 / m22;
    const float farZ = nearZ + 1.0f / m22;

    const float l = -width * 0.5f;
    const float r = width * 0.5f;
    const float b = -height * 0.5f;
    const float t = height * 0.5f;

    return XMMatrixOrthographicOffCenterLH(l, r, b, t, nearZ, farZ);
}

// ----------------------------------------------------------------------------
// �������FScene/Game �� RT ���E�B���h�E�����T�C�Y�ō쐬
//   �������\���傫�߂ɍ���Ă����ƁA�g�厞�̈ꎞ�I�Ȃڂ₯������₷��
// ----------------------------------------------------------------------------
void Viewports::Initialize(ID3D12Device* dev, unsigned w, unsigned h)
{
    // Scene
    {
        RenderTargetDesc s{};
        s.width = w; s.height = h;
        s.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        s.depthFormat = DXGI_FORMAT_D32_FLOAT;
        s.clearColor[0] = 0.10f; s.clearColor[1] = 0.10f; s.clearColor[2] = 0.10f; s.clearColor[3] = 1.0f;
        s.clearDepth = 1.0f;
        m_scene.Create(dev, s);
    }
    // Game
    {
        RenderTargetDesc g{};
        g.width = w; g.height = h;
        g.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        g.depthFormat = DXGI_FORMAT_D32_FLOAT;
        g.clearColor[0] = 0.12f; g.clearColor[1] = 0.12f; g.clearColor[2] = 0.12f; g.clearColor[3] = 1.0f;
        g.clearDepth = 1.0f;
        m_game.Create(dev, g);
    }

    // Scene ����̓J�����̓��e���u�L���v�`���O�v
    m_sceneProjCaptured = false;

    // Game �́uScene �Ɠ������܂��v���
    m_gameFrozen = false;

    // ���T�C�Y�f�o�E���X�̓�����Ԃ����Z�b�g
    m_desiredW = m_desiredH = 0;
    m_desiredStableTime = 0.0f;
    m_pendingW = m_pendingH = 0;
}

// ----------------------------------------------------------------------------
// ImGui �֓n�����i�e�N�X�`��ID�Ǝ�RT�T�C�Y�j�� EditorContext �ɓ]�L
//   - sceneSrvBase/gameSrvBase + frameIndex �� SRV �X���b�g�Փ˂����
// ----------------------------------------------------------------------------
void Viewports::FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
    unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase)
{
    const unsigned sceneSlot = sceneSrvBase + frameIndex;
    const unsigned gameSlot = gameSrvBase + frameIndex;

    // Scene
    ctx.sceneTexId = m_scene.EnsureImGuiSRV(imgui, sceneSlot);
    ctx.sceneRTWidth = m_scene.Width();
    ctx.sceneRTHeight = m_scene.Height();

    // Game
    ctx.gameTexId = m_game.EnsureImGuiSRV(imgui, gameSlot);
    ctx.gameRTWidth = m_game.Width();
    ctx.gameRTHeight = m_game.Height();
}

// ----------------------------------------------------------------------------
// UI ������n�����uScene �E�B���h�E�̗��p�\�T�C�Y�v���󂯎��A
//  - StepSnap�i�����ݕω��}���j
//  - �������i�e�N�X�`���݊��E�s�N�Z������}���j
//  - ���莞�Ԃ����𒴂����� pending �Ɋm��
// ----------------------------------------------------------------------------
void Viewports::RequestSceneResize(unsigned w, unsigned h, float dt)
{
    if (w == 0 || h == 0) return;

    // 1) �X�e�b�v�X�i�b�v�i�h���b�O���̔����ω��ōĐ������Ȃ��j
    constexpr unsigned kStep = 8;
    auto StepSnap = [](unsigned v, unsigned step) {
        if (step == 0) return v;
        return (unsigned)(((v + step / 2) / step) * step);
        };
    auto Even = [](unsigned v) { return (v + 1u) & ~1u; }; // ������

    unsigned wantW = Even(StepSnap(w, kStep));
    unsigned wantH = Even(StepSnap(h, kStep));

    // 2) �ڕW���ς��������莞�Ԃ����Z�b�g
    if (wantW != m_desiredW || wantH != m_desiredH) {
        m_desiredW = wantW;
        m_desiredH = wantH;
        m_desiredStableTime = 0.0f;
        return;
    }

    // 3) �����ڕW���p���������Ԃ�ώZ
    if (dt > 0.0f) m_desiredStableTime += dt;

    // 4) ��莞�ԃu���Ȃ������� pending �Ɋm��
    constexpr float kStableSec = 0.12f; // 0.08�`0.20 �Œ�����
    if (m_desiredStableTime >= kStableSec) {
        // ���ɓ��T�C�Y�Ȃ� noop
        if (m_desiredW != m_scene.Width() || m_desiredH != m_scene.Height()) {
            m_pendingW = m_desiredW;
            m_pendingH = m_desiredH;
        }
        // ����ɔ����ă��Z�b�g
        m_desiredW = m_desiredH = 0;
        m_desiredStableTime = 0.0f;
    }
}

// ----------------------------------------------------------------------------
// �y���f�B���O���ꂽ���T�C�Y������ΓK�p
//   - �V���� RT ���쐬
//   - �Â� RT �� Detach ���� 1 �����߂��i2 �ڂ� m_carryOverDead �Ƀv�[���j
//   - �Ăяo�����iRenderer�j�͖߂�l�����t���[���� EndFrame �Œx���j���o�^
//   - ��Game �̌����iView�j�͕ێ����A���e�iProj�j�̂݁gGame �̊�h����č\�z��
// ----------------------------------------------------------------------------
RenderTargetHandles Viewports::ApplyPendingResizeIfNeeded(ID3D12Device* dev)
{
    RenderTargetHandles toDispose{}; // ���t���[���� EndFrame �ɓn�����i�ő�1�j
    if (m_pendingW == 0 || m_pendingH == 0) return toDispose;

    const bool needScene = (m_pendingW != m_scene.Width() || m_pendingH != m_scene.Height());
    const bool needGame = (m_pendingW != m_game.Width() || m_pendingH != m_game.Height());

    // ����K�� Detach�i�� Release �͂��Ȃ��j��GPU �Ŏg���Ă���\�������邽��
    if (needScene) {
        RenderTargetHandles old = m_scene.Detach();
        // 1�ڂ͍��t���[���ցA2�ڂ͎����z��
        if (!toDispose.color && !toDispose.depth && !toDispose.rtvHeap && !toDispose.dsvHeap) {
            toDispose = std::move(old);
        }
        else if (!m_carryOverDead.color && !m_carryOverDead.depth
            && !m_carryOverDead.rtvHeap && !m_carryOverDead.dsvHeap) {
            m_carryOverDead = std::move(old);
        }
    }
    if (needGame) {
        RenderTargetHandles old = m_game.Detach();
        if (!toDispose.color && !toDispose.depth && !toDispose.rtvHeap && !toDispose.dsvHeap) {
            toDispose = std::move(old);
        }
        else if (!m_carryOverDead.color && !m_carryOverDead.depth
            && !m_carryOverDead.rtvHeap && !m_carryOverDead.dsvHeap) {
            m_carryOverDead = std::move(old);
        }
    }

    // �V�K�쐬�iScene / Game �Ƃ��j
    if (needScene) {
        RenderTargetDesc s{};
        s.width = m_pendingW; s.height = m_pendingH;
        s.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        s.depthFormat = DXGI_FORMAT_D32_FLOAT;
        s.clearColor[0] = 0.10f; s.clearColor[1] = 0.10f; s.clearColor[2] = 0.10f; s.clearColor[3] = 1.0f;
        s.clearDepth = 1.0f;
        m_scene.Create(dev, s);
        m_sceneProjCaptured = false; // ���e�����蒼���i�A�X�y�N�g�ω��ɑΉ��j
    }
    if (needGame) {
        RenderTargetDesc g{};
        g.width = m_pendingW; g.height = m_pendingH;
        g.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        g.depthFormat = DXGI_FORMAT_D32_FLOAT;
        g.clearColor[0] = 0.12f; g.clearColor[1] = 0.12f; g.clearColor[2] = 0.12f; g.clearColor[3] = 1.0f;
        g.clearDepth = 1.0f;
        m_game.Create(dev, g);

        // ������(View)�͈ێ��B���e(Proj)�̂݁gGame �̊���e�h����č\�z����B
        if (m_game.Width() > 0 && m_game.Height() > 0) {
            const float gaspect = float(m_game.Width()) / float(m_game.Height());

            // �č\�z�̊�ƂȂ铊�e�s��
            XMFLOAT4X4 base{};
            if (m_gameFrozen) {
                // ���ɏ��񓯊��ς� �� Game ���g�̊���e���g���i���d�v�FScene �Ɉ�������Ȃ��j
                base = m_gameProjInit;
            }
            else if (m_sceneProjCaptured) {
                // �܂������O�Ȃ�b��I�� Scene �̓��e���g���i����̂݁j
                base = m_sceneProjInit;
            }
            else {
                // �ŏI�t�H�[���o�b�N�l�i60�x, 0.1-1000�j
                XMStoreFloat4x4(&base,
                    XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), gaspect, 0.1f, 1000.0f));
            }

            XMMATRIX P = VP_IsPerspective(base)
                ? VP_RebuildPerspConstHFov(base, gaspect)
                : VP_RebuildOrthoConstWidth(base, gaspect);
            XMStoreFloat4x4(&m_gameProjInit, P);
        }
        // m_gameFrozen �͐G��Ȃ��i= View �͌Œ�̂܂܁j
    }

    // ����ς�
    m_pendingW = m_pendingH = 0;
    return toDispose; // �� EndFrame �ɓn���͍̂ő�1�i2�ڂ͓��������z���j
}

// ----------------------------------------------------------------------------
// Scene �`��i�� FOV �Œ�ŃA�X�y�N�g�Ǐ]�j
//   - ����ɃJ�����̓��e���L���v�`���i��j
//   - ���݂� RT �A�X�y�N�g�ɍ��킹�ďc FOV ���Čv�Z
//   - �`���AGame �̏��񓯊��i�Œ�J�����̎d���݁j
// ----------------------------------------------------------------------------
void Viewports::RenderScene(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
    const CameraComponent* cam, const Scene* scene,
    unsigned frameIndex, unsigned maxObjects)
{
    if (!m_scene.Color() || !cam) return;

    // ���e�̊�i����̂݃L���v�`���j
    if (!m_sceneProjCaptured) {
        XMStoreFloat4x4(&m_sceneProjInit, cam->GetProjectionMatrix());
        m_sceneProjCaptured = true;
    }

    // ���݂̃A�X�y�N�g
    const float aspect = (m_scene.Height() > 0)
        ? float(m_scene.Width()) / float(m_scene.Height())
        : 1.0f;

    // ��FOV�Œ�ŏcFOV���Čv�Z�������e�s��
    const XMMATRIX proj = MakeProjConstHFov(XMLoadFloat4x4(&m_sceneProjInit), aspect);
    CameraMatrices C{ cam->GetViewMatrix(), proj };

    // Scene �������_�����O�icbBase=0..maxObjects-1�j
    sr.Record(cmd, m_scene, C, scene, /*cbBase=*/0, frameIndex, maxObjects);

    // --- Game �̏��񓯊��i1�񂾂��j ---
    if (!m_gameFrozen && m_game.Width() > 0 && m_game.Height() > 0) {
        const float gaspect = float(m_game.Width()) / float(m_game.Height());
        const XMMATRIX gproj = MakeProjConstHFov(XMLoadFloat4x4(&m_sceneProjInit), gaspect);
        XMStoreFloat4x4(&m_gameViewInit, cam->GetViewMatrix());
        XMStoreFloat4x4(&m_gameProjInit, gproj);
        m_gameFrozen = true; // �ȍ~ Game �͌Œ�J�����ŕ`���iView �͌Œ�AProj �̓A�X�y�N�g�Ǐ]�j
    }
}

// ----------------------------------------------------------------------------
// Game �`��i�Œ�J�����F�ŏ��� Scene �Ɠ������� View/Proj ���g�p�j
//   - cbBase=maxObjects..(2*maxObjects-1) ���g���O��� SceneRenderer �ɓn��
// ----------------------------------------------------------------------------
void Viewports::RenderGame(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
    const Scene* scene, unsigned frameIndex, unsigned maxObjects)
{
    if (!m_gameFrozen || !m_game.Color()) return;

    CameraMatrices C{
        XMLoadFloat4x4(&m_gameViewInit),
        XMLoadFloat4x4(&m_gameProjInit)
    };
    sr.Record(cmd, m_game, C, scene, /*cbBase=*/maxObjects, frameIndex, maxObjects);
}

// ----------------------------------------------------------------------------
// MakeProjConstHFov
// �ړI�F���̓��e P0 ���� near/far �� �g��FOV�h ��ۂ����܂܁A�V�����A�X�y�N�g newAspect ��
//       �cFOV ���Čv�Z�����������e�s���Ԃ��B
// ���w�Fm00 = 1 / (tan(vFov/2) * aspect) �� 1/m00 = tan(vFov/2) * aspect = tan(hFov/2)
//       ����� tan(hFov/2) = 1/m00�BnewAspect ���ς������ tan(vFov/2)=tan(hFov/2)/newAspect�B
// ----------------------------------------------------------------------------
XMMATRIX Viewports::MakeProjConstHFov(XMMATRIX P0, float newAspect)
{
    XMFLOAT4X4 M; XMStoreFloat4x4(&M, P0);

    // �߉������s�񂩂璊�o�i����n�������e�̈�ʌ`�j
    const float A = M._33;  // far/(far - near)
    const float B = M._43;  // -near*far/(far - near)
    const float nearZ = -B / A;
    const float farZ = (A * nearZ) / (A - 1.0f);

    // �� FOV ��ς����ɏc FOV ���Čv�Z
    const float tanHalfH = 1.0f / M._11;         // = tan(hFov/2)
    const float tanHalfV = tanHalfH / newAspect; // = tan(vFov/2)
    const float vFovNew = 2.0f * std::atan(tanHalfV);

    return XMMatrixPerspectiveFovLH(vFovNew, newAspect, nearZ, farZ);
}

// ----------------------------------------------------------------------------
// �A�N�Z�X�n�FRT �����݂��Ȃ��ꍇ�� 0 ��Ԃ��i���S���j
// ----------------------------------------------------------------------------
unsigned Viewports::SceneWidth()  const noexcept { return m_scene.Color() ? m_scene.Width() : 0; }
unsigned Viewports::SceneHeight() const noexcept { return m_scene.Color() ? m_scene.Height() : 0; }
unsigned Viewports::GameWidth()   const noexcept { return m_game.Color() ? m_game.Width() : 0; }
unsigned Viewports::GameHeight()  const noexcept { return m_game.Color() ? m_game.Height() : 0; }
