#pragma once
#include "Component.h"
#include <DirectXMath.h>

class GameObject;

/*
===============================================================================
 CameraComponent.h
 ����:
   - �Q�[�����́u���_�v��\���R���|�[�l���g�B
   - View �s��i�ʒu�E�����j�� Projection �s��i�������e�̃p�����[�^�j��ێ����A
     �����_������������Q�Ƃ��ĕ`�悷��B

 �݌v����:
   - �p�x�͓x(degree)�ŕێ����ADirectXMath �̍s�񐶐��ł͓����Ń��W�A���ɕϊ�����z��B
   - �s���x�N�g���̓w�b�_�̉���������邽�� DirectX:: ��**���S�C��**�Ŏg�p�B
   - �A�X�y�N�g��́u�`��Ώۂ̕�/�����v�B�E�B���h�E�S�̂ł͂Ȃ�**�N���C�A���g�̈�**���g�����ƁB
   - ���W�n�� DirectX �̍���n (LH) �O��iXMMatrixPerspectiveFovLH / XMMatrixLookToLH ���g�p�j�B

 �T�^�I�ȗ��p�菇:
   1) GameObject �� CameraComponent �� AddComponent ����
   2) �K�v�ɉ����� SetFOV / SetAspect �œ��e���X�V
   3) ���t���[�� Update() �ŁA���L GameObject �� Transform ���� View ���X�V
===============================================================================
*/
class CameraComponent : public Component {
public:
    //--------------------------------------------------------------------------
    // �R���X�g���N�^
    // @param owner  : ���̃J���������L���� GameObject�iTransform ���Q�Ƃ���j
    // @param fov    : ����p�i�x�j�B��ʓI�ɂ� 45�`75 ���x
    // @param aspect : �A�X�y�N�g��i��/�����j�B�N���C�A���g�̈悩��Z�o���ēn��
    // @param nearZ  : �j�A�N���b�v�ʂ̋����i>0�j�B�ɒ[�ɏ���������Ɛ[�x���x��������
    // @param farZ   : �t�@�[�N���b�v�ʂ̋����inearZ ���傫���j
    //--------------------------------------------------------------------------
    CameraComponent(GameObject* owner,
        float fov = 60.0f,
        float aspect = 16.0f / 9.0f,
        float nearZ = 0.1f,
        float farZ = 1000.0f);

    //--------------------------------------------------------------------------
    // �t���[���X�V
    //  - ���L GameObject �� Transform�i�ʒu/�����j���� View �s����v�Z����
    //  - deltaTime �͍��̂Ƃ��떢�g�p�i�����̃��[�V�����u���[/�J�����A�j���p�j
    //--------------------------------------------------------------------------
    void Update(float deltaTime) override;

    // ���C�t�T�C�N���ʒm�i�K�v�ɉ����ă��O���ɗ��p�j
    void OnEnable() override;
    void OnDisable() override;
    void OnDestroy() override;

    //--------------------------------------------------------------------------
    // �s��擾�i�����_�������Q�Ɓj
    //  - �Ԃ��Q�Ƃ͂��̃R���|�[�l���g�̓����o�b�t�@�i���L���͈ړ����Ȃ��j
    //--------------------------------------------------------------------------
    const DirectX::XMMATRIX& GetViewMatrix() const { return m_View; }
    const DirectX::XMMATRIX& GetProjectionMatrix() const { return m_Projection; }

    //--------------------------------------------------------------------------
    // ���e�p�����[�^�ݒ�
    //  - �l��ς���Ǝ����� Projection �s�񂪍Čv�Z�����
    //--------------------------------------------------------------------------
    void SetFOV(float fov) { m_FOV = fov;     UpdateProjectionMatrix(); }
    void SetAspect(float aspect) { m_Aspect = aspect; UpdateProjectionMatrix(); }

    //--------------------------------------------------------------------------
    // View �s���**����**�w�肵�����ꍇ�Ɏg�p�iLookTo �`���j
    //  - position : �J�����ʒu�i���[���h�j
    //  - forward  : ���������i���[���h�A���K�������j
    //  - up       : ������i�ʏ�� (0,1,0)�j
    // �� �ʏ�� Transform ���玩���v�Z����邽�ߕs�v�B�Œ�J�����p�r�ȂǂɁB
    //--------------------------------------------------------------------------
    void SetView(const DirectX::XMVECTOR& position,
        const DirectX::XMVECTOR& forward,
        const DirectX::XMVECTOR& up);

private:
    //--------------------------------------------------------------------------
    // ����: View �s����X�V�i����n�� LookTo�j
    //--------------------------------------------------------------------------
    void UpdateViewMatrix(const DirectX::XMVECTOR& position,
        const DirectX::XMVECTOR& forward,
        const DirectX::XMVECTOR& up);

    //--------------------------------------------------------------------------
    // ����: Projection �s����X�V�i����n�� PerspectiveFov�j
    //  - �������e�̊�{���Ɋ�Â��Am_FOV/m_Aspect/m_NearZ/m_FarZ ����Čv�Z
    //--------------------------------------------------------------------------
    void UpdateProjectionMatrix();

private:
    // ���L�ҁiTransform ���Q�Ƃ��邽�߂ɕK�v�Bnullptr �̏ꍇ�� Update �ŉ������Ȃ��j
    GameObject* m_Owner = nullptr;

    // ���e�p�����[�^�i�K�v�ɉ����� Editor �����瑀��j
    float                 m_FOV = 60.0f;       // ����p[deg]
    float                 m_Aspect = 16.0f / 9.0f;  // ��/����
    float                 m_NearZ = 0.1f;        // �߃N���b�v
    float                 m_FarZ = 1000.0f;     // ���N���b�v

    // �v�Z�ςݍs��i�����_�������t���Q�Ɓj
    DirectX::XMMATRIX     m_View;        // �J�������W�n�i���[���h���r���[�j
    DirectX::XMMATRIX     m_Projection;  // �������e�i�r���[���N���b�v�j
};
