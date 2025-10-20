#include "Renderer/SceneRenderer.h"
#include <algorithm>
#include <cmath>
#include <functional>

using Microsoft::WRL::ComPtr;

/*
    SceneRenderer::Record
    ----------------------------------------------------------------------------
    �����F
      - �^����ꂽ RenderTarget �ɑ΂��ăV�[���S�̂�`�悷��B
      - �e�I�u�W�F�N�g�̒萔�o�b�t�@( b0 )���t���[���p�A�b�v���[�h�o�b�t�@�ɏ������݁A
        ���[�g CBV (slot=0) ��s�x�����ւ��Ȃ��� Draw ��ςށB

    ���O�����F
      - rt.Color() ���L���iRT���쐬�ς݁j
      - cmd / m_frames�iFrameResources�j���L��
      - m_pipe.root�iRootSignature�j�� Initialize ���ɐݒ�ς�

    �萔�o�b�t�@�̃X���b�g���蓖�āF
      - cbBase �c�c �Ăяo���������̕`��p�X�Ɋ��蓖�Ă� �g�J�n�C���f�b�N�X�h
      - slot   �c�c ���̊֐����� 0..(maxObjects-1) ������Adst = cbBase + slot �ɏ���
      - ���X���b�g���� FrameResources ���������� maxObjects �ƈ�v�����邱��
*/
void SceneRenderer::Record(ID3D12GraphicsCommandList* cmd,
    RenderTarget& rt,
    const CameraMatrices& cam,
    const Scene* scene,
    UINT cbBase,
    UINT frameIndex,
    UINT maxObjects)
{
    // --- �h��F�Œ���̈ˑ��֌W�������ꍇ�͉������Ȃ� ---
    if (!rt.Color() || !cmd || !m_frames) return;

    // ==============================
    // 1) �o�̓^�[�Q�b�g�̏����iRTV/�N���A/�r���[�|�[�g�j
    // ==============================

    // �K�v�Ȃ� RT ��Ԃ֑J�ځi�����řp���`�F�b�N�j
    rt.TransitionToRT(cmd);

    // OM �� RTV/DSV ���o�C���h�i���̊֐��ł� DSV ���g��Ȃ��\���j
    rt.Bind(cmd);

    // �N���A�i�w�i�F/�[�x�j
    rt.Clear(cmd);

    // �r���[�|�[�g/�V�U�[�� RT �T�C�Y�S�ʂ�
    {
        D3D12_VIEWPORT vp{ 0.f, 0.f, (float)rt.Width(), (float)rt.Height(), 0.f, 1.f };
        D3D12_RECT     sc{ 0, 0, (LONG)rt.Width(), (LONG)rt.Height() };
        cmd->RSSetViewports(1, &vp);
        cmd->RSSetScissorRects(1, &sc);
    }

    // �g�|���W/���[�g�V�O�l�`���iPSO �͌Ăяo�����ŃZ�b�g�ςݑO��j
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->SetGraphicsRootSignature(m_pipe.root.Get());

    // ==============================
    // 2) �V�[���𑖍����ĕ`��i�[����/���������̏����͂����ł͖��l���j
    // ==============================
    if (scene)
    {
        // ���t���[���̃A�b�v���[�h�̈���擾
        auto& fr = m_frames->Get(frameIndex);
        UINT8* cbCPU = fr.cpu;                            // CPU�������ݐ�iMap�ρj
        D3D12_GPU_VIRTUAL_ADDRESS cbGPU = fr.resource->GetGPUVirtualAddress(); // GPU���x�[�X
        const UINT cbStride = m_frames->GetCBStride();    // 256B �A���C���ς݃T�C�Y
        UINT slot = 0;                                    // ���̕`��p�X�ŏ���郍�[�J���X���b�g

        using namespace DirectX;

        // ---- �ċA�����_�F�V�[���O���t�������� MeshRenderer ������Ε`�� ----
        std::function<void(std::shared_ptr<GameObject>)> draw =
            [&](std::shared_ptr<GameObject> go)
            {
                if (!go || slot >= maxObjects) return; // �X���b�g����ő����I��

                if (auto mr = go->GetComponent<MeshRendererComponent>())
                {
                    // VB/IB ���L���ŁA�`��C���f�b�N�X�������Ȃ�`��
                    if (mr->VertexBuffer && mr->IndexBuffer && mr->IndexCount > 0)
                    {
                        // 2.1) �s��v�Z�FM, MVP, (M^-1)^T
                        XMMATRIX world = go->Transform->GetWorldMatrix();
                        XMMATRIX mvp = world * cam.view * cam.proj;

                        // �t�s��̌��S���`�F�b�N�i�k�ނ� NaN/Inf �ɂȂ肤��j
                        XMVECTOR det;
                        XMMATRIX inv = XMMatrixInverse(&det, world);
                        float detScalar = XMVectorGetX(det);
                        if (!std::isfinite(detScalar) || std::fabs(detScalar) < 1e-8f) {
                            // �ɒ[�ȃX�P�[��/�k�� �� �@��������̂Ńt�H�[���o�b�N
                            inv = XMMatrixIdentity();
                        }
                        XMMATRIX worldIT = XMMatrixTranspose(inv);

                        // 2.2) �萔�o�b�t�@��g�ݗ��Ă� Upload
                        SceneConstantBuffer cb{};
                        XMStoreFloat4x4(&cb.mvp, mvp);
                        XMStoreFloat4x4(&cb.world, world);
                        XMStoreFloat4x4(&cb.worldIT, worldIT);
                        // �ȈՃ��C�g�i��O������̕��s�����j
                        XMStoreFloat3(&cb.lightDir,
                            XMVector3Normalize(XMVectorSet(0.0f, -1.0f, -1.0f, 0.0f)));
                        cb.pad = 0.0f;

                        // 2.3) ���̃I�u�W�F�N�g�� CBV �X���b�g�icbBase �N�_�j
                        const UINT dst = cbBase + slot;

                        // CPU ���A�b�v���[�h�������փR�s�[�i256B �A���C�������j
                        std::memcpy(cbCPU + (UINT64)dst * cbStride, &cb, sizeof(cb));

                        // ���[�g CBV �������ւ��ib0�j
                        cmd->SetGraphicsRootConstantBufferView(
                            0, cbGPU + (UINT64)dst * cbStride);

                        // 2.4) �W�I���g�����o�C���h���� Draw
                        cmd->IASetVertexBuffers(0, 1, &mr->VertexBufferView);
                        cmd->IASetIndexBuffer(&mr->IndexBufferView);
                        cmd->DrawIndexedInstanced(mr->IndexCount, 1, 0, 0, 0);

                        ++slot; // ���I�u�W�F�N�g��
                    }
                }

                // �q�m�[�h���ċA����
                for (auto& ch : go->GetChildren()) draw(ch);
            };

        // ���[�g���瑖���J�n�i�������̓V�[���f�[�^�ˑ��B�K�v�ɉ����ă\�[�g��ǉ��j
        for (auto& root : scene->GetRootGameObjects()) draw(root);
    }

    // ==============================
    // 3) �o�͂� SRV ��ԂցiUI �����T���v���ł���悤�Ɂj
    // ==============================
    rt.TransitionToSRV(cmd);
}

/*
�y������̃��� / ���Ƃ����z
- FrameResources �� cbStride �� 256B �A���C���K�{�iD3D12 �萔�o�b�t�@�K��j�B
  Initialize(cbSize) �� (cbSize+255)&~255 �ɂȂ��Ă��邱�Ƃ�v�m�F�B
- cbBase/slot �� maxObjects�F
  * cbBase �� �g���̕`��p�X�̐擪�h ���Ăяo�������Ǘ��i��FScene=0..N-1, Game=N..2N-1�j�B
  * slot �͂��̊֐��̃��[�J���BmaxObjects �𒴂�������S�ɑł��؂�B
- PSO/RS/RootSig�F
  * �{�֐��ł� RootSignature �݂̂��Z�b�g�BPSO �Z�b�g�͌Ăяo�����̐Ӗ��B
  * �g�� InputLayout/�V�F�[�_�� VB/IB �� stride/format ����v���Ă��邱�ƁB
- Transform �̋t�s��F
  * �ɒ[�ȃX�P�[���i0�ɋ߂��j��񐳑��s�񂾂� inv �� NaN ���o��B
    �� det ���`�F�b�N���A���Ă����� Identity �փt�H�[���o�b�N�i�@���������̂�����j�B
- �[�x�e�X�g�F
  * ����� DSV �� Bind ���Ă��Ȃ��B�[�x���g���`��ɂ���Ȃ� RenderTarget ����
    DSV ���������ABind() �� RTV+DSV ��ݒ肷�� or �Ăяo�����œK�؂ɐݒ肷��B
*/
