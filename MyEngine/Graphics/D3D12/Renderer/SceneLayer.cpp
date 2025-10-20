// Renderer/SceneLayer.cpp
#include "Renderer/SceneLayer.h"

/*
    SceneLayer
    ----------------------------------------------------------------------------
    役割：
      - 「シーン描画の高レベル調整役」：下位の Viewports（RT/サイズ管理）と
        SceneRenderer（実描画コマンド生成）の橋渡しを行う。
      - UI（ImGui）側へ供給する SRV の窓口も担う。

    呼び出しモデルの目安（1フレーム）：
      1) BeginFrame()              ... UI起因のリサイズ確定を適用（RT再生成があれば旧RTを返す）
      2) Record(args, maxObjects)  ... Scene→Game の順でオフスクリーン描画を記録
      3) FeedToUI(ctx, imgui, ...) ... ImGui へ描画結果の SRV を供給
      4) （呼び出し側で Presenter.Begin → ImGui → Presenter.End）
      5) BeginFrame で返った旧RTは FrameScheduler.EndFrame(sig) 側で遅延破棄登録

    注意点：
      - リサイズ判定/ヒステリシス/偶数化は Viewports 側に集約（責務分離）
      - Scene と Game は同解像度で同期している前提（初回は Scene に同期）
      - UI へ渡す SRV スロットは外側（Renderer）がフレームインデックスでユニークに割当て
*/

void SceneLayer::Initialize(ID3D12Device* dev, DXGI_FORMAT /*rtvFmt*/, DXGI_FORMAT /*dsvFmt*/,
    FrameResources* frames, const PipelineSet& pipe,
    unsigned initW, unsigned initH)
{
    // SceneRenderer を結線（パイプライン/フレームUPL などの関連付け）
    m_sceneRenderer.Initialize(dev, pipe, frames);

    // Viewports（RT/サイズ管理）初期化：ウィンドウ初期サイズで作成することで
    // 初回の拡大時にぼやけない（小サイズ→拡大による一時的な縮小描画回避）
    m_viewports.Initialize(dev, initW, initH);
}

RenderTargetHandles SceneLayer::BeginFrame(ID3D12Device* dev)
{
    // UI 側で前フレーム中に安定化した希望サイズがあれば、ここで RT を作り直す。
    // 旧RT は「Detach したハンドル（ComPtr束）」で返すので、呼び出し側で
    // GpuGarbageQueue に（今フレの fence 値で）登録して遅延破棄すること。
    return m_viewports.ApplyPendingResizeIfNeeded(dev);
}

void SceneLayer::Record(const SceneLayerBeginArgs& a, unsigned maxObjects)
{
    // 防御：必要パラメータが欠けていたら何もしない（安全側）
    if (!a.camera || !a.scene || !a.cmd) return;

    // 1) Scene へ描画
    //    - HFOV（横FOV）を基準に、アスペクト変化に追従する投影を Viewports 側で調整
    //    - View/Proj の選定や CB 設定は SceneRenderer.Record 内で実行
    m_viewports.RenderScene(a.cmd, m_sceneRenderer, a.camera, a.scene, a.frameIndex, maxObjects);

    // 2) Game へ描画
    //    - 初回は Scene と同期した固定カメラ（View/Proj）で静的表示
    //    - Scene 側の投影/アスペクトに合わせて Game も作られる（初回同期）
    m_viewports.RenderGame(a.cmd, m_sceneRenderer, a.scene, a.frameIndex, maxObjects);
}

void SceneLayer::FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
    unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase)
{
    // ImGui へ描画結果（RT の Color）の SRV を供給。
    // - DX12 の ImTextureID は「GPU 可視 SRV ディスクリプタハンドル（UINT64）」互換
    // - フォントは slot=0 を使う前提なので、sceneSrvBase/gameSrvBase は 1 以上で
    //   各フレーム（frameIndex）に応じたユニークなスロット割当をすること。
    m_viewports.FeedToUI(ctx, imgui, frameIndex, sceneSrvBase, gameSrvBase);
}

void SceneLayer::RequestResize(unsigned wantW, unsigned wantH, float dt)
{
    // ImGui 側で測った “Scene ウィンドウ内の利用可能サイズ” を Viewports へ伝える。
    // Viewports 側で：
    //   - ステップスナップ/偶数化
    //   - 一定時間の安定（ヒステリシス）
    //   - pending への確定
    // をまとめて面倒を見る。ここではただ渡すだけ。
    m_viewports.RequestSceneResize(wantW, wantH, dt);
}

// 追加実装：UI 側の Stats 表示など「元情報（真のRTサイズ）」の同期に使う。
void SceneLayer::SyncStatsTo(EditorContext& ctx) const
{
    // 真のソース：Viewports が保持する現在の RT 実サイズ
    ctx.sceneRTWidth = m_viewports.SceneWidth();
    ctx.sceneRTHeight = m_viewports.SceneHeight();
    ctx.gameRTWidth = m_viewports.GameWidth();
    ctx.gameRTHeight = m_viewports.GameHeight();

    // もし EditorContext::rtWidth/rtHeight を「Scene RT のミラー」として扱いたいなら
    // 下記を有効化（現状はスワップチェインサイズ表示用途なのでコメントアウト）
    // ctx.rtWidth  = ctx.sceneRTWidth;
    // ctx.rtHeight = ctx.sceneRTHeight;
}

void SceneLayer::Shutdown()
{
    // 安全側：この場で RT を「Detach」して参照を解放しておく
    // （ここでは返却先がないため即時破棄にはならない。シャットダウン順で
    //  GPU 完了待ち→遅延破棄キュー全 flush を済ませておくこと）
    (void)m_viewports.SceneRT().Detach();
    (void)m_viewports.GameRT().Detach();

    // SceneRenderer の結線を切る（nullptr で無効化）
    m_sceneRenderer.Initialize(nullptr, PipelineSet{}, nullptr);
}

RenderTargetHandles SceneLayer::TakeCarryOverDead() {
    // Viewports 側が保持している「持ち越し死骸（2個目以降の旧RT）」を回収。
    // 呼び出し側（Renderer）で「今フレームで捨てる枠」が空ならこれを使う。
    return m_viewports.TakeCarryOverDead();
}
