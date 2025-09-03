#pragma once // �����w�b�_�[�t�@�C����������C���N���[�h�����̂�h���v���v���Z�b�T�f�B���N�e�B�u

#include <vector>        // std::vector���g�p
#include <DirectXMath.h> // DirectX::XMFLOAT3, DirectX::XMFLOAT4 �Ȃǂ��g�p
#include <string>        // (����͒��ڎg���Ă��Ȃ����A���̕����Ŏg���\�����l��)
#include <wrl/client.h>  // Microsoft::WRL::ComPtr ���g�p�iCOM�I�u�W�F�N�g�̃X�}�[�g�|�C���^�j

// DirectX 12 �̌^��`�̂��߂ɒǉ�
#include <d3d12.h>       // Direct3D 12 API�̃R�A�w�b�_�[
#include "d3dx12.h"      // D3D12�I�u�W�F�N�g�̍쐬���ȑf�����郆�[�e�B���e�B�֐����

// Component �N���X����`����Ă��� GameObject.h ���C���N���[�h
#include "GameObject.h"  // Component���N���X��GameObject�N���X�̒�`
#include "Component.h"
#include "TransformComponent.h"

// �O���錾:
// D3D12Renderer �N���X�� MeshRendererComponent �̃v���C�x�[�g�����o�[�ɃA�N�Z�X�ł���悤�ɁA
// �t�����h�N���X�Ƃ��Đ錾���邽�߂ɕK�v
class D3D12Renderer;

// --- ���_�\���� ---
// �V�F�[�_�[�̓��̓��C�A�E�g�Ɠ���������K�v������
struct Vertex
{
    DirectX::XMFLOAT3 Position; // ���_��3D��ԍ��W (x, y, z)
    DirectX::XMFLOAT4 Color;    // ���_�̐F (r, g, b, a)
    // DirectX::XMFLOAT2 TexCoord; // �e�N�X�`���}�b�s���O����������ۂɕK�v�ƂȂ�e�N�X�`�����W
    // DirectX::XMFLOAT3 Normal;   // ���C�e�B���O����������ۂɕK�v�ƂȂ�@���x�N�g��
};

// --- ���b�V���f�[�^�\���� ---
// CPU���Ń��b�V���̒��_�f�[�^�ƃC���f�b�N�X�f�[�^��ێ����邽�߂̍\����
struct MeshData
{
    std::vector<Vertex> Vertices;       // ���_�f�[�^�̃��X�g
    std::vector<unsigned int> Indices;  // �C���f�b�N�X�f�[�^�̃��X�g
    // unsigned short ���� unsigned int �ɕύX���邱�ƂŁA
    // 65535�ȏ�̑�ʂ̒��_�������b�V���ɂ��Ή��\�ɂȂ�
};

// --- MeshRendererComponent�N���X ---
// GameObject�Ƀ��b�V����`�悳���邽�߂̃R���|�[�l���g�B
// ���ۂ̕`�揈����D3D12Renderer���s�����A�`��ɕK�v�ȃ��\�[�X���͂��̃R���|�[�l���g���ێ�����B
class MeshRendererComponent : public Component
{
    // D3D12Renderer �N���X���t�����h�Ƃ��Đ錾���邱�ƂŁA
    // D3D12Renderer �����̃N���X�� private/protected �����o�[�ɒ��ڃA�N�Z�X�ł���悤�ɂȂ�B
    // ����́AD3D12Renderer �����̃R���|�[�l���g��GPU���\�[�X�iVertexBuffer, IndexBuffer�j��
    // ���ڏ������E�Ǘ����邽�߂ɗp������݌v�p�^�[���B
    friend class D3D12Renderer;

public:
    // ���̃R���|�[�l���g�̐ÓI�Ȍ^���ʎq��Ԃ��֐��B
    // GetComponent<T>() �̂悤�ȃe���v���[�g�֐��Ō^�����ʂ��邽�߂Ɏg�p�����B
    static ComponentType StaticType() { return MeshRenderer; }

    // �R���X�g���N�^:
    // ���N���X�ł��� Component �̃R���X�g���N�^���Ăяo���A�^�C�v�� MeshRenderer �ɐݒ肷��B
    MeshRendererComponent();

    // �f�X�g���N�^:
    // �f�t�H���g�̃f�X�g���N�^���g�p�BComPtr�������I�Ƀ��\�[�X��������邽�߁A���ʂȏ����͕s�v�B
    virtual ~MeshRendererComponent() = default;

    // ���b�V���f�[�^��ݒ肷��֐�:
    // CPU����MeshData�����̃R���|�[�l���g�ɐݒ肷��B
    // GPU���\�[�X�̍쐬�͂����ł͂Ȃ��AD3D12Renderer::CreateMeshRendererResources�ōs����B
    void SetMesh(const MeshData& meshData);

    // --- GPU���\�[�X�ƃr���[ ---
    // ������D3D12Renderer�ɂ����GPU��ɍ쐬����A���̃R���|�[�l���g�����̃n���h����ێ�����B
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;      // ���_�f�[�^���i�[����GPU���\�[�X
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView;                // ���_�o�b�t�@��GPU���ǂ̂悤�ɉ��߂��邩���`����r���[

    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;       // �C���f�b�N�X�f�[�^���i�[����GPU���\�[�X
    D3D12_INDEX_BUFFER_VIEW IndexBufferView;                  // �C���f�b�N�X�o�b�t�@��GPU���ǂ̂悤�ɉ��߂��邩���`����r���[

    UINT IndexCount; // �`�悷��C���f�b�N�X�̑����BDrawIndexedInstanced�Ăяo���ɕK�v�B

    // �������֐�:
    // ���̃R���|�[�l���g�Ǝ��̏��������W�b�N��D3D12Renderer::CreateMeshRendererResources�ōs���邽�߁A
    // �����ł͊��N���X��Initialize���I�[�o�[���C�h���ċ�̎����Ƃ���B
    virtual void Initialize() override {}

private:
    // CPU���ŕێ����郁�b�V���f�[�^�B
    // �K�v�ɉ�����GPU�ɃA�b�v���[�h����鐶�f�[�^�BD3D12Renderer����A�N�Z�X�����B
    MeshData m_MeshData;
};