#pragma once
#include <functional>
#include <cstdint>
#include "imgui.h"

/*
    EditorContext
    ----------------------------------------------------------------------------
    �����F
      - �����_���� �� ImGui ���ŋ��L����g�ҏW UI �̏�ԁh�̎󂯓n���p POD�B
      - 1�t���[���̒��Łu�����_�������߂� �� ImGui ���Q��/�X�V �� �����_�������ʂ�ǂށv
        �Ƃ����Е����f�[�^�t���[��z��B

    ���C�t�T�C�N���E�X�V���̃C���[�W�F
      1) �����_�����t���[���`���ŁAScene/Game �� SRV ���RT�T�C�Y���𖄂߂�
         �i��FSceneLayer::FeedToUI / SyncStatsTo �ōX�V�j
      2) ImGui ���iEditorPanels �Ȃǁj�����̒l�����ăE�B���h�E�\�z�E���͎擾
      3) ���ʁi�Ⴆ�΁uScene �r���[�|�[�g�̎��\���T�C�Y�v��uHovered/Focused�v�j��
         ���̍\���̂ɏ����߂�
      4) �����_���͏����߂��ꂽ�l�isceneViewportSize �Ȃǁj���g����
         ���t���[���̃��T�C�Y�v����������

    �p��F
      - RT�T�C�Y�isceneRTWidth/Height, gameRTWidth/Height�j�F
        ���ۂ̃I�t�X�N���[�� RenderTarget �̃s�N�Z���T�C�Y
      - ViewportSize�isceneViewportSize, gameViewportSize�j�F
        ImGui ��Ƀe�N�X�`�������s�N�Z���l���ŕ`�����i�����̈�̊�]�T�C�Y�j
      - ImTextureID�isceneTexId, gameTexId�j�F
        imgui_impl_dx12 �������ŕێ�����uGPU �� SRV �f�B�X�N���v�^�v���w�� ID
*/

struct EditorContext {
    // ----------------------------------------------------------------------------
    // �g�O���E�����C�A�E�g
    // ----------------------------------------------------------------------------
    bool* pEnableEditor = nullptr; // �G�f�B�^ UI �S�̗̂L��/�����i�`�F�b�N�{�b�N�X�A���Ȃǁj
    bool* pRequestResetLayout = nullptr; // true �ɂ���ƃ��C�A�E�g��������ԂɃ��Z�b�g�i1�t���g���̂āj
    bool* pAutoRelayout = nullptr; // �������C�A�E�g�œK����L�������邩�i�^�p�|���V�[�C���j

    // ----------------------------------------------------------------------------
    // Scene �r���[�i�I�t�X�N���[�� RT �� ImGui �ɕ\���j
    // ----------------------------------------------------------------------------
    ImTextureID sceneTexId = 0;               // ImGui::Image �ɓn�� SRV �� ID�iimgui_impl_dx12 ���s�j
    ImVec2      sceneViewportSize = ImVec2(0, 0); // ���ۂ� ImGui ��ŕ`�悳�ꂽ��`�T�C�Y�i�s�N�Z���j
    bool        sceneHovered = false;           // ���t���[���AScene �r���[���}�E�X�z�o�[����Ă��邩
    bool        sceneFocused = false;           // ���t���[���AScene �r���[���L�[�{�[�h�t�H�[�J�X������
    unsigned    sceneRTWidth = 0;               // �� RT �̕��i�s�N�Z���j�� Viewports::FeedToUI �Ŗ��߂�
    unsigned    sceneRTHeight = 0;               // �� RT �̍����i�s�N�Z���j

    // ----------------------------------------------------------------------------
    // Game �r���[�i�Œ�J�����o�͂� ImGui �ɕ\���j
    //   - �u����(View)�͌Œ�E���e(Proj)�̂݃A�X�y�N�g�Ǐ]�v�ɂ��邱�ƂŁA
    //     �E�B���h�E���T�C�Y�Łg�������ς��h�̂�h���݌v�iViewports ���Q�Ɓj
    // ----------------------------------------------------------------------------
    ImTextureID gameTexId = 0;               // Game �p SRV �� ID
    ImVec2      gameViewportSize = ImVec2(0, 0);  // ImGui ��ł̕\���T�C�Y�i�s�N�Z���j
    bool        gameHovered = false;           // ���t���[���AGame �r���[���z�o�[����Ă��邩
    bool        gameFocused = false;           // ���t���[���AGame �r���[���t�H�[�J�X������
    unsigned    gameRTWidth = 0;               // �� RT �̕��i�s�N�Z���j
    unsigned    gameRTHeight = 0;               // �� RT �̍����i�s�N�Z���j

    // ----------------------------------------------------------------------------
    // �X���b�v�`�F�C���i�o�b�N�o�b�t�@�j���̏��E���v
    // ----------------------------------------------------------------------------
    std::uint32_t rtWidth = 0;               // ���݂̃o�b�N�o�b�t�@���iSwapChain �̎��s�N�Z���j
    std::uint32_t rtHeight = 0;               // ���݂̃o�b�N�o�b�t�@��
    float         fps = 0.0f;            // ImGui::GetIO().Framerate �����疄�߂�

    // ----------------------------------------------------------------------------
    // �p�l���`��̃G���g���|�C���g�i�Ăяo�����������_���l�߂�j
    //   - Hierarchy/Inspector �̕`��͊֐��|�C���^�ł͂Ȃ� std::function �Ŏ󂯂�
    // ----------------------------------------------------------------------------
    std::function<void()> DrawInspector;          // ��F�I�� GameObject �� Transform ��
    std::function<void()> DrawHierarchy;          // ��F�V�[���c���[�i�e�q�֌W�̉����j

    // ----------------------------------------------------------------------------
    // �t���O�FScene �r���[�̃��T�C�Y���삪�i�s����
    //   - ��F�h���b�O�ŃT�C�Y���h��Ă���Ԃ� true�A���肵���� false
    //   - �f�o�E���X�̒�����u�m�肵���� RT ����蒼���v���̔���Ɏg����
    // ----------------------------------------------------------------------------
    bool sceneResizing = false;
};
