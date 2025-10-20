#pragma once
#include <memory>

/*
===============================================================================
 Component 基底クラス
-------------------------------------------------------------------------------
役割
- すべてのゲーム用コンポーネント（Transform, MeshRenderer, Camera など）の共通土台。
- Unity の MonoBehaviour ライクなライフサイクル（Awake/Start/Update…）を提供。
- 「どの GameObject に属するか」の所有関係を保持（循環参照は回避）。

設計ポリシー
- 所有関係：GameObject → Component は shared_ptr、Component → GameObject は weak_ptr。
  → 循環参照を避け、破棄フローを単純化。
- ライフサイクル呼び出し者の一貫性：
  * Awake/OnEnable …… AddComponent 時（または Scene への組み込み時）に実行する方針が一般的。
  * Start ……………… 最初の Update の直前に 1 度だけ。
- 有効/無効：SetEnabled() で OnEnable/OnDisable を自動発火（重複呼び出し防止）。
- スレッド：基本はメインスレッド前提（レンダラや入力と競合しないように）。
- 仮想デストラクタ：派生クラスの破棄時に正しくデストラクタが呼ばれるように必須。

使用ガイド
- 新しい機能は「派生クラス」を作って必要なライフサイクルだけ override する。
- 毎フレームの処理は Update / LateUpdate に分ける（依存順序を調整しやすい）。
- 描画がある場合は Render(D3D12Renderer*) を override。
- 一時的に無効化したい場合は SetEnabled(false) を使う（OnDisable が走る）。
===============================================================================
*/

// 前方宣言（ヘッダ依存を最小化）
class GameObject;

class Component
{
public:
    //-------------------------------------------------------------------------
    // コンポーネントの種類を識別（デバッグ/エディタ用途）
    // ※ 必要に応じて追加。MAX_COMPONENT_TYPES は配列サイズ確保などに使用可。
    //-------------------------------------------------------------------------
    enum ComponentType
    {
        None = 0,          // 汎用/未指定
        Transform,         // Transform コンポーネント
        MeshRenderer,      // MeshRenderer コンポーネント
        Camera,            // Camera コンポーネント
        MAX_COMPONENT_TYPES
    };

    //-------------------------------------------------------------------------
    // Ctor / Dtor
    //  - type は派生クラスから固定で渡す（例：Component(ComponentType::Camera)）
    //  - 仮想デストラクタ：派生破棄を正しく行うために必須
    //-------------------------------------------------------------------------
    explicit Component(ComponentType type) : m_Type(type) {}
    virtual ~Component() = default;

    // 自身の種類を取得（エディタ表示や型分岐に）
    ComponentType GetType() const { return m_Type; }

    //-------------------------------------------------------------------------
    // ライフサイクル（必要なものだけ override）
    //  - Awake      : 生成直後に一度だけ（参照の解決・初期化）
    //  - OnEnable   : 有効化時に都度（サブシステム登録など）
    //  - Start      : 最初の Update 直前に一度だけ（遅延初期化）
    //  - Update     : 毎フレーム（ゲームロジック）
    //  - LateUpdate : Update 後（追従/カメラなど順序依存に便利）
    //  - OnDisable  : 無効化時に都度（サブシステム登録解除など）
    //  - OnDestroy  : 破棄直前に一度だけ（GPU/OS リソース解放など）
    //-------------------------------------------------------------------------
    virtual void Awake() {}
    virtual void OnEnable() {}
    virtual void Start() {}
    virtual void Update(float /*deltaTime*/) {}
    virtual void LateUpdate(float /*deltaTime*/) {}
    virtual void OnDisable() {}
    virtual void OnDestroy() {}

    //-------------------------------------------------------------------------
    // Render
    //  - 描画処理を持つコンポーネントで override（MeshRenderer など）
    //  - Renderer へのコマンド記録をここで行う設計
    //-------------------------------------------------------------------------
    virtual void Render(class D3D12Renderer* /*renderer*/) {}

    //-------------------------------------------------------------------------
    // Owner（所属する GameObject）
    //  - 循環参照回避のため weak_ptr。必要時に lock() で shared_ptr を取得。
    //  - AddComponent 時や Scene への組み込み時にエンジン側でセットする想定。
    //-------------------------------------------------------------------------
    void SetOwner(std::shared_ptr<GameObject> owner) { m_Owner = owner; }
    std::shared_ptr<GameObject> GetOwner() const { return m_Owner.lock(); }

    //-------------------------------------------------------------------------
    // 有効/無効切り替え
    //  - 変更時のみ OnEnable / OnDisable を発火（重複呼び出しを防止）
    //  - ゲーム中の一時停止や可視/不可視の切替に利用
    //-------------------------------------------------------------------------
    bool IsEnabled() const { return m_Enabled; }
    void SetEnabled(bool enabled)
    {
        if (m_Enabled != enabled)
        {
            m_Enabled = enabled;
            if (m_Enabled) OnEnable();
            else           OnDisable();
        }
    }

    //-------------------------------------------------------------------------
    // Start() 呼び出し管理
    //  - Start は一度きりのため、フラグで制御（エンジン側の Update 入口で使用）
    //-------------------------------------------------------------------------
    bool HasStarted() const { return m_Started; }
    void MarkStarted() { m_Started = true; }

protected:
    // 型情報（デバッグ/エディタ用）
    ComponentType m_Type = ComponentType::None;

    // 所属 GameObject（循環参照防止のため weak）
    std::weak_ptr<GameObject> m_Owner;

    // 状態フラグ
    bool m_Started = false; // Start() 済みか
    bool m_Enabled = true;  // 現在有効か（OnEnable/OnDisable の発火制御）
};
