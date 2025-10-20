#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <vector>

/*
    FrameResources
    ----------------------------------------------------------------------------
    役割：
      - フレームインフライト数分（例：2～3）の “フレーム別リソース” をまとめて管理。
        * コマンドアロケータ（ID3D12CommandAllocator）
        * アップロード用の定数バッファ（1 リソースに N オブジェクトぶんを詰める）
        * CPU マップ先ポインタ（cb 書き込み用）
        * フレーム完了を識別するフェンス値

    使い方（典型）：
      1) Initialize(dev, frameCount, sizeof(SceneCB), maxObjects)
      2) フレーム先頭で current = Get(frameIndex)
         - current.cmdAlloc->Reset()
         - cmdList->Reset(current.cmdAlloc.Get(), …)
      3) 定数バッファは current.cpu + (objectIndex * GetCBStride()) に書き込む
         - GPU 側は current.resource->GetGPUVirtualAddress() + (objectIndex * GetCBStride())
      4) Submit 後、取得したフェンス値を current.fenceValue に記録
      5) 次回同じ frameIndex を使う前に fence の完了を待つ

    注意点：
      - cbStride は 256 バイト境界に揃える（D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT）
      - Upload リソースは常時マップ（Map once → Unmap never）の運用を想定
*/

struct FrameItem {
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;  // このフレーム専用のコマンドアロケータ
    Microsoft::WRL::ComPtr<ID3D12Resource>         resource;  // Upload CB（連続領域に maxObjects ぶん）
    UINT8* cpu = nullptr; // 常時マップ先（nullptr のとき未マップ）
    UINT64                                         fenceValue = 0; // このフレームの完了を示すフェンス値
};

class FrameResources {
public:
    FrameResources() = default;

    /*
        Initialize
        ------------------------------------------------------------------------
        @param dev         : D3D12 デバイス
        @param frameCount  : フレームインフライト数（バックバッファ数に一致させるのが一般的）
        @param cbSize      : 1 オブジェクトぶんの定数バッファ struct サイズ（例：sizeof(SceneCB)）
        @param maxObjects  : 1 フレーム内で想定する最大オブジェクト数（スロット数）
        戻り値：成功で true
        効能  ：各フレームにつき CommandAllocator と “maxObjects × cbStride” サイズの
                Upload バッファを確保し、Map して cpu に保持する。
    */
    bool Initialize(ID3D12Device* dev, UINT frameCount, UINT cbSize, UINT maxObjects);

    /*
        Destroy
        ------------------------------------------------------------------------
        - 保持リソースを破棄。Map を明示的に Unmap する必要はない（安全側で Unmap しても OK）。
        - アロケータや Upload バッファ、CPU ポインタをクリア。
    */
    void Destroy();

    // ------------------------- アクセサ -------------------------
    UINT GetCount() const { return m_count; }

    // 1 オブジェクト当たりの CB ストライド（256 バイト境界に揃えた値）
    UINT GetCBStride() const { return m_cbStride; }

    // 現在フレームのハンドル取得（書き込み／読み出し）
    FrameItem& Get(UINT idx) { return m_items[idx]; }
    const FrameItem& Get(UINT idx) const { return m_items[idx]; }

private:
    std::vector<FrameItem> m_items; // frameCount 要素ぶん
    UINT m_count = 0;             // = frameCount
    UINT m_cbStride = 0;            // = Align(cbSize, 256)
};
