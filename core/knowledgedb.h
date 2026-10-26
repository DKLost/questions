#ifndef KNOWLEDGEDB_H
#define KNOWLEDGEDB_H

#include <QString>
#include <QList>
#include <QVariant>
#include <QSqlDatabase>

struct Phenomenon;
struct Abstract;

// 量化记号（详见 docs/00-核心原理、docs/01 §1.3）：∃=具象存在量化（展开为真正的 is-a），(∃)=抽象存在量化算子（展开为 ∀is-a）；
// 录入形式 F(∃)/(∃)F 即右(∃)/左(∃)，对应 : / @。出于简化，用户写的 pred 概念上即 ∃pred∃、但只写作 pred；
// 内部 is-a 概念上即 ∀is-a、但只写作 is-a（∃ 与 ∀ 两符号在系统中均不出现）。
// 软件不构造实例层关系项，库存的 is-a 一律为 ∀is-a 锚点。
struct Abstract {
    QString name;
    QString unfold_L_Predicate = "";//｢P A B｣ ⇒ ∃x (x is I ∧ x P_L A ∧ x P_R B)
    QString unfold_C_Idea = "";
    QString unfold_R_Predicate = "";

    Abstract() = default;

    Abstract(const QString& n){name = n;}  // 隐式转换：QString → Abstract，便于嵌套调用

    bool operator==(const Abstract& other) const {
        return name == other.name;
    }

    bool operator!=(const Abstract& other) const {
        return name != other.name;
    }
};

struct Phenomenon {
    Abstract predicate;   // P
    Abstract subject;     // S
    Abstract object;      // O

    bool operator==(const Phenomenon& other) const {
        return predicate == other.predicate
               && subject == other.subject
               && object == other.object;
    }

    bool operator!=(const Phenomenon& other) const {
        return predicate != other.predicate
               || subject != other.subject
               || object != other.object;
    }

    QString toStr() const{
        return QString("｢%1 %2 %3｣").arg(predicate.name, subject.name, object.name);
    }
};

struct PhenIntension {
    Phenomenon conditioned;
    Phenomenon prerequisite;

    bool operator==(const PhenIntension& other) const {
        return conditioned == other.conditioned
               && prerequisite == other.prerequisite;
    }

    bool operator!=(const PhenIntension& other) const {
        return conditioned != other.conditioned
               || prerequisite != other.prerequisite;
    }
};

struct TreeNode {
    int phenId=-1;
    QList<TreeNode*> children;

    // buildTree 预填的显示数据（queryTree 建树时不填，保持默认）
    bool atFlag = false;          // true=@ 行（lEAbs），false=: 行（rEAbs）
    QString subjectName;
    QString objectName;
    QString unfoldedPred;         // : 行为谓词 unfold_L 名；@ 行为 unfold_R 名
    QString revUnfoldedPred;      // : 行为 rev(unfold_L) 名（渲染关系段用）；@ 行同 unfoldedPred
    QString derivedAbs;           // 本边（生成内涵现象的现象）派生的抽象 = DerivedAbs（渲染层 DerivedAbsRole，见 docs/01 定义 1.14b）

    TreeNode() = default;
    TreeNode(int _phenId=-1)
        : phenId(_phenId) {}

    ~TreeNode() {
        qDeleteAll(children);
    }
};

struct LineSegment {
    QString pred;
    QString obj;
    bool atFlag = false;
};

struct LineResult {
    int phenomenonId = -1;
    QString derivedAbs; // 本行（生成内涵现象的现象）派生的抽象名，供下一层嵌套行作父主体
};

struct Scope {
    QString subj = "world";
    QString pred = "has";
};

class KnowledgeDB // 连接名 "knowledge"
{
private:
    KnowledgeDB() = default;

    QSqlDatabase db;
    QString dbFileName = "knowledge.db";

public: //数据库
    static KnowledgeDB* instance();
    static void init(const QString& fileName);      //首次使用前指定数据库路径
    void clear();
    void ensureOpen();                              //惰性打开数据库并建表

public: //概念抽象
    int ensureAbstract(const Abstract& a);       //INSERT OR IGNORE，返回 id
    int getAbstractId(const Abstract& a);
    Abstract getAbstract(const QString& name);      //按 name 查询完整记录（不存在则返回仅有 name 的记录）
    Abstract getAbstract(int id);                   //按 id 查询完整记录（不存在则返回空记录）
    QList<int> getDirectDerivedAbsIds(const Abstract& a); //查询直接派生自 a 的抽象 id（只一层，不含 a 自身）
    QList<int> getDerivedAbsIds(const Abstract& a); //查询所有派生自 a 的抽象 id（含各级后代，不含 a 自身）


    //查找
    QList<Abstract> abstract();
    QList<Abstract> findObjects(const QString& pred, const QString& subj);   // 指象 [pred subj]：满足｢pred, subj, ?｣的全部宾词（docs/00-核心原理）
    QList<Abstract> findObjsScoped(const QString &pred, const QString &subj, const Scope &scope = {});
    QList<Abstract> findObjsScoped(const QString &pred, const QString &subj, const QString &scope_has = "world"); //带域指象：查｢pred scope→has:[subj] ??｣；pred 为 is 时即 identify，可代替一般数据库查询（返回 scope/subj 的值），但同一 subject 的 is 宾词可能多个，故可能返回多值，这点和传统数据库不同。

    //构造相关
    Abstract revPredicate(const Abstract& pred);
    Abstract revTempPredicate(const Abstract& pred);
    Abstract constructPred(const Abstract& pred_L,const Abstract& idea_C,const Abstract& pred_R);
    Abstract lExistAbsPredicate(const Abstract& pred);
    Abstract rExistAbsPredicate(const Abstract& pred);
    Abstract rExistAbsTempPredicate(const Abstract &pred);
    Abstract lExistAbsTempPredicate(const Abstract &pred);

    bool isLExistAbsPred(Abstract pred);
    bool isRExistAbsPred(Abstract pred);
    bool isRExistAbsPred(int pred_id);
    bool isLExistAbsPred(int pred_id);

public: //现象
    void insertPhenomenon(int predId,int subjId,int objId); //INSERT OR IGNORE
    int ensurePhenomenon(const Phenomenon& p);   //INSERT OR IGNORE，返回 id
    void addPhenomenon(const Phenomenon& p);     // 添加现象（自动补充逆现象）
    void delPhenomenon(const Phenomenon& p);     // 删除现象（自动删除逆现象；若为 : / @ 行主现象，级联删除其派生抽象下的现象）
    void delPhenomenon(int id);                  // 按现象 id 删除（同上）
    //底层：把现象就地换成新三元组（只接受库内形态；主现象保 id，派生名整族换名、旧锚点对删旧重建）——下面四个只是它的薄包装
    bool changePhenomenon(int phenId, const Phenomenon& newPhenomenon);
    bool changePhenomenonSubj(int phenId, const QString& newSubj); // 改主语并连带整棵子树换派生名（现象 id 保留；只接受库内形态）；宾语按新父级重限定（createLine 口径）
    bool changePhenomenonObj(int phenId, const QString& newObj);   // 改宾语并连带整棵子树换派生名（同上，只接受库内形态）
    bool changePhenomenonPred(int phenId, const QString& newPred); // 改谓词（书写谓词 has/is…，按原有 : / @ 形态包成复合谓词）并连带整棵子树换派生名
    bool changePhenomenonPredType(int phenId);                     // 换 (∃) 形式：同一书写谓词与两端下把 : 行（rEAbs）与 @ 行（lEAbs）互换，存在量化落点换到另一端
    bool copyPhenomenonSubj(int phenId, const QString& newSubj);   // 同上但只新建不删除：原树保留，新树挂到 newSubj 下
    //void copyPhenomenon(const Phenomenon& p,const Abstract& newSubj);
    


    //查找
    QList<Phenomenon> phenomena();
    QList<Phenomenon> queryPhenomena(const QString& where, const QList<QVariant>& binds = {}); //通用现象查询：JOIN abstracts 还原三元组名称，并回填谓词的 unfold 字段
    QList<int> queryPhenomenonIds(const QString& where, const QList<QVariant>& binds = {});
    int phenomenonId(const QString& pred, const QString& subj, const QString& obj);
    int phenomenonId(const Phenomenon &phen);
    Phenomenon getPhenomenon(int id);
    Abstract getDerivedAbs(const Phenomenon &p);
    QList<int> getDerivedPhenIds(const Phenomenon &p);
    QList<int> queryLines(const int& rootSubject_id);
    QList<int> queryELines(const int& rootSubject_id);
    // 大数据下替代旧递归 queryTree：两条 SQL 全量载入后纯内存递归建树（零 SQL/零写库），roots 可多主体、异常环退化为叶边
    TreeNode* buildTree(const QList<int>& rootSubjectIds);
    TreeNode* buildTree(int rootSubject_id);


public: //现象间内涵关系
    void insertPhenIntension(int conId, int preId);
    void addPhenIntension(const PhenIntension& pi);

    //查找
    QList<int> queryDirectPrerequisites(int phenId);
    QList<int> queryDirectPrerequisites(Phenomenon phen);

public: //交叉功能

    //构造相关
    Abstract constructAbs(const Abstract& subject_idea, const Abstract& predicate, const Abstract& object_idea);
    Abstract constructTempAbs(const Abstract& subject_idea, const Abstract &predicate, const Abstract &object_idea);
    Abstract constructTempPred(const Abstract &pred_L, const Abstract &idea_C, const Abstract &pred_R);

    // 纯字符串工具（零 SQL/零写库），供 auto(chain) 重放判分复用；DerivedAbs 名形如 "A→pred:[B]"，末段为 "关系段:[宾词]"
    static QString lastObjectOfDerivedAbs(const QString &derivedAbs);        // 取末段宾词（显示名），非法名返回原串
    static QString replaceLastObjectOfDerivedAbs(const QString &derivedAbs,
                                                 const QString &newObjectUnEncoded);  // 替换末段宾词（前缀/关系段不动），非法名返回空串

    // 链卡(typst)的宾词 ↔ 用户输入口径（见 docs/01 约定 4.5a）：宾词名写成 `$ 公式体 $`，用户只输入公式体
    static bool isChainTypstName(const QString &objectText);      // 是否 `$ … $` 完全包裹（只有这种宾词名能按该类型复习）
    static QString chainTypstBodyOf(const QString &objectText);   // 剥两端 $ 并去外侧空白 → 公式体；非完全包裹时原样返回（暂不支持混排）

    //折叠相关
    int predicateDepth(const Abstract& pred);
    void genFoldedPhenomenon(int depth);
    void _genFoldedPhenomenon(Abstract a,int depth); //｢<∃F∃,I,∃G∃(∃)> A B｣ Being-∀is-a->I,A-∃F∃->Being-∃G∃(∃)>B ;; <F I G(∃)> <F I <~G,Abstract,is-a>>（Being=通用具象，本系统以通用抽象 Abstract 顶替；见 docs/00-核心原理）

    //作用域相关
    QList<LineSegment> parseLineSegments(const QString& text);
    QString scopePrefix(){return QStringLiteral("world");}
    bool isQualified(const QString& name);
    QString qualify(const QString& name);
    QString qualify(const QString &name, const Scope &scope);
    QString unqualify(const QString& name);
    QString unqualify(const QString &name, const Scope &scope); // 按域投影：先去一层 scope 外壳（仅当它是前缀），再剥全部 world→has:[…]，最后缩写+解码
    
    // 名字层转义：结构元符号 \ [ ] → 前加反斜杠（\ 自身也转义），使叶子名可安全嵌入 DerivedAbs 名
    static QString encodeName(const QString& nameUnEncoded);
    static QString decodeName(const QString& name);
    static bool isEscapedAt(const QString& text, int index);
    static bool containsUnescaped(const QString& text, QChar c);
    static bool isChainName(const QString& name);                // 含未转义 →：结构名而非叶子名
    static QString expandByRegex(const QString &input);
    static QString expandRecursively(const QString &input);

    // 笔记相关
    Abstract relate(const QString& pred, const QString& subj, const QString& obj, bool atFlag = false);    // 构造一个关系存在断言，返回其派生的抽象（DerivedAbs，即下一段主体）。
    Abstract TempRelated(const QString& pred, const QString& subj, const QString& obj, bool atFlag = false); // 临时谓词版本：不落库，仅推导 DerivedAbs 名
    LineResult createLine(const QString &rootSubject, const QString &pred, const QString &obj, const bool &atFlag);
    LineResult createLine(const QString &rootSubject, const QList<LineSegment> &segments); // 返回行主现象 id（最后一段）+ DerivedAbs（供下一层嵌套行作父）。
    void ensureDeclaration(const QString& individualName);     // 确保个体限定声明存在（world→has:[X] 锚点对），幂等。
    Abstract barePredicateOf(const Phenomenon& p);             // 由存储的 (∃)形式谓词还原书写谓词（删除/换主语/换宾语/复制共用）
    void changeAbstract(int absId, const QString& newAbs); // 抽象换名级联：引用它的现象就地改引用、派生名继续下探（只用于派生名；顶层锚点由 changePhenomenon 处理）
    void delAnchorsOf(const Phenomenon& p, const QString& derivedAbs); // 删本行锚点对（∀is-a 与内属关系现象，含各自逆行）
    void addAnchorsOf(const Phenomenon& p);                            // 按本现象当前三元组重建锚点对
    void rehangPhenomenon(const Phenomenon& p, const Phenomenon& newPhenomenon);  // 主现象整行换、逆现象换其逆
    Abstract existAbsPredicateOf(const Phenomenon& p, const QString& bareName);   // 书写谓词 → 同形态 (∃)形式复合谓词

    // 顶层孤立概念：world 级概念 X（束 C = world→has:[X]）参与的现象若全部是内属现象
    // （X 的 is-a 自反、world 级声明语句及其逆行、束的 ∀is-a 锚点与 ~has 内属关系各含逆），
    // 它就只是"声明某个体"时顺带造出的空壳：无子行、无外部引用、无别的 world 级语句指向它。
    QList<QString> orphanTopLevelConceptNames();                  // 返回这些概念的裸名 X（只看图谱，不涉及题库）
    bool removeOrphanTopLevelConcept(const QString &bareName);    // 删除一个孤立概念（声明语句级联 + 其 is-a 自反锚点）

    //旧内容
    QString generateBody(const Abstract& subject, QSet<QString>& visited, const int& level);
    QString generateNote(const Abstract& subject);

    //其他函数

};

#endif // KNOWLEDGEDB_H