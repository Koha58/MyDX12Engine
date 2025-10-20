#pragma once

/*
===============================================================================
 EditorInterop.h
-------------------------------------------------------------------------------
�ړI
- ImGui ���i�G�f�B�^ UI�j�ƃQ�[�����i�R���|�[�l���g/���͏����j�ŋ��L������
  �uScene �r���[�̃z�o�[/�t�H�[�J�X��ԁv���󂯓n�����߂̋ɏ��u���b�W�B
- ��̗�F
  * ImGuiLayer �� Scene �E�B���h�E���z�o�[���ꂽ�� SetSceneHovered(true)
  * CameraController �Ȃǂ� IsSceneHovered()/IsSceneFocused() ���Q�Ƃ���
    ���͂��󂯕t���邩�ǂ��������߂�

�݌v����
- �O���[�o���ȃt���O�� getter/setter �ŕ�񂾂����̌y�� API�B
- �X���b�h�Z�[�t�ł͂���܂���i�ʏ�AUI �Ɠ��͏����͓��X���b�h�ŉ񂷑z��j�B
- �u���߃t���[���ł̏�ԁv���������b�`�I�Ȏg������z��B
  �t���[���̓��� UI ���� Set�` ���A�Q�[�������Q�Ƃ���^�p�ɂ��Ă��������B
===============================================================================
*/
struct EditorInterop {
    /**
     * @brief Scene �r���[���}�E�X�z�o�[�����ǂ�����ݒ肷��
     * @param v true: �z�o�[�� / false: ��z�o�[
     * @note ImGuiLayer::BuildDockAndWindows �Ȃ� UI �\�z���ɍX�V����z��
     */
    static void SetSceneHovered(bool v);

    /**
     * @brief Scene �r���[���t�H�[�J�X�i�A�N�e�B�u�j�������Ă��邩�ݒ肷��
     * @param v true: �t�H�[�J�X���� / false: �t�H�[�J�X�Ȃ�
     * @note ImGui::IsWindowFocused() ���̌��ʂōX�V����z��
     */
    static void SetSceneFocused(bool v);

    /**
     * @brief ���݂̃t���[���� Scene �r���[���z�o�[�����ǂ���
     * @return true: �z�o�[�� / false: ��z�o�[
     * @note ���̓R���|�[�l���g���i�J��������Ȃǁj���Q��
     */
    static bool IsSceneHovered();

    /**
     * @brief ���݂̃t���[���� Scene �r���[���t�H�[�J�X�������ǂ���
     * @return true: �t�H�[�J�X���� / false: �t�H�[�J�X�Ȃ�
     * @note ���̗͂D��x�����V���[�g�J�b�g�̉ۂȂǂŎg�p
     */
    static bool IsSceneFocused();
};
