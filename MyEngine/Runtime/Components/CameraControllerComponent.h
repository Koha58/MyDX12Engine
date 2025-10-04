#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <DirectXMath.h>
#include "Component.h"

// ─────────────────────────────────────────────────────────────────────────────
// Forward Declarations（heavy include を避けてビルド時間短縮）
// ─────────────────────────────────────────────────────────────────────────────
class GameObject;
class CameraComponent;
class TransformComponent;

// ============================================================================
// CameraInputState
//  - 1フレームにおける入力の“事実”をスナップショットとして集約。
//  - Update の先頭で ReadInput() により構築 → 各ハンドラへ値渡し。
//  - 「入力取得」と「動作ロジック」を分離して、デバッグやテストを容易にする。
// ============================================================================
struct CameraInputState
{
    // ── Buttons / Modifiers ────────────────────────
    bool  lmb = false;  // Left Mouse Button   … オービット（Alt 併用時）
    bool  mmb = false;  // Middle Mouse Button … パン
    bool  rmb = false;  // Right Mouse Button  … フライ（回転＋移動）
    bool  alt = false;  // Alt                 … オービット/ドリーの修飾キー
    bool  ctrl = false;  // Ctrl                … フライ移動の加速修飾キー

    // ── Mouse delta / Wheel ────────────────────────
    float dx = 0.0f; // マウス移動量 X（px / frame）  +:右 / -:左
    float dy = 0.0f; // マウス移動量 Y（px / frame）  +:下 / -:上（※Win 準拠）
    float wheel = 0.0f; // スクロール量（実装依存：+前 / -後 を想定）
};

// ============================================================================
// CameraTuning
//  - すべての感度・速度を一箇所に集約。外部UIからの調整も容易。
//  - 単位：lookSpeed は “度/px”、他は “ワールド単位/px or 秒”。
// ============================================================================
struct CameraTuning
{
    float lookSpeed = 0.15f; // 視線回転感度（度/px）
    float panSpeed = 0.01f; // パン速度（ワールド/px）
    float dollySpeed = 0.10f; // Alt+RMB ドリー感度（距離/px）
    float wheelSpeed = 0.50f; // ホイールドリー（距離/ノッチ）
    float flySpeed = 3.0f;  // フライ基本速度（units/sec）
    float flyBoost = 3.0f;  // Ctrl での加速倍率
};

// ============================================================================
// CameraControllerComponent
//  - Unity ライクなフリーカメラ操作を提供するコンポーネント。
//  - Transform を直接書き換え（Camera は Transform から View を組む想定）。
//  - 左手系・オイラー角（deg）：Rotation = {Pitch(X), Yaw(Y), Roll(Z)} を採用。
//  - ImGui がマウスを占有中（io.WantCaptureMouse==true）は一切操作しない。
// ----------------------------------------------------------------------------
// 【操作一覧（Unity準拠の感覚）】
//   マウス：
//     ・MMB（中ボタン）+ドラッグ    … パン（左右・上下）
//     ・RMB（右ボタン）+ドラッグ    … 視点の回転（フライモード）
//     ・Alt + LMB（左ボタン）+ドラッグ … オービット（画面中央＝注視点を中心に回転）
//     ・ホイール                     … ドリー（前後移動；FOVではなく位置移動）
//   キー（フライ中 = RMB押下中に有効）:
//     ・W / S  … 前進 / 後退（ローカル forward）
//     ・A / D  … 左 / 右（ローカル right）
//     ・Q / E  … 下降 / 上昇（ワールド up）
//     ・Ctrl   … 速度ブースト（flyBoost 倍）
// ----------------------------------------------------------------------------
// 【処理フロー】（各フレーム）
//   1) 入力を ReadInput() でスナップショット化
//   2) モード優先度順に処理（パン → オービット → ドリー → ホイール → フライ）
//      ※ どれかのモードが動いたらそのフレームは return（競合防止）
// ----------------------------------------------------------------------------
// 【設計メモ】
//   ・押下直後の“姿勢が跳ぶ”現象を防ぐため、押したフレームは姿勢を変更しない
//     （*JustStarted フラグでスキップ / 押下時点の Transform を保存＆即時復元）。
//   ・左右/上下の反転は HandleOrbit / HandleFly の yaw/pitch 計算の符号で調整可能。
// ============================================================================
class CameraControllerComponent : public Component
{
public:
    // ─────────────────────────────────────────────────────────────────────
    // ctor
    //   @owner  : 本コンポーネントを載せる GameObject（Transform をキャッシュ）
    //   @camera : 操作対象カメラ（FOV 等の参照用。姿勢は Transform が主）
    // ─────────────────────────────────────────────────────────────────────
    CameraControllerComponent(GameObject* owner, CameraComponent* camera);

    // ─────────────────────────────────────────────────────────────────────
    // Update（毎フレーム）
    //   1) 入力収集（ReadInput）
    //   2) 優先順モード処理（パン→オービット→ドリー→ホイール→フライ）
    //   3) どれかが処理したら早期 return（モード競合防止）
    // ─────────────────────────────────────────────────────────────────────
    void Update(float deltaTime) override;

    // デバッグログ用（任意）
    void OnEnable() override;
    void OnDisable() override;

    // ─────────────────────────────────────────────────────────────────────
    // Tuning アクセス（外部UIから感度・速度を調整したい場合に）
    // ─────────────────────────────────────────────────────────────────────
    CameraTuning& Tuning() { return m_cfg; }
    const CameraTuning& Tuning() const { return m_cfg; }

private:
    // === 入力のスナップショット化 =================================================
    CameraInputState ReadInput() const;

    // === 各モード処理（true を返したら今フレームは処理完了）====================
    bool HandlePan(const CameraInputState& in);           // MMB：画面平行移動
    bool HandleOrbit(const CameraInputState& in);           // Alt+LMB：注視点中心の回転
    bool HandleDolly(const CameraInputState& in);           // Alt+RMB：注視点までの距離変更
    bool HandleWheel(const CameraInputState& in);           // Wheel：前後ドリー
    bool HandleFly(const CameraInputState& in, float dt); // RMB(+WASD/QE)：フライ

private:
    // ── Orbit 用内部状態（Alt+LMB）───────────────────────────────────────
    bool                  m_prevAltLmb = false;           // Alt+LMB の前フレーム状態
    bool                  m_orbitActive = false;           // オービット中
    bool                  m_orbitJustStarted = false;           // 押下直後1Fは見た目固定
    DirectX::XMFLOAT3     m_orbitPivot = { 0,0,0 };         // 回転の中心（固定）
    float                 m_orbitYaw0 = 0.0f;            // 押下時の基準 Yaw（deg）
    float                 m_orbitPitch0 = 0.0f;            // 押下時の基準 Pitch（deg）
    float                 m_orbitAccX = 0.0f;            // 押下以降の累計ドラッグ X
    float                 m_orbitAccY = 0.0f;            // 押下以降の累計ドラッグ Y

    // ── Fly 用内部状態（RMB）───────────────────────────────────────────
    bool                  m_flyPrevRmb = false;           // RMB の前フレーム状態
    bool                  m_flyJustStarted = false;           // 押下直後1Fは見た目固定
    float                 m_flyYaw0 = 0.0f;            // 押下時の基準 Yaw（deg）
    float                 m_flyPitch0 = 0.0f;            // 押下時の基準 Pitch（deg）
    float                 m_flyAccX = 0.0f;            // 押下以降の累計ドラッグ X
    float                 m_flyAccY = 0.0f;            // 押下以降の累計ドラッグ Y

    // ── 押下時の Transform バックアップ（“押した瞬間に姿勢が飛ぶ”対策）──────────
    DirectX::XMFLOAT3     m_pressPos{};                         // 押下時の Position
    DirectX::XMFLOAT3     m_pressRot{};                         // 押下時の Rotation（Pitch,Yaw,Roll）

private:
    // ── 外部参照先（所有はしない）────────────────────────────────────────
    CameraComponent* m_Camera = nullptr; // 操作対象カメラ
    TransformComponent* m_Transform = nullptr; // 姿勢を更新する Transform

    // ── 姿勢パラメータ（degや距離）──────────────────────────────────────
    float                 m_Yaw = 0.0f;    // 水平回転（deg）
    float                 m_Pitch = 0.0f;    // 垂直回転（deg）※実装側で ±89° にクランプ
    float                 m_OrbitDist = 5.0f;    // オービット距離（pivot からの距離）

    // Transform → 角度同期が必要なら利用（未使用でも可）
    bool                  m_InitSynced = false;

    // ── 感度・速度のチューニング値 ───────────────────────────────────────
    CameraTuning          m_cfg{};
};
