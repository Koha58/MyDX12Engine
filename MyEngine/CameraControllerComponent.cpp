#include "CameraControllerComponent.h"
#include "GameObject.h"
#include "Input.h"
#include <algorithm> // std::clamp

using namespace DirectX;

// =============================
// CameraControllerComponent
// -----------------------------
// ・FPSライクなカメラ操作（マウスで回転、WASD/Space/Ctrlで平行移動）
// ・TransformComponent を直接更新する（位置・オイラー回転）
// ・CameraComponent 自体は View 行列を Transform から計算する想定
// =============================
CameraControllerComponent::CameraControllerComponent(GameObject* owner, CameraComponent* camera)
    : Component(ComponentType::None), m_Camera(camera)
{
    // Owner の Transform をキャッシュ（毎フレーム検索しない）
    // GetComponent が shared_ptr を返す想定のため .get() で生ポインタ化
    m_Transform = owner->GetComponent<TransformComponent>().get();
}

void CameraControllerComponent::OnEnable()
{
    OutputDebugStringA("CameraControllerComponent: OnEnable\n");
}

void CameraControllerComponent::OnDisable()
{
    OutputDebugStringA("CameraControllerComponent: OnDisable\n");
}

void CameraControllerComponent::Update(float deltaTime)
{
    if (!m_Transform || !m_Camera) return;

    // =============================
    // 1) マウス入力で視点回転
    // -----------------------------
    // ・Yaw（水平）にマウスX、Pitch（垂直）にマウスYを反映
    // ・Pitch は真上/真下付近での反転を防ぐため ±89° にクランプ
    // ・ここでは「度数」で保持している前提（Transform 側で適宜ラジアン変換）
    // =============================
    auto delta = Input::GetMouseDelta();          // フレーム内のマウス移動量（ピクセル）
    m_Yaw += delta.x * m_MouseSensitivity;      // 右ドラッグで +Yaw
    m_Pitch -= delta.y * m_MouseSensitivity;      // 上ドラッグで 視線↑（直感に合わせてマイナス）

    m_Pitch = std::clamp(m_Pitch, -89.0f, 89.0f); // 真上/真下を超えない

    // Transform のオイラー角（度数）を直接更新（Z回転は未使用）
    m_Transform->Rotation = { m_Pitch, m_Yaw, 0.0f };

    // =============================
    // 2) キー入力で移動
    // -----------------------------
    // ・W/S で前後、A/D で左右、Space/Ctrl で上下
    // ・forward/right は現在の回転に基づく方向ベクトル（正規化想定）
    // ・移動はワールド空間の up(0,1,0) を採用（ロール無視）
    // =============================
    XMVECTOR pos = XMLoadFloat3(&m_Transform->Position);
    XMVECTOR forward = m_Transform->GetForwardVector(); // カメラの前方向（回転反映済み）
    XMVECTOR right = m_Transform->GetRightVector();   // カメラの右方向
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);         // 世界上方向（ロールを考慮しない）

    // 前後移動
    if (Input::GetKey(KeyCode::W)) pos += forward * m_MoveSpeed * deltaTime;
    if (Input::GetKey(KeyCode::S)) pos -= forward * m_MoveSpeed * deltaTime;

    // 左右移動（ストレイフ）
    if (Input::GetKey(KeyCode::A)) pos -= right * m_MoveSpeed * deltaTime;
    if (Input::GetKey(KeyCode::D)) pos += right * m_MoveSpeed * deltaTime;

    // 上下移動（昇降）
    if (Input::GetKey(KeyCode::Space))        pos += up * m_MoveSpeed * deltaTime;
    if (Input::GetKey(KeyCode::LeftControl))  pos -= up * m_MoveSpeed * deltaTime;
    if (Input::GetKey(KeyCode::RightControl)) pos -= up * m_MoveSpeed * deltaTime;

    // 位置を Transform に書き戻し
    XMStoreFloat3(&m_Transform->Position, pos);
}