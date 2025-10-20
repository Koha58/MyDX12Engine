#include "FrameResources.h"
#include "d3dx12.h"

using Microsoft::WRL::ComPtr;

/*
    FrameResources
    ----------------------------------------------------------------------------
    役割：
      ・フレームごとに必要な「コマンドアロケータ」と「アップロード用CBバッファ(UPL)」を保持。
      ・ダブル/トリプルバッファリング時は、各フレームインデックスに対して独立のリソースを持つ。
      ・UPL は「永続マップ」して CPU 書き込みポインタを保持（書き込み効率重視）。

    用語：
      - frameCount  : スワップチェインのフレーム数（例：2 or 3）
      - cbSize      : 1 オブジェクトが使う定数バッファのサイズ（バイト）
      - maxObjects  : 1 フレームで更新しうる最大オブジェクト数（スロット数）

    注意点：
      - CBV のハードアライメントは 256 バイト。cbSize は 256 の倍数に切り上げて使う。
      - UPL をフレーム数分確保し、各フレームが自分の領域にのみ書き込む設計。
      - Destroy() 時に Map を必ず Unmap してから Reset する。
*/

bool FrameResources::Initialize(ID3D12Device* dev, UINT frameCount, UINT cbSize, UINT maxObjects)
{
    // フレーム数（リングバッファの段数）
    m_count = frameCount;

    // ===== 重要：CBV の 256B アライメントに切り上げ =====
    // D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT (256)
    // 例) cbSize=160 → stride=256, cbSize=300 → stride=512
    m_cbStride = (cbSize + 255) & ~255u;

    // フレームごとのリソースセットを確保
    m_items.resize(frameCount);

    for (UINT i = 0; i < frameCount; ++i) {
        // --------------------------------------------------------------------
        // 1) コマンドアロケータ（DIRECT）
        //    ・各フレーム専用に 1 つずつ用意。
        //    ・再利用するときは Fence で完了を待ってから Reset するのが上位層の責務。
        // --------------------------------------------------------------------
        if (FAILED(dev->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_items[i].cmdAlloc))))
        {
            return false;
        }

        // --------------------------------------------------------------------
        // 2) 定数バッファ用のアップロードヒープ（フレーム i 用）
        //    ・「maxObjects × stride」分の連続領域を確保して、1フレームで使う全オブジェクト分をここに詰める。
        //    ・UPLOAD は CPU 書き込み可、GPU 読み取り可。毎フレ更新に向く。
        //    ・Default ヒープと違い、バリア管理は不要（ただし帯域は遅い）。
        // --------------------------------------------------------------------
        const UINT64 bytes = UINT64(m_cbStride) * UINT64(maxObjects);
        auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes);

        if (FAILED(dev->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, // UPL なので GENERIC_READ で常駐
            nullptr,                           // CB 用なので ClearValue は不要
            IID_PPV_ARGS(&m_items[i].resource))))
        {
            return false;
        }

        // --------------------------------------------------------------------
        // 3) 永続 Map
        //    ・UPL は「永続マップ」して OK（Unmap コスト/回数を減らす）。
        //    ・CD3DX12_RANGE(0,0) は CPU からの書き込みのみで、GPU 側への読み取り範囲通知は不要の意味。
        //    ・戻りポインタ(m_items[i].cpu)は "フレーム i の UPL の先頭" を指す。
        // --------------------------------------------------------------------
        CD3DX12_RANGE rr(0, 0); // 読み取り無し（書き込みだけ）
        if (FAILED(m_items[i].resource->Map(0, &rr, reinterpret_cast<void**>(&m_items[i].cpu))))
        {
            return false;
        }

        // Fence 値初期化（フレーム完了待ちの管理は上位で行う）
        m_items[i].fenceValue = 0;
    }

    return true;
}

void FrameResources::Destroy()
{
    // 生成済みフレーム分を逆順に片付け（順序は必須ではないが分かりやすいため）
    for (auto& it : m_items) {
        // Map している場合は Unmap してから解放（永続マップ解除）
        if (it.resource && it.cpu) {
            it.resource->Unmap(0, nullptr);
            it.cpu = nullptr; // 懸念：dangling pointer防止
        }

        // UPL リソースとアロケータを解放
        it.resource.Reset();
        it.cmdAlloc.Reset();

        // Fence 値も念のため初期化
        it.fenceValue = 0;
    }

    // ベクタを縮小し、全体のメタ情報もリセット
    m_items.clear();
    m_count = 0;
    m_cbStride = 0;
}
