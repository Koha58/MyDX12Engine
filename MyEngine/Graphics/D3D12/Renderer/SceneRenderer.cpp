#include "Renderer/SceneRenderer.h"
#include <algorithm>
#include <cmath>
#include <functional>

using Microsoft::WRL::ComPtr;

/*
    SceneRenderer::Record
    ----------------------------------------------------------------------------
    役割：
      - 与えられた RenderTarget に対してシーン全体を描画する。
      - 各オブジェクトの定数バッファ( b0 )をフレーム用アップロードバッファに書き込み、
        ルート CBV (slot=0) を都度差し替えながら Draw を積む。

    事前条件：
      - rt.Color() が有効（RTが作成済み）
      - cmd / m_frames（FrameResources）が有効
      - m_pipe.root（RootSignature）は Initialize 時に設定済み

    定数バッファのスロット割り当て：
      - cbBase …… 呼び出し側がこの描画パスに割り当てた “開始インデックス”
      - slot   …… この関数内で 0..(maxObjects-1) を消費し、dst = cbBase + slot に書く
      - 総スロット数は FrameResources 初期化時の maxObjects と一致させること
*/
void SceneRenderer::Record(ID3D12GraphicsCommandList* cmd,
    RenderTarget& rt,
    const CameraMatrices& cam,
    const Scene* scene,
    UINT cbBase,
    UINT frameIndex,
    UINT maxObjects)
{
    // --- 防御：最低限の依存関係が無い場合は何もしない ---
    if (!rt.Color() || !cmd || !m_frames) return;

    // ==============================
    // 1) 出力ターゲットの準備（RTV/クリア/ビューポート）
    // ==============================

    // 必要なら RT 状態へ遷移（内部で冪等チェック）
    rt.TransitionToRT(cmd);

    // OM に RTV/DSV をバインド（この関数では DSV を使わない構成）
    rt.Bind(cmd);

    // クリア（背景色/深度）
    rt.Clear(cmd);

    // ビューポート/シザーを RT サイズ全面へ
    {
        D3D12_VIEWPORT vp{ 0.f, 0.f, (float)rt.Width(), (float)rt.Height(), 0.f, 1.f };
        D3D12_RECT     sc{ 0, 0, (LONG)rt.Width(), (LONG)rt.Height() };
        cmd->RSSetViewports(1, &vp);
        cmd->RSSetScissorRects(1, &sc);
    }

    // トポロジ/ルートシグネチャ（PSO は呼び出し側でセット済み前提）
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->SetGraphicsRootSignature(m_pipe.root.Get());

    // ==============================
    // 2) シーンを走査して描画（深さ順/半透明等の順序はここでは未考慮）
    // ==============================
    if (scene)
    {
        // 現フレームのアップロード領域を取得
        auto& fr = m_frames->Get(frameIndex);
        UINT8* cbCPU = fr.cpu;                            // CPU書き込み先（Map済）
        D3D12_GPU_VIRTUAL_ADDRESS cbGPU = fr.resource->GetGPUVirtualAddress(); // GPU側ベース
        const UINT cbStride = m_frames->GetCBStride();    // 256B アライン済みサイズ
        UINT slot = 0;                                    // この描画パスで消費するローカルスロット

        using namespace DirectX;

        // ---- 再帰ラムダ：シーングラフ走査して MeshRenderer があれば描く ----
        std::function<void(std::shared_ptr<GameObject>)> draw =
            [&](std::shared_ptr<GameObject> go)
            {
                if (!go || slot >= maxObjects) return; // スロット上限で早期終了

                if (auto mr = go->GetComponent<MeshRendererComponent>())
                {
                    // VB/IB が有効で、描画インデックス数が正なら描く
                    if (mr->VertexBuffer && mr->IndexBuffer && mr->IndexCount > 0)
                    {
                        // 2.1) 行列計算：M, MVP, (M^-1)^T
                        XMMATRIX world = go->Transform->GetWorldMatrix();
                        XMMATRIX mvp = world * cam.view * cam.proj;

                        // 逆行列の健全性チェック（縮退で NaN/Inf になりうる）
                        XMVECTOR det;
                        XMMATRIX inv = XMMatrixInverse(&det, world);
                        float detScalar = XMVectorGetX(det);
                        if (!std::isfinite(detScalar) || std::fabs(detScalar) < 1e-8f) {
                            // 極端なスケール/縮退 → 法線が壊れるのでフォールバック
                            inv = XMMatrixIdentity();
                        }
                        XMMATRIX worldIT = XMMatrixTranspose(inv);

                        // 2.2) 定数バッファを組み立てて Upload
                        SceneConstantBuffer cb{};
                        XMStoreFloat4x4(&cb.mvp, mvp);
                        XMStoreFloat4x4(&cb.world, world);
                        XMStoreFloat4x4(&cb.worldIT, worldIT);
                        // 簡易ライト（上前方からの平行光源）
                        XMStoreFloat3(&cb.lightDir,
                            XMVector3Normalize(XMVectorSet(0.0f, -1.0f, -1.0f, 0.0f)));
                        cb.pad = 0.0f;

                        // 2.3) このオブジェクトの CBV スロット（cbBase 起点）
                        const UINT dst = cbBase + slot;

                        // CPU 側アップロードメモリへコピー（256B アラインを守る）
                        std::memcpy(cbCPU + (UINT64)dst * cbStride, &cb, sizeof(cb));

                        // ルート CBV を差し替え（b0）
                        cmd->SetGraphicsRootConstantBufferView(
                            0, cbGPU + (UINT64)dst * cbStride);

                        // 2.4) ジオメトリをバインドして Draw
                        cmd->IASetVertexBuffers(0, 1, &mr->VertexBufferView);
                        cmd->IASetIndexBuffer(&mr->IndexBufferView);
                        cmd->DrawIndexedInstanced(mr->IndexCount, 1, 0, 0, 0);

                        ++slot; // 次オブジェクトへ
                    }
                }

                // 子ノードを再帰処理
                for (auto& ch : go->GetChildren()) draw(ch);
            };

        // ルートから走査開始（※順序はシーンデータ依存。必要に応じてソートを追加）
        for (auto& root : scene->GetRootGameObjects()) draw(root);
    }

    // ==============================
    // 3) 出力を SRV 状態へ（UI 側がサンプルできるように）
    // ==============================
    rt.TransitionToSRV(cmd);
}

/*
【実装上のメモ / 落とし穴】
- FrameResources の cbStride は 256B アライン必須（D3D12 定数バッファ規約）。
  Initialize(cbSize) で (cbSize+255)&~255 になっていることを要確認。
- cbBase/slot と maxObjects：
  * cbBase は “この描画パスの先頭” を呼び出し側が管理（例：Scene=0..N-1, Game=N..2N-1）。
  * slot はこの関数のローカル。maxObjects を超えたら安全に打ち切る。
- PSO/RS/RootSig：
  * 本関数では RootSignature のみをセット。PSO セットは呼び出し側の責務。
  * 使う InputLayout/シェーダと VB/IB の stride/format が一致していること。
- Transform の逆行列：
  * 極端なスケール（0に近い）や非正則行列だと inv に NaN が出る。
    → det をチェックし、壊れていたら Identity へフォールバック（法線が崩れるのを回避）。
- 深度テスト：
  * 現状は DSV を Bind していない。深度を使う描画にするなら RenderTarget 側で
    DSV を持たせ、Bind() で RTV+DSV を設定する or 呼び出し側で適切に設定する。
*/
