#pragma once
#include <memory>
#include "imgui.h"  // ImTextureID / ImVec2 ���g�����߁iImGui �̊�{�^�j

// �O���錾�i�����ł͌^������������Ώ\���B�d���w�b�_�ˑ�������ăr���h���ԒZ�k�j
class Scene;
class GameObject;

namespace EditorPanels
{
    // =========================================================================
    // GONameUTF8
    // -------------------------------------------------------------------------
    // �EGameObject �̕\�����iUTF-8�j��Ԃ����[�e�B���e�B�B
    // �EImGui ���͊�{ UTF-8 �z��̂��߁A�G�f�B�^�\���Œ��ڎg����`�ɂ��Ă����B
    // �Ego==nullptr �̏ꍇ�� "(null)" �Ȃǂ̃v���[�X�z���_��Ԃ�������z��B
    // =========================================================================
    const char* GONameUTF8(const GameObject* go);

    // =========================================================================
    // DrawHierarchy
    // -------------------------------------------------------------------------
    // �E�V�[���̃c���[�i�e�q�K�w�j���c���[�r���[�ŕ`�悷��B
    // �E�m�[�h�N���b�N�ɂ���đI��Ώہiselected�j���X�V����B
    // �Eselected �͊O���ł��ێ�����邽�� std::weak_ptr �Ŏ󂯓n���B
    //   �i�I��Ώۂ��j�����ꂽ�ꍇ�Ɏ����Ŗ���������闘�_�j
    //
    // @param scene    : �`��Ώۂ̃V�[���i���[�g�z����񋓂��ăc���[���j
    // @param selected : ���݂̑I����ێ�/�X�V����n���h���i��Q�Ɓj
    // =========================================================================
    void DrawHierarchy(Scene* scene, std::weak_ptr<GameObject>& selected);

    // =========================================================================
    // DrawInspector
    // -------------------------------------------------------------------------
    // �E�I�� GameObject �̏ڍׁi�C���X�y�N�^�j��`�悷��B
    // �E���ʂ� Transform �̂݁iPosition/Rotation/Scale�j��ҏW�\�ɂ��������z��B
    // �Eselected ������/�ؒf����Ă���΁uNo selection�v�\���B
    //
    // @param selected : �Ώ� GameObject�i��Q�ƁA����������UI���ŃK�[�h�j
    // =========================================================================
    void DrawInspector(const std::weak_ptr<GameObject>& selected);

    // =========================================================================
    // DrawViewportTextureNoEdge
    // -------------------------------------------------------------------------
    // �E�I�t�X�N���[�� RT ���u�g�Ȃ��E�ɂ��ݗ}���v�� ImGui �ɓ\�邽�߂̃w���p�B
    // �E���e�N�Z������ UV �I�t�Z�b�g�i0.5/Width, 0.5/Height�j������
    //   �T���v�����O���E�̂ɂ��݂��y���B
    // �EwantSize �̓E�B���h�E/�̈�ɍ��킹�ČĂяo�����Ōv�Z������]�T�C�Y�B
    //
    // @param tex     : ImGui �ɓn���e�N�X�`��ID�i�O��SRV�q�[�v���ō쐬�������́j
    // @param texW    : �e�N�X�`�����i�s�N�Z���j
    // @param texH    : �e�N�X�`�������i�s�N�Z���j
    // @param wantSize: �\���������T�C�Y�i�s�N�Z���AImVec2�j
    // =========================================================================
    void DrawViewportTextureNoEdge(ImTextureID tex, int texW, int texH, ImVec2 wantSize);
}
