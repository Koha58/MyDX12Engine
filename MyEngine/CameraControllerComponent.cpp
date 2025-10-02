#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "CameraControllerComponent.h"
#include "GameObject.h"
#include "Input.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include <algorithm> // std::clamp, std::max
#include <cmath>     // sinf, cosf

using namespace DirectX;

// ============================================================================
// Controls (Unityライク)
// ----------------------------------------------------------------------------
// マウス：
//   ・MMB（中ボタン） + ドラッグ …… パン（画面と平行に移動：左右・上下）
//   ・RMB（右ボタン） + ドラッグ …… フライ回転（視点の向きを変更）
//   ・Alt + LMB（左ボタン）+ ドラッグ …… オービット（画面中央の注視点を中心に回転）
//   ・マウスホイール …………………… ドリー（前後移動 / ズーム相当）
//
// キーボード（RMB押下中のフライ時に有効）：
//   ・W / S ……………………………… 前進 / 後退（ローカル forward）
//   ・A / D ……………………………… 左 / 右（ローカル right）
//   ・Q / E ……………………………… 下降 / 上昇（ワールド up）
//   ・Ctrl（左右どちらでも） ………… フライ速度ブースト（m_cfg.flyBoost 倍）
//
// 備考：
//   ・「押した瞬間に姿勢が飛ぶ」を防ぐため、RMB/Alt+LMBの押下フレームは
//     姿勢を変更しない（*JustStartedフラグで制御）。
//   ・パンの上下方向は、Windowsのマウス座標系（上へ動かすと dy<0）に合わせて
//     +up へは -dy を使用。
//   ・左右/上下の反転や感度は CameraTuning (m_cfg) で調整可能。
//      - 左右反転：HandleFly/HandleOrbit の yaw 加算の符号を反転
//      - 上下反転：pitch 側の in.dy の符号を反転
// ============================================================================

//==============================================================================
// コンストラクタ：毎フレームの GetComponent を避けるため Transform をキャッシュ
//==============================================================================
CameraControllerComponent::CameraControllerComponent(GameObject* owner, CameraComponent* camera)
    : Component(ComponentType::None), m_Camera(camera)
{
    m_Transform = owner->GetComponent<TransformComponent>().get();
}

void CameraControllerComponent::OnEnable() { OutputDebugStringA("CameraControllerComponent: OnEnable\n"); }
void CameraControllerComponent::OnDisable() { OutputDebugStringA("CameraControllerComponent: OnDisable\n"); }

//==============================================================================
// Update（毎フレーム）
// 1) ImGui がマウスを掴んでいれば何もしない
// 2) 入力を一括でスナップショット化（ReadInput）
// 3) モード優先順に処理：パン→オービット→ドリー→ホイール→フライ
//    ※どれかが動いたらそのフレームは return（モード競合防止）
//==============================================================================
void CameraControllerComponent::Update(float deltaTime)
{
    if (!m_Transform || !m_Camera) return;

    // 1) ImGui がマウスを使用中なら早期 return
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    // 2) 入力を読む
    const CameraInputState in = ReadInput();

    // --- Alt+LMB のエッジ検出（オービット開始/終了） -------------------------
    const bool altLmb = (in.alt && in.lmb);

    if (altLmb && !m_prevAltLmb) // 押し始め
    {
        // 現在姿勢を保存（開始フレームは復元してスキップ）
        m_pressPos = m_Transform->Position;
        m_pressRot = m_Transform->Rotation;

        // pivot は「今の forward × 距離」の先
        XMVECTOR pos = XMLoadFloat3(&m_Transform->Position);
        XMVECTOR fwd = m_Transform->GetForwardVector();
        float distGuess = (m_OrbitDist > 0.0f) ? m_OrbitDist : 5.0f;
        XMVECTOR pivotV = XMVectorAdd(pos, XMVectorScale(fwd, distGuess));
        XMStoreFloat3(&m_orbitPivot, pivotV);

        // 実距離に更新
        m_OrbitDist = XMVectorGetX(XMVector3Length(XMVectorSubtract(pivotV, pos)));

        // ★基準角は「いまの Rotation」から読む（LookAt は使わない）
        m_orbitYaw0 = m_pressRot.y;
        m_orbitPitch0 = m_pressRot.x;

        m_orbitAccX = 0.0f;
        m_orbitAccY = 0.0f;

        m_orbitActive = true;
        m_orbitJustStarted = true;
    }

    else if (!altLmb && m_prevAltLmb) // ★離した
    {
        m_orbitActive = false;
        m_orbitJustStarted = false;
    }
    m_prevAltLmb = altLmb;

    // --- RMB(フライ) のエッジ検出 -------------------------------------------
    const bool rmbNow = (in.rmb);

    if (rmbNow && !m_flyPrevRmb) // ★押し始め
    {
        // 押下時の Transform を保存（押下フレームは復元）
        m_pressPos = m_Transform->Position;
        m_pressRot = m_Transform->Rotation;

        // 基準角は現在角から
        m_flyYaw0 = m_pressRot.y;
        m_flyPitch0 = m_pressRot.x;

        // 累計ドラッグをリセット
        m_flyAccX = 0.0f;
        m_flyAccY = 0.0f;

        m_flyJustStarted = true;  // ★このフレームは見た目を変えない
    }
    else if (!rmbNow && m_flyPrevRmb) // ★離した
    {
        m_flyJustStarted = false;
    }
    m_flyPrevRmb = rmbNow;

    // ★押した瞬間のフレームは、必ず保存した Transform に復元して終了
    if (m_orbitJustStarted || m_flyJustStarted)
    {
        m_Transform->Position = m_pressPos;
        m_Transform->Rotation = m_pressRot;

        // 次フレームから通常動作
        if (m_orbitJustStarted) m_orbitJustStarted = false;
        if (m_flyJustStarted)   m_flyJustStarted = false;
        return;
    }

    // 3) 優先度順に処理
    if (HandlePan(in))            return; // 中ボタン（MMB）：パン
    if (HandleOrbit(in))          return; // Alt+LMB：オービット
    if (HandleDolly(in))          return; // Alt+RMB：ドリー距離変更
    if (HandleWheel(in))          return; // ホイール：前後ドリー
    if (HandleFly(in, deltaTime)) return; // RMB：フライ（回転＋移動）
}

//==============================================================================
// ReadInput：Input から 1 フレーム分の入力状態を収集
//------------------------------------------------------------------------------
CameraInputState CameraControllerComponent::ReadInput() const
{
    CameraInputState s{};
    s.lmb = Input::GetMouseButton(MouseButton::Left);
    s.mmb = Input::GetMouseButton(MouseButton::Middle);
    s.rmb = Input::GetMouseButton(MouseButton::Right);
    s.alt = Input::GetKey(KeyCode::LeftAlt) || Input::GetKey(KeyCode::RightAlt);
    s.ctrl = Input::GetKey(KeyCode::LeftControl) || Input::GetKey(KeyCode::RightControl);

    // マウス移動量は1回でまとめて取得（XとYでタイミング差を出さない）
    auto md = Input::GetMouseDelta();
    s.dx = md.x;
    s.dy = md.y;

    s.wheel = Input::GetMouseScrollDelta();
    return s;
}

//==============================================================================
// HandlePan（MMB + ドラッグ）
//   ※中ボタンドラッグでパン（画面平行の左右・上下移動）
//------------------------------------------------------------------------------
bool CameraControllerComponent::HandlePan(const CameraInputState& in)
{
    if (!(in.mmb && !in.alt)) return false;

    XMVECTOR pos = XMLoadFloat3(&m_Transform->Position);
    XMVECTOR right = m_Transform->GetRightVector();
    XMVECTOR fwd = m_Transform->GetForwardVector();

    // 画面“上”ベクトルを Forward × Right で求める（左手系: forward×right = up）
    XMVECTOR up = XMVector3Cross(fwd, right);
    if (XMVector3Less(XMVector3LengthSq(up), XMVectorReplicate(1e-6f)))
    {
        up = XMVectorSet(0.f, 1.f, 0.f, 0.f);
    }      
    else
    {
        up = XMVector3Normalize(up);
    }
        
    // Unity感：右へドラッグで +right、上へドラッグ（dy<0）で +up（-dy）
    pos = XMVectorAdd(pos, XMVectorScale(right, +in.dx * m_cfg.panSpeed));
    pos = XMVectorAdd(pos, XMVectorScale(up, -in.dy * m_cfg.panSpeed));

    XMStoreFloat3(&m_Transform->Position, pos);
    return true;
}

//==============================================================================
// HandleOrbit（Alt + LMB + ドラッグ）
//   ※Alt+左ドラッグでオービット（画面中央の注視点を中心に回転）
//------------------------------------------------------------------------------
// オービット（Alt + LMB）
bool CameraControllerComponent::HandleOrbit(const CameraInputState& in)
{
    // 条件：Alt+LMB を押している＆開始処理済み
    if (!(in.alt && in.lmb) || !m_orbitActive)
        return false;

    // 開始フレームは見た目を変えない（クリック時の“飛び”防止）
    if (m_orbitJustStarted) {
        m_orbitJustStarted = false;
        return true;
    }

    // 累計ドラッグで相対角を決定（※左右反転したければ dx の符号を変える）
    m_orbitAccX += in.dx;
    m_orbitAccY += in.dy;

    const float newYaw = m_orbitYaw0 + m_orbitAccX * m_cfg.lookSpeed;
    const float newPitch = std::clamp(m_orbitPitch0 + m_orbitAccY * m_cfg.lookSpeed, -89.0f, 89.0f);

    // 1) まず回転だけを確定（Transform の forward はこの回転から計算される）
    m_Transform->Rotation = { newPitch, newYaw, 0.0f };

    // 2) その回転から forward を取得（Transform の式に完全に合わせる）
    DirectX::XMVECTOR pivot = XMLoadFloat3(&m_orbitPivot);
    DirectX::XMVECTOR forward = m_Transform->GetForwardVector(); // ← これが最重要
    DirectX::XMVECTOR pos = DirectX::XMVectorSubtract(pivot, DirectX::XMVectorScale(forward, m_OrbitDist));

    // 3) 位置を反映（向きは上で確定済み）
    DirectX::XMStoreFloat3(&m_Transform->Position, pos);

    return true;
}


//==============================================================================
// HandleDolly（Alt + RMB + ドラッグ）
//   ※Alt+右ドラッグでオービット距離を変更（右/下ドラッグで近づく符号）
//------------------------------------------------------------------------------
bool CameraControllerComponent::HandleDolly(const CameraInputState& in)
{
    if (!(in.alt && in.rmb)) return false;

    // 0.01f で下限クリップ（ゼロ割れ・反転防止）
    m_OrbitDist = std::max(0.01f, m_OrbitDist + (-in.dx - in.dy) * m_cfg.dollySpeed);
    return true;
}

//==============================================================================
// HandleWheel（マウスホイール）
//   ※ホイールで前後ドリー（望遠的なFOV変更ではなく位置移動）
//------------------------------------------------------------------------------
bool CameraControllerComponent::HandleWheel(const CameraInputState& in)
{
    if (in.wheel == 0.0f) return false;

    XMVECTOR pos = XMLoadFloat3(&m_Transform->Position);
    XMVECTOR forward = m_Transform->GetForwardVector();

    pos = XMVectorAdd(pos, XMVectorScale(forward, in.wheel * m_cfg.wheelSpeed));
    XMStoreFloat3(&m_Transform->Position, pos);
    return true;
}

//==============================================================================
// HandleFly（RMB + ドラッグ / + WASD/QE）
//   ※右ドラッグで視線回転、WASD/QE と Ctrl でフライ移動
//------------------------------------------------------------------------------
bool CameraControllerComponent::HandleFly(const CameraInputState& in, float dt)
{
    if (!in.rmb) return false;

    // 累計ドラッグを更新（左右反転したければ in.dx の符号を反転）
    m_flyAccX += in.dx;
    m_flyAccY += in.dy;

    // 相対ドラッグで角度を決定
    m_Yaw = m_flyYaw0 + m_flyAccX * m_cfg.lookSpeed;
    m_Pitch = std::clamp(m_flyPitch0 + m_flyAccY * m_cfg.lookSpeed, -89.0f, 89.0f);

    // Transform の回転を直接反映（Z=0：ロール無し）
    m_Transform->Rotation = { m_Pitch, m_Yaw, 0.0f };

    // 方向ベクトルは回転反映後に取得
    XMVECTOR pos = XMLoadFloat3(&m_Transform->Position);
    XMVECTOR forward = m_Transform->GetForwardVector();
    XMVECTOR right = m_Transform->GetRightVector();
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    const float v = m_cfg.flySpeed * dt * (in.ctrl ? m_cfg.flyBoost : 1.0f);

    // キーに応じて移動（合成可：例 W + D で斜め前右）
    if (Input::GetKey(KeyCode::W)) pos = XMVectorAdd(pos, XMVectorScale(forward, +v));
    if (Input::GetKey(KeyCode::S)) pos = XMVectorAdd(pos, XMVectorScale(forward, -v));
    if (Input::GetKey(KeyCode::A)) pos = XMVectorAdd(pos, XMVectorScale(right, -v));
    if (Input::GetKey(KeyCode::D)) pos = XMVectorAdd(pos, XMVectorScale(right, +v));
    if (Input::GetKey(KeyCode::E)) pos = XMVectorAdd(pos, XMVectorScale(up, +v));
    if (Input::GetKey(KeyCode::Q)) pos = XMVectorAdd(pos, XMVectorScale(up, -v));

    XMStoreFloat3(&m_Transform->Position, pos);
    return true;
}
