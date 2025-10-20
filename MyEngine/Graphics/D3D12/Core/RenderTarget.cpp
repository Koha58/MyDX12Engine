#include "RenderTarget.h"
#include "d3dx12.h"
#include "Editor/ImGuiLayer.h" 

// ----------------------------------------------------------------------------
// 目的：あとでクラッシュ解析/寿命追跡しやすいよう、RTがどの瞬間/状態にあるかを出す。
//       （軽量ログ。必要なら本格ロガーに置き換え）
// ----------------------------------------------------------------------------
#include <sstream>
static void RT_LogPtr(const char* tag, ID3D12Resource* color, ID3D12Resource* depth)
{
    std::ostringstream oss;
    oss << "[RT][" << tag << "] color=" << color << " depth=" << depth << "\n";
    OutputDebugStringA(oss.str().c_str());
}


// ----------------------------------------------------------------------------
// DX_CALL：HRESULT をチェックして即 return false。RenderTarget.* は bool を返す設計。
//           ※本番コードでは例外 or ログ付エラーコード返却などに置き換えてOK。
// ----------------------------------------------------------------------------
#ifndef DX_CALL
#define DX_CALL(x) do {                                           \
    HRESULT _hr = (x);                                            \
    if (FAILED(_hr)) {                                            \
        char _buf[256];                                           \
        sprintf_s(_buf, "%s failed (hr=0x%08X)\n", #x, (unsigned)_hr); \
        OutputDebugStringA(_buf);                                  \
        return false;                                             \
    }                                                             \
} while (0)
#endif


// ============================================================================
// Create
// 役割：RenderTargetDesc(d) に従って Color/Depth リソースと RTV/DSV ヒープを生成。
// 前提：dev != nullptr, d.width/height > 0
// 注意：初期状態は COLOR=RENDER_TARGET, DEPTH=DEPTH_WRITE にしておく（Clear直後に使える）。
//       sRGB が必要ならフォーマットやパイプライン側で統一する（BackBufferと混同注意）。
// ============================================================================
bool RenderTarget::Create(ID3D12Device* dev, const RenderTargetDesc& d)
{
    if (!dev) return false;
    if (d.width == 0 || d.height == 0) return false; // ★0サイズ作成禁止（不正アクセス防止）

    // 設定を保持（Resize で使い回す）
    m_desc = d;

    // --- RTV heap（Color用） ---
    {
        D3D12_DESCRIPTOR_HEAP_DESC r{};
        r.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        r.NumDescriptors = 1;                                 // 単一RTV
        r.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;            // CPU専用
        DX_CALL(dev->CreateDescriptorHeap(&r, IID_PPV_ARGS(&m_rtvHeap)));
        m_rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    }

    // ★ Color/Depth の両方で使う「共通」ヒーププロパティを外側に出す
    CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);      // GPUローカル

    // --- Color テクスチャ本体 ---
    {
        // Tex2D(desc, width, height, arraySize=1, mipLevels=1)
        auto rd = CD3DX12_RESOURCE_DESC::Tex2D(d.colorFormat, d.width, d.height);
        rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;   // RTV として使用

        // Clear時の最適化用値（レンダーターゲットはColorをセット）
        D3D12_CLEAR_VALUE c{};
        c.Format = d.colorFormat;
        memcpy(c.Color, d.clearColor, sizeof(float) * 4);

        // 初期状態を RENDER_TARGET にしておくと、そのまま Clear/Bind できる
        DX_CALL(dev->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_RENDER_TARGET, &c, IID_PPV_ARGS(&m_color)));

        // RTV 作成（デフォルト記述子）
        dev->CreateRenderTargetView(m_color.Get(), nullptr, m_rtv);
        m_colorState = D3D12_RESOURCE_STATE_RENDER_TARGET; // 内部状態を合わせる（バリア計画の起点）
    }

    // --- Depth（必要な場合のみ）---
    if (d.depthFormat != DXGI_FORMAT_UNKNOWN) {
        // DSV heap（CPU専用）
        D3D12_DESCRIPTOR_HEAP_DESC dh{};
        dh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dh.NumDescriptors = 1;
        DX_CALL(dev->CreateDescriptorHeap(&dh, IID_PPV_ARGS(&m_dsvHeap)));
        m_dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

        // Depth テクスチャ本体
        auto dd = CD3DX12_RESOURCE_DESC::Tex2D(d.depthFormat, d.width, d.height);
        dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE dc{};
        dc.Format = d.depthFormat;
        dc.DepthStencil.Depth = d.clearDepth;    // 既定 1.0
        // Stencil を使う場合は .Stencil も適切に

        DX_CALL(dev->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &dd,     // ← 共通の hp をそのまま利用
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &dc, IID_PPV_ARGS(&m_depth)));

        // DSV 作成
        dev->CreateDepthStencilView(m_depth.Get(), nullptr, m_dsv);
    }

#ifdef _DEBUG
    // 可視化時のヒント名（VS/PIX 等で辿りやすく）
    if (m_color) m_color->SetName(L"RenderTarget.Color");
    if (m_depth) m_depth->SetName(L"RenderTarget.Depth");
#endif

    RT_LogPtr("Create", m_color.Get(), m_depth.Get());

    // ImGui SRV の遅延作成に備えてIDをクリア
    m_imguiTexId = 0; // ※ヘッダ側で 'mutable' 指定がある前提（constメソッドから触るため）
    return true;
}


// ============================================================================
// Resize
// 役割：現在のRTを一旦 Release() し、同じ desc を新しい w/h で Create() し直す。
// 注意：GPU側で使用中の可能性がある場合は、呼び出し側でフェンス完了待ちを済ませてから。
//       （このメソッド自体は「安全な瞬間に呼ばれる」前提）
// ============================================================================
bool RenderTarget::Resize(ID3D12Device* dev, UINT w, UINT h)
{
    if (w == 0 || h == 0) return false;                          // 0サイズは無効
    if (w == m_desc.width && h == m_desc.height) return false;   // 実質 no-op

    RT_LogPtr("Resize.Begin", m_color.Get(), m_depth.Get());      // 旧リソースの生存確認に役立つ
    Release();                                                    // 参照を切る（ResizeBuffers 同様の作法）
    m_desc.width = w; m_desc.height = h;
    const bool ok = Create(dev, m_desc);                          // 再作成（初期状態/クリア値も復元）
    RT_LogPtr(ok ? "Resize.End.OK" : "Resize.End.FAIL", m_color.Get(), m_depth.Get());
    return ok;
}


// ============================================================================
// Release
// 役割：所有している Color/Depth と各ヒープを開放。内部状態を既定値に戻す。
// 注意：Detach() と違い、“ハンドルの所有権を外へ渡さない”破棄。
//       遅延破棄が必要なら Detach() → GpuGarbageQueue へ。
// ============================================================================
void RenderTarget::Release()
{
    // ImGui 側から参照されないよう SRV ID をクリア（SRV自体は ImGuiLayer 側のヒープ）
    m_imguiTexId = 0;

    // 深度 → RTV の順で解放（順序は厳密ではないが、追跡しやすく）
    m_depth.Reset();
    m_dsvHeap.Reset();

    m_color.Reset();
    m_rtvHeap.Reset();

    // CPU ハンドルも無効化しておく（Bind/Clear の誤用対策）
    m_rtv = {};
    m_dsv = {};

    // 以降のバリア計画の起点を COMMON に戻す（完全に未使用状態）
    m_colorState = D3D12_RESOURCE_STATE_COMMON;

    // ログ：解放後は常に null になっているはず
    RT_LogPtr("Release", m_color.Get(), m_depth.Get());
}


// ============================================================================
// TransitionToRT
// 役割：Color リソースを RENDER_TARGET 状態へ（OMSetRenderTargets/Clear 前に呼ぶ）
// 注意：内部で状態をトラッキング。連続遷移は抑制して不要なバリアを避ける。
// ============================================================================
void RenderTarget::TransitionToRT(ID3D12GraphicsCommandList* cmd)
{
    if (m_colorState == D3D12_RESOURCE_STATE_RENDER_TARGET) {
        return; // すでにRT状態なら何もしない（冪等）
    }
    RT_LogPtr("TransitionToRT", m_color.Get(), m_depth.Get());
    auto b = CD3DX12_RESOURCE_BARRIER::Transition(
        m_color.Get(), m_colorState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ResourceBarrier(1, &b);
    m_colorState = D3D12_RESOURCE_STATE_RENDER_TARGET; // 内部状態も更新
}


// ============================================================================
// TransitionToSRV
// 役割：Color リソースを PIXEL_SHADER_RESOURCE 状態へ（シェーダ読み出し用）
// 注意：SRV を作るのは ImGuiLayer 側（本クラスは“状態”だけ移行）。
// ============================================================================
void RenderTarget::TransitionToSRV(ID3D12GraphicsCommandList* cmd)
{
    if (m_colorState == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        return; // 既にPSRなら何もしない
    }
    RT_LogPtr("TransitionToSRV", m_color.Get(), m_depth.Get());
    auto b = CD3DX12_RESOURCE_BARRIER::Transition(
        m_color.Get(), m_colorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); // ★from は実状態
    cmd->ResourceBarrier(1, &b);
    m_colorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}


// ============================================================================
// Bind
// 役割：現在の RTV/DSV を OM にバインド。
// 前提：RenderTargetView/DepthStencilView は作成済み。リソースは RT 状態にしてから使う。
// ============================================================================
void RenderTarget::Bind(ID3D12GraphicsCommandList* cmd)
{
    RT_LogPtr("Bind", m_color.Get(), m_depth.Get());
    // DSV が無い場合は nullptr を渡す（Color のみ描画）
    cmd->OMSetRenderTargets(1, &m_rtv, FALSE, m_depth ? &m_dsv : nullptr);
}


// ============================================================================
// Clear
// 役割：RTV/DSV をクリア（初期化）。描画前に呼ぶことが多い。
// 前提：Color は RENDER_TARGET、Depth は DEPTH_WRITE 状態が望ましい（実装は遷移を強制しない）。
// ============================================================================
void RenderTarget::Clear(ID3D12GraphicsCommandList* cmd)
{
    RT_LogPtr("Clear", m_color.Get(), m_depth.Get());
    cmd->ClearRenderTargetView(m_rtv, m_desc.clearColor, 0, nullptr);
    if (m_depth) {
        cmd->ClearDepthStencilView(
            m_dsv, D3D12_CLEAR_FLAG_DEPTH, m_desc.clearDepth, 0, 0, nullptr);
    }
}


// ============================================================================
// EnsureImGuiSRV
// 役割：ImGuiLayer を介して Color リソースの SRV を（指定スロットで）作成/更新。
// 戻り：ImTextureID（DX12 では GPU 可視 SRV ディスクリプタハンドルを詰めた整数）
// 注意：このメソッドは 'const' だが m_imguiTexId を更新するため、ヘッダ側では 'mutable' を想定。
//       slot: フレームインデックスで衝突しない SRV スロット（フォントは通常 slot=0 予約）。 
// ============================================================================
ImTextureID RenderTarget::EnsureImGuiSRV(ImGuiLayer* imgui, UINT slot) const
{
    if (!m_imguiTexId) {
        // ImGuiLayer 側で SRV を作成（DX12 の ImTextureID=GPUハンドル）
        m_imguiTexId = imgui->CreateOrUpdateTextureSRV(m_color.Get(), m_desc.colorFormat, slot);
        RT_LogPtr("ImGuiSRV.Create", m_color.Get(), m_depth.Get());
    }
    // 既に作成済みならそのまま返す（ImGui 側では同じIDを使い回す）
    return m_imguiTexId;
}


// ============================================================================
// Detach
// 役割：所有している Color/Depth/各ヒープと CPUハンドルを RenderTargetHandles として“引き剥がし”返す。
//       → 呼び出し側（例：Viewports/Renderer）が GpuGarbageQueue に積んで「GPU完了後に破棄」できる。
// 注意：このインスタンス側は“空”状態になる（サイズ=0, state=COMMON, SRV ID リセット）。
// ============================================================================
RenderTargetHandles RenderTarget::Detach()
{
    // ログ：detach する“旧RT”のアドレスを吐く（クラッシュ時の追跡に有効）
    RT_LogPtr("Detach.Before", m_color.Get(), m_depth.Get());

    RenderTargetHandles out{};
    // ComPtr の所有権を out に移す（このRenderTargetからは切り離される）
    out.color = std::move(m_color);
    out.depth = std::move(m_depth);
    out.rtvHeap = std::move(m_rtvHeap);
    out.dsvHeap = std::move(m_dsvHeap);
    // CPU ハンドルはコピー（ハンドル自体は軽量。外側でOMSet時に使える）
    out.rtv = m_rtv;
    out.dsv = m_dsv;

    // 自身は完全に“空”へ戻す
    m_rtv = {};
    m_dsv = {};
    m_imguiTexId = 0;                              // ImGui 側から参照されないように
    m_colorState = D3D12_RESOURCE_STATE_COMMON;    // バリア計画の起点へ
    m_desc.width = m_desc.height = 0;              // 0x0 として扱う（誤用検出に役立つ）

    // ログ：detach 後に out に移ったアドレスも出す（キューの寿命追跡が楽になる）
    RT_LogPtr("Detach.After(out)", out.color.Get(), out.depth.Get());
    return out;
}
