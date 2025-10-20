#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgiformat.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "imgui_internal.h"

/*
    RenderTarget.h
    ----------------------------------------------------------------------------
    役割：
      - オフスクリーン描画用の Color/Depth テクスチャと、その RTV/DSV をひとまとめに管理。
      - 作成(Create)／解放(Release)／サイズ変更(Resize)／状態遷移(Transition*)／
        バインド/クリア(Bind/Clear) といった一連の操作を提供。
      - ImGui へ表示するための SRV（ImTextureID）を、外部ヒープ（ImGuiLayer）に作成。

    設計のポイント：
      - RenderTargetDesc…必要十分な作成パラメータ（w/h/format/クリア値）を保持。
      - 内部状態トラッキング…Color の直近の D3D12_RESOURCE_STATE を保持し、
        不要なバリアを抑制（冪等な TransitionToRT/TransitionToSRV）。
      - Detach() … このインスタンスの所有リソースを RenderTargetHandles に引き剥がして
        返す（自分は“空”に）。GPU 完了待ちの遅延破棄キューへ載せる用途を想定。
      - ImGui SRV … DX12 の ImTextureID は GPU 可視 SRV ハンドル（UINT64相当）。
        SRV 自体は ImGuiLayer 側のヒープに作るため、本クラスは ID のキャッシュのみを持つ。
*/

class ImGuiLayer; // SRV 作成を委譲するレイヤ（前方宣言）

// ----------------------------------------------------------------------------
// RenderTargetDesc：RT 作成パラメータ
// ----------------------------------------------------------------------------
struct RenderTargetDesc
{
    UINT        width = 0;                                      // テクスチャ幅（px）
    UINT        height = 0;                                      // テクスチャ高さ（px）
    DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;        // Color の DXGI フォーマット
    DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;             // Depth の DXGI フォーマット（不要なら UNKNOWN を指定）
    float       clearColor[4] = { 0, 0, 0, 1 };                  // RTV クリア色（Create 時の最適化値にも使用）
    float       clearDepth = 1.0f;                               // DSV クリア深度
};

// ----------------------------------------------------------------------------
// RenderTargetHandles：Detach() で外へ“所有権移譲”する束
//   - GpuGarbageQueue にフェンスと紐付けて遅延破棄するケースで使用。
//   - CPU ディスクリプタハンドルはコピーで持ち出し、ComPtr は move で所有権を渡す。
// ----------------------------------------------------------------------------
struct RenderTargetHandles {
    Microsoft::WRL::ComPtr<ID3D12Resource>       color;          // Color テクスチャ
    Microsoft::WRL::ComPtr<ID3D12Resource>       depth;          // Depth テクスチャ（任意）
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;        // RTV 用の CPU 可視ヒープ（1エントリ）
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;        // DSV 用の CPU 可視ヒープ（1エントリ）
    D3D12_CPU_DESCRIPTOR_HANDLE                  rtv{};          // RTV の CPU ハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE                  dsv{};          // DSV の CPU ハンドル
};

// ----------------------------------------------------------------------------
// RenderTarget：Color/Depth + RTV/DSV をまとめて面倒を見るユーティリティ
// ----------------------------------------------------------------------------
class RenderTarget
{
public:
    /*
        Create
        ------------------------------------------------------------------------
        役割 : RenderTargetDesc に従って Color/Depth リソースと RTV/DSV を生成。
        前提 : dev != nullptr, width/height > 0
        注意 : 初期状態は Color=RENDER_TARGET, Depth=DEPTH_WRITE（クリア直後に使用可）。
               sRGB が必要な場合は、フォーマット整合やパイプライン側で統一すること。
    */
    bool Create(ID3D12Device* dev, const RenderTargetDesc& d);

    /*
        Release
        ------------------------------------------------------------------------
        役割 : 所有している Color/Depth と各ディスクリプタヒープを解放し、空状態に戻す。
        注意 : “所有権を外へ渡さない”破棄。GPU で使用中の可能性があるなら Detach() を使い、
               破棄は GpuGarbageQueue に委譲すること。
    */
    void Release();

    /*
        Resize
        ------------------------------------------------------------------------
        役割 : 現在の RT を Release()→新しいサイズで Create() し直す。
        戻り : 成功/失敗
        注意 : 呼び出し側でフェンス完了を待ってから行うこと（安全なタイミングで呼ぶ前提）。
    */
    bool Resize(ID3D12Device* dev, UINT w, UINT h);

    // ------------------------------------------------------------------------
    // パス先頭/末尾でのユーティリティ：内部状態トラッキング付きの冪等バリア
    // ------------------------------------------------------------------------

    // OMSetRenderTargets / Clear 前に呼ぶ：Color → RENDER_TARGET
    void TransitionToRT(ID3D12GraphicsCommandList* cmd);

    // シェーダ読み出し用途で使う前に呼ぶ：Color → PIXEL_SHADER_RESOURCE
    void TransitionToSRV(ID3D12GraphicsCommandList* cmd);

    // 現在の RTV/DSV を OM バインド（DSV 無ければ nullptr）
    void Bind(ID3D12GraphicsCommandList* cmd);

    // RTV/DSV をクリア（Color は m_desc.clearColor、Depth は m_desc.clearDepth）
    void Clear(ID3D12GraphicsCommandList* cmd);

    // ------------------------------------------------------------------------
    // ImGui 表示
    // ------------------------------------------------------------------------
    /*
        EnsureImGuiSRV
        役割 : ImGuiLayer を介して Color リソースの SRV を（指定スロットに）作成/更新し、
               ImTextureID（=GPU 可視 SRV ハンドル）を返す。
        注意 : 本クラスは SRV ヒープを持たない。SRV 自体は ImGuiLayer 側に作成。
               'const' メソッドだが ID キャッシュを書き換えるため m_imguiTexId は mutable。
    */
    ImTextureID EnsureImGuiSRV(ImGuiLayer* imgui, UINT slot) const;

    // ------------------------------------------------------------------------
    // 参照系（安全側：Color が無ければ Width/Height は 0 を返すのが理想だが
    //         本実装では m_desc を真とする。使用側は Color() の null を併せて確認を）
    // ------------------------------------------------------------------------
    UINT         Width()       const { return m_desc.width; }
    UINT         Height()      const { return m_desc.height; }
    DXGI_FORMAT  ColorFormat() const { return m_desc.colorFormat; }
    ID3D12Resource* Color()    const { return m_color.Get(); }

    /*
        Detach
        ------------------------------------------------------------------------
        役割 : Color/Depth/各ヒープと CPU ハンドルを RenderTargetHandles として“引き剥がし”返す。
               このインスタンス側は完全に“空”へ戻る（SRV ID も無効化）。
        用途 : リサイズ時など、古い RT を GPU 完了後に破棄したい場合に、
               GpuGarbageQueue へ積むための所有権移譲に使用。
    */
    RenderTargetHandles Detach();

private:
    // 作成パラメータ（Resize 時にも再利用）
    RenderTargetDesc m_desc{};

    // 本体リソース
    Microsoft::WRL::ComPtr<ID3D12Resource>            m_color;   // 必須：Color テクスチャ
    Microsoft::WRL::ComPtr<ID3D12Resource>            m_depth;   // 任意：Depth テクスチャ

    // ディスクリプタヒープ（CPU 可視）
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_rtvHeap; // RTV 1 つだけ確保
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_dsvHeap; // DSV 1 つだけ確保

    // CPU ハンドル（OMSet/Clear 用）
    D3D12_CPU_DESCRIPTOR_HANDLE                       m_rtv{};
    D3D12_CPU_DESCRIPTOR_HANDLE                       m_dsv{};

    // ImGui 用 SRV の ID（DX12 では GPU 可視 SRV ハンドルが格納される）
    mutable ImTextureID                               m_imguiTexId = 0;

    // Color リソースの“現在状態”をトラック（冪等バリアのため）
    D3D12_RESOURCE_STATES m_colorState = D3D12_RESOURCE_STATE_COMMON;
};
