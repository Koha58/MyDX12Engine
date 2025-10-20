#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "CameraControllerComponent.h"
#include "Scene/GameObject.h"
#include "Core/Input.h"
#include "Core/EditorInterop.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include <algorithm> // std::clamp, std::max
#include <cmath>     // sinf, cosf

using namespace DirectX;

/*
    CameraControllerComponent
    ----------------------------------------------------------------------------
    目的：
      - シーンビュー用のエディタカメラ（Unity風操作）を担当。
      - マウス/キーボード入力から、所有 GameObject の Transform を直接更新する。
      - カメラ本体（CameraComponent）は Transform を参照して描画時に View を作る前提。

    操作（デフォルト）：
      マウス
        * MMBドラッグ：パン（平行移動）
        * RMBドラッグ：フライ（回転）＋ WASD/QE で移動
        * Alt+LMBドラッグ：オービット（注視点 pivot を中心に回転）
        * Alt+RMBドラッグ：ドリー距離変更（pivot との距離）
        * ホイール：前後ドリー（位置を前後に移動）
      キー（RMBフライ中）
        * W/S : 前進/後退
        * A/D : 左/右
        * Q/E : 下/上
        * Ctrl : スピードブースト（flyBoost 倍）

    ポイント：
      - クリック開始フレームで姿勢が飛ばないよう “JustStarted” フラグで1フレーム抑止。
      - 操作の開始は「Sceneウィンドウがホバー中のみ」。継続中はウィンドウ外でもラッチ。
      - ImGui の IO.WantCaptureMouse を尊重。ただし “Scene 上の明示操作” と “継続中” は許可。
*/

//==============================================================================
// コンストラクタ：所有 Transform をキャッシュ（毎フレの GetComponent を避ける）
//==============================================================================
CameraControllerComponent::CameraControllerComponent(GameObject* owner, CameraComponent* camera)
    : Component(ComponentType::None), m_Camera(camera)
{
    // TransformComponent は頻繁アクセスのため生ポインタで保持（所有は GameObject 側）
    m_Transform = owner->GetComponent<TransformComponent>().get();
}

// ログのみ（必要に応じてビューの有効化/無効化処理に置き換え可）
void CameraControllerComponent::OnEnable() { OutputDebugStringA("CameraControllerComponent: OnEnable\n"); }
void CameraControllerComponent::OnDisable() { OutputDebugStringA("CameraControllerComponent: OnDisable\n"); }

//==============================================================================
// Update（毎フレーム）
//  - Scene領域のホバー/操作中ラッチ、ImGuiキャプチャなどのポリシーを先に判定
//  - 優先度付きで各操作ハンドラを実行（パン → オービット → ドリー → ホイール → フライ）
//==============================================================================
void CameraControllerComponent::Update(float deltaTime)
{
    if (!m_Transform || !m_Camera) return;

    ImGuiIO& io = ImGui::GetIO();

    // 1) 入力収集（この段階で一度だけ）
    const CameraInputState in = ReadInput();

    // 2) 「明示的にカメラ操作したい」入力か？
    const bool sceneIntent =
        in.mmb || in.rmb ||
        (in.alt && (in.lmb || in.rmb)) ||
        (std::fabs(in.wheel) > 0.0f);

    // 3) Scene 上での新規開始のみ許可。継続中は許可（ホバーを外れても操作継続）
    const bool sceneHoveredNow = EditorInterop::IsSceneHovered();
    const bool actionActive = m_orbitActive || m_flyPrevRmb; // どれか継続中か
    if (!sceneHoveredNow && !actionActive) {
        return; // ホバー外かつ何も開始していない
    }

    // 4) ImGui がマウスを掴んでいる場合のガード
    //    - Scene上での明示操作 or すでに継続中 なら入力を通す
    if (io.WantCaptureMouse && !((sceneHoveredNow && sceneIntent) || actionActive)) {
        return;
    }

    // --- Alt+LMB（オービット）の開始/終了フラグ管理 ---------------------------
    const bool altLmb = (in.alt && in.lmb);

    if (altLmb && !m_prevAltLmb) // 押し始め
    {
        // 現在姿勢を保存（開始フレームはこの姿勢に復元して視覚的な“飛び”を防ぐ）
        m_pressPos = m_Transform->Position;
        m_pressRot = m_Transform->Rotation;

        // pivot は「forward 方向の距離分」先（暫定距離は既存 dist か 5.0）
        XMVECTOR pos = XMLoadFloat3(&m_Transform->Position);
        XMVECTOR fwd = m_Transform->GetForwardVector();
        float distGuess = (m_OrbitDist > 0.0f) ? m_OrbitDist : 5.0f;
        XMVECTOR pivotV = XMVectorAdd(pos, XMVectorScale(fwd, distGuess));
        XMStoreFloat3(&m_orbitPivot, pivotV);

        // 実距離に更新（押下時点の pivot-位置 の長さ）
        m_OrbitDist = XMVectorGetX(XMVector3Length(XMVectorSubtract(pivotV, pos)));

        // 基準角は現在の回転から（以後、ドラッグ累積で加算）
        m_orbitYaw0 = m_pressRot.y;
        m_orbitPitch0 = m_pressRot.x;

        m_orbitAccX = 0.0f;
        m_orbitAccY = 0.0f;

        m_orbitActive = true;
        m_orbitJustStarted = true; // このフレームは見た目を変えない
    }
    else if (!altLmb && m_prevAltLmb) // 離した
    {
        m_orbitActive = false;
        m_orbitJustStarted = false;
    }
    m_prevAltLmb = altLmb;

    // --- RMB（フライ）の開始/終了フラグ管理 ----------------------------------
    const bool rmbNow = (in.rmb);

    if (rmbNow && !m_flyPrevRmb) // 押し始め
    {
        // 押下時の姿勢を保存（開始フレームは復元）
        m_pressPos = m_Transform->Position;
        m_pressRot = m_Transform->Rotation;

        // 基準角（以後、ドラッグ累積で加算）
        m_flyYaw0 = m_pressRot.y;
        m_flyPitch0 = m_pressRot.x;

        m_flyAccX = 0.0f;
        m_flyAccY = 0.0f;

        m_flyJustStarted = true;
    }
    else if (!rmbNow && m_flyPrevRmb) // 離した
    {
        m_flyJustStarted = false;
    }
    m_flyPrevRmb = rmbNow;

    // クリック開始フレームは保存姿勢に戻して終了（“飛び”抑止）
    if (m_orbitJustStarted || m_flyJustStarted)
    {
        m_Transform->Position = m_pressPos;
        m_Transform->Rotation = m_pressRot;
        if (m_orbitJustStarted) m_orbitJustStarted = false;
        if (m_flyJustStarted)   m_flyJustStarted = false;
        return;
    }

    // 5) 優先度順に処理（複数同時操作の競合を避ける）
    if (HandlePan(in))            return; // MMB：パン
    if (HandleOrbit(in))          return; // Alt+LMB：オービット
    if (HandleDolly(in))          return; // Alt+RMB：ドリー距離変更
    if (HandleWheel(in))          return; // ホイール：前後ドリー
    if (HandleFly(in, deltaTime)) return; // RMB：フライ（回転＋移動）
}

//==============================================================================
// ReadInput：1フレーム分の入力状態をまとめて取得
//   - マウス移動量/スクロール量はここで一括取得して他の処理とブレさせない
//==============================================================================
CameraInputState CameraControllerComponent::ReadInput() const
{
    CameraInputState s{};
    s.lmb = Input::GetMouseButton(MouseButton::Left);
    s.mmb = Input::GetMouseButton(MouseButton::Middle);
    s.rmb = Input::GetMouseButton(MouseButton::Right);
    s.alt = Input::GetKey(KeyCode::LeftAlt) || Input::GetKey(KeyCode::RightAlt);
    s.ctrl = Input::GetKey(KeyCode::LeftControl) || Input::GetKey(KeyCode::RightControl);

    // マウス移動量（X/Y）…同一タイミングで取得（別々に読むとズレる可能性）
    auto md = Input::GetMouseDelta();
    s.dx = md.x;
    s.dy = md.y;

    // ホイール（正/負で前後）
    s.wheel = Input::GetMouseScrollDelta();
    return s;
}

//==============================================================================
// HandlePan（MMB + ドラッグ）
//   - 画面に平行にカメラを移動（right/up 方向へ）
//   - Windows座標系では“上へ動かすと dy < 0”なので、+up へは -dy を加える
//==============================================================================
bool CameraControllerComponent::HandlePan(const CameraInputState& in)
{
    if (!(in.mmb && !in.alt)) return false; // Alt+MMB はここでは扱わない

    XMVECTOR pos = XMLoadFloat3(&m_Transform->Position);
    XMVECTOR right = m_Transform->GetRightVector();
    XMVECTOR fwd = m_Transform->GetForwardVector();

    // 画面の“上”は forward × right（左手系）
    XMVECTOR up = XMVector3Cross(fwd, right);
    if (XMVector3Less(XMVector3LengthSq(up), XMVectorReplicate(1e-6f))) {
        // 万一 forward と right が並行で up が求められない場合の保険
        up = XMVectorSet(0.f, 1.f, 0.f, 0.f);
    }
    else {
        up = XMVector3Normalize(up);
    }

    // ドラッグ量に応じて移動（感度は m_cfg.panSpeed）
    pos = XMVectorAdd(pos, XMVectorScale(right, +in.dx * m_cfg.panSpeed));
    pos = XMVectorAdd(pos, XMVectorScale(up, -in.dy * m_cfg.panSpeed));

    XMStoreFloat3(&m_Transform->Position, pos);
    return true;
}

//==============================================================================
// HandleOrbit（Alt + LMB + ドラッグ）
//   - pivot を中心に角度（yaw/pitch）を更新 → その角度から forward を再取得 → pivot から距離分引いて位置決定
//   - クリック開始フレームは視覚的に固定（m_orbitJustStarted）
//==============================================================================
bool CameraControllerComponent::HandleOrbit(const CameraInputState& in)
{
    if (!(in.alt && in.lmb) || !m_orbitActive)
        return false;

    if (m_orbitJustStarted) {
        m_orbitJustStarted = false;
        return true; // 始めの1フレは見た目を変えない
    }

    // ドラッグ累計 → 相対角
    m_orbitAccX += in.dx;
    m_orbitAccY += in.dy;

    const float newYaw = m_orbitYaw0 + m_orbitAccX * m_cfg.lookSpeed;              // 左右
    const float newPitch = std::clamp(m_orbitPitch0 + m_orbitAccY * m_cfg.lookSpeed,   // 上下（±89°でクランプ）
        -89.0f, 89.0f);

    // 1) Transform の回転のみを先に確定（Transform の forward を一貫して使う）
    m_Transform->Rotation = { newPitch, newYaw, 0.0f };

    // 2) forward を Transform から再取得（回転計算を一点化）
    XMVECTOR pivot = XMLoadFloat3(&m_orbitPivot);
    XMVECTOR forward = m_Transform->GetForwardVector();

    // 3) 位置 = pivot - forward * 距離
    XMVECTOR pos = XMVectorSubtract(pivot, XMVectorScale(forward, m_OrbitDist));
    XMStoreFloat3(&m_Transform->Position, pos);

    return true;
}

//==============================================================================
// HandleDolly（Alt + RMB + ドラッグ）
//   - pivot との距離を増減（右/下ドラッグで近づく符号設定）
//   - 最小距離は 0.01f にクリップ（0 や負で反転/ゼロ割れ防止）
//==============================================================================
bool CameraControllerComponent::HandleDolly(const CameraInputState& in)
{
    if (!(in.alt && in.rmb)) return false;

    // -in.dx - in.dy：右/下ドラッグを「+近づく」に
    m_OrbitDist = std::max(0.01f, m_OrbitDist + (-in.dx - in.dy) * m_cfg.dollySpeed);
    return true;
}

//==============================================================================
// HandleWheel（マウスホイール）
//   - カメラを forward 方向に移動（望遠的なFOV変更ではなく“位置移動”でズーム相当）
//==============================================================================
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
//   - ドラッグで yaw/pitch を更新して回転、キー入力で移動
//   - Ctrl で速度ブースト（flyBoost 倍）
//==============================================================================
bool CameraControllerComponent::HandleFly(const CameraInputState& in, float dt)
{
    if (!in.rmb) return false;

    // 累計ドラッグ → 相対角
    m_flyAccX += in.dx;
    m_flyAccY += in.dy;

    // yaw/pitch を基準角から更新（上下は±89°にクランプ）
    m_Yaw = m_flyYaw0 + m_flyAccX * m_cfg.lookSpeed;
    m_Pitch = std::clamp(m_flyPitch0 + m_flyAccY * m_cfg.lookSpeed, -89.0f, 89.0f);

    // Transform の回転を反映（ロールは常に 0）
    m_Transform->Rotation = { m_Pitch, m_Yaw, 0.0f };

    // 方向ベクトルは回転反映後の Transform から取得
    XMVECTOR pos = XMLoadFloat3(&m_Transform->Position);
    XMVECTOR forward = m_Transform->GetForwardVector();
    XMVECTOR right = m_Transform->GetRightVector();
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    // 速度（Ctrl でブースト）
    const float v = m_cfg.flySpeed * dt * (in.ctrl ? m_cfg.flyBoost : 1.0f);

    // 合成移動（斜め移動可）
    if (Input::GetKey(KeyCode::W)) pos = XMVectorAdd(pos, XMVectorScale(forward, +v));
    if (Input::GetKey(KeyCode::S)) pos = XMVectorAdd(pos, XMVectorScale(forward, -v));
    if (Input::GetKey(KeyCode::A)) pos = XMVectorAdd(pos, XMVectorScale(right, -v));
    if (Input::GetKey(KeyCode::D)) pos = XMVectorAdd(pos, XMVectorScale(right, +v));
    if (Input::GetKey(KeyCode::E)) pos = XMVectorAdd(pos, XMVectorScale(up, +v));
    if (Input::GetKey(KeyCode::Q)) pos = XMVectorAdd(pos, XMVectorScale(up, -v));

    XMStoreFloat3(&m_Transform->Position, pos);
    return true;
}
