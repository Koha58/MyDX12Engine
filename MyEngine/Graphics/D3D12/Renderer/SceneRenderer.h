#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "Core/RenderTarget.h"              // オフスクリーンRT管理（カラー/深度、RTV/DSV、遷移ユーティリティ）
#include "Core/FrameResources.h"            // フレームリング（Upload CB 等）
#include "Pipeline/PipelineStateBuilder.h"  // PipelineSet 定義（RootSignature / PSO）
#include "Scene/Scene.h"
#include "Scene/GameObject.h"
#include "Graphics/SceneConstantBuffer.h"   // HLSL に合わせた定数バッファ構造
#include "Components/MeshRendererComponent.h"

/*
    SceneRenderer.h
    ----------------------------------------------------------------------------
    目的：
      - 「1つのカメラ行列セット → 1枚の RenderTarget」へ描画コマンドを記録する最小の描画役。
      - RootSignature/PSO（PipelineSet）と、各フレームのアップロードCB（FrameResources）
        を受け取って保持し、与えられた Scene を順次描く。

    想定フロー（呼び出し側=Viewports/SceneLayer など）：
      1) Initialize(dev, pipe, frames)
         - 使用する PSO とフレームリング（CB）へのポインタを保持
      2) Record(cmd, rt, cam, scene, cbBase, frameIndex, maxObjects)
         - rt を RT 状態へ遷移 → バインド/クリア
         - cam(view/proj) と Scene から、GameObject/Component を辿ってメッシュを描画
         - 定数バッファは FrameResources 上の [cbBase .. cbBase+maxObjects-1] を使用

    注意点：
      - 「cbBase」は複数パスで同じ FrameResources を分割利用するためのオフセット。
        例）Scene パスに 0..N-1、Game パスに N..2N-1 を割り当てる設計。
      - 「frameIndex」はフレームリングのインデックス（BackBufferIndex と一致させると分かりやすい）。
      - Record 内では PSO/RootSignature をセットする想定。実際の PSO は呼び出し側で上書き可能。
      - RenderTarget は、Record 内の冒頭で RT への遷移/設定、末尾で SRV への遷移を行うユーティリティを
        呼び出す（オフスクリーン→ImGui 表示に備える）。
*/

// ===== 共通描画パス(1カメラ→1RT) =====
/** カメラ用行列束（LH: 左手系想定） */
struct CameraMatrices
{
    DirectX::XMMATRIX view;  ///< View 行列（ワールド→ビュー）
    DirectX::XMMATRIX proj;  ///< Projection 行列（ビュー→クリップ）
};

/**
 * @brief シーン描画の薄いファサード。
 *        与えられた RenderTarget に対し、Scene 内の MeshRenderer を順に描く。
 */
class SceneRenderer
{
public:

    /**
     * @brief 使用する PipelineSet と FrameResources を関連付ける。
     * @param dev     （未使用。将来の拡張で RootSig 生成などが必要になった場合に備え保持）
     * @param pipe    ルートシグネチャ/PSO のセット
     * @param frames  フレームリング（アップロード定数バッファやコマンドアロケータ群）
     *
     * @note 現状 dev は使用していないが、Initialize の統一インターフェースとして受けておく。
     */
    void Initialize(ID3D12Device* /*dev*/, const PipelineSet& pipe, FrameResources* frames)
    {
        m_pipe = pipe;
        m_frames = frames;
    }

    /**
     * @brief 1 カメラ → 1 RenderTarget へ描画コマンドを記録する。
     *
     * @param cmd         記録先コマンドリスト（DIRECT）
     * @param rt          描画対象の RenderTarget（オフスクリーン）
     * @param cam         カメラ行列（view/proj）
     * @param scene       描画対象 Scene（ルートから再帰巡回）
     * @param cbBase      FrameResources 上の定数バッファスロットの開始オフセット
     * @param frameIndex  フレームリングのインデックス（BackBufferIndex に対応）
     * @param maxObjects  このパスで確保してよい CB スロット数（ガード用）
     *
     * @details
     *   - 本メソッドの中で：
     *       1) rt.TransitionToRT(cmd) / Bind(cmd) / Clear(cmd) を呼ぶ
     *       2) VP/SC/IA/RS/RootSignature をセットし、Scene を辿って MeshRenderer を描く
     *       3) 定数バッファ（SceneConstantBuffer）を FrameResources の Upload 領域に書き込む
     *       4) 最後に rt.TransitionToSRV(cmd) で SRV readable に戻す
     *
     *   - cbBase と maxObjects により、1フレーム内で複数パス（Scene/Game 等）が
     *     同じ FrameResources を干渉なく使える。
     */
    void Record(ID3D12GraphicsCommandList* cmd,
        RenderTarget& rt,
        const CameraMatrices& cam,
        const Scene* scene,
        UINT cbBase,
        UINT frameIndex,
        UINT maxObjects);

private:
    PipelineSet     m_pipe{};        ///< ルートシグネチャ/PSO（Lambert 等）
    FrameResources* m_frames = nullptr; ///< フレームリング（Upload CB/コマンドアロケータ等）
};
