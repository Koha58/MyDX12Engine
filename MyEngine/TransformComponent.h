#pragma once
#include "Component.h"
#include <DirectXMath.h>

// ===============================================================
// TransformComponent
//  - GameObject �̈ʒu(Position)�E��](Rotation)�E�g�k(Scale)���Ǘ�����R���|�[�l���g�B
//  - 3D��Ԃł̃��[���h�s��𐶐�����@�\��񋟂���B
//  - Unity �� Transform �Ɠ��l�̖����B
// ===============================================================
class TransformComponent : public Component
{
public:
    // ------------------- �����o�ϐ� -------------------

    DirectX::XMFLOAT3 Position;
    // ���[���h���W�n�ɂ�����ʒu
    // (x, y, z) ��3�������W�l

    DirectX::XMFLOAT3 Rotation;
    // ��]�p�x�i�x���@�j
    // pitch (x����]), yaw (y����]), roll (z����])
    // �� �Ⴆ�� yaw ��ς���Ɛ��������̌������ς��

    DirectX::XMFLOAT3 Scale;
    // �g�k
    // (1,1,1) �œ��{�A(2,2,2) ��2�{�̑傫��

    // ------------------- �R���X�g���N�^ -------------------

    TransformComponent();
    // �f�t�H���g�l:
    // Position = (0,0,0), Rotation = (0,0,0), Scale = (1,1,1)

    // ------------------- �s��v�Z -------------------

    DirectX::XMMATRIX GetWorldMatrix() const;
    // ���[���h�s���Ԃ�
    // �EScale �� RotationX �� RotationY �� RotationZ �� Translation �̏��ō���
    // �E������V�F�[�_�[�ɓn�����ƂŃI�u�W�F�N�g�𐳂����ʒu�ɕ`��ł���

    // ------------------- �����x�N�g���擾 -------------------
    // Unity �� Transform �Ɠ����悤�ɁA�I�u�W�F�N�g�́u�����v���x�N�g���Ŏ擾�ł���B

    DirectX::XMVECTOR GetForwardVector() const;
    // �O���� (Z+ �) �����[���h��Ԃɕϊ����ĕԂ�
    // �J�����̑O�i��I�u�W�F�N�g�̈ړ������ɗ��p

    DirectX::XMVECTOR GetRightVector() const;
    // �E���� (X+ �) �����[���h��Ԃɕϊ����ĕԂ�
    // �X�g���C�t�ړ��⃍�[�J�����W�ł̉��ړ��ɗ��p

    DirectX::XMVECTOR GetUpVector() const;
    // ����� (Y+ �) �����[���h��Ԃɕϊ����ĕԂ�
    // �W�����v��J�����́u������x�N�g���v�ɗ��p
};
