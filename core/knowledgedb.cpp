#include "knowledgedb.h"
#include <QSqlQuery>
#include <QRegularExpression>
#include <QSqlError>
#include <QHash>
#include <QSet>
#include <algorithm>

KnowledgeDB* KnowledgeDB::instance() {
    static KnowledgeDB db;
    return &db;
}

void KnowledgeDB::init(const QString &fileName)
{
    instance()->dbFileName = fileName;
}

int KnowledgeDB::predicateDepth(const Abstract &pred){
    int result = 0;
    if(pred.unfold_L_Predicate=="")
        return 0;
    if(pred.unfold_L_Predicate!=""){
        Abstract p_L = getAbstract(pred.unfold_L_Predicate);
        Abstract p_R = getAbstract(pred.unfold_R_Predicate);
        if(p_L.unfold_L_Predicate=="") result+=1;
        else{
            result+=predicateDepth(p_L);
        }

        if(p_R.unfold_L_Predicate=="") result+=1;
        else{
            result+=predicateDepth(p_R);
        }
        return result;
    }

}

void KnowledgeDB::ensureOpen()
{
    if(db.isOpen()) return;
    db = QSqlDatabase::addDatabase("QSQLITE","knowledge");
    db.setDatabaseName(dbFileName);
    db.open();
    QSqlQuery query(db);
    query.exec("PRAGMA foreign_keys = ON"); // SQLite 默认关闭外键：不开则表上的 ON DELETE CASCADE 全是摆设
    query.exec("CREATE TABLE IF NOT EXISTS abstracts("
               "id INTEGER PRIMARY KEY,"
               "name TEXT UNIQUE NOT NULL,"
               "unfold_L TEXT NOT NULL DEFAULT '',"
               "unfold_C TEXT NOT NULL DEFAULT '',"
               "unfold_R TEXT NOT NULL DEFAULT '')");
    query.exec("CREATE TABLE IF NOT EXISTS phenomena("
               "id INTEGER PRIMARY KEY,"
               "predicate INTEGER NOT NULL REFERENCES abstracts(id) ON DELETE CASCADE,"
               "subject INTEGER NOT NULL REFERENCES abstracts(id) ON DELETE CASCADE,"
               "object INTEGER NOT NULL REFERENCES abstracts(id) ON DELETE CASCADE,"
               "UNIQUE(predicate,subject,object))");
    query.exec("CREATE TABLE IF NOT EXISTS phenIntensions("
               "id INTEGER PRIMARY KEY,"
               "conditioned INTEGER NOT NULL REFERENCES phenomena(id) ON DELETE CASCADE,"
               "prerequisite INTEGER NOT NULL REFERENCES phenomena(id) ON DELETE CASCADE,"
               "CHECK(conditioned <> prerequisite),"
               "UNIQUE(conditioned, prerequisite))");
    // 外键列索引：建树 / findObjects 依赖；无索引时 subject=? 走全表 SCAN（× 3 JOIN），随库增大近二次方劣化
    query.exec("CREATE INDEX IF NOT EXISTS idx_phenomena_subject ON phenomena(subject)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_phenomena_predicate ON phenomena(predicate)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_phenomena_object ON phenomena(object)");
}

int KnowledgeDB::ensureAbstract(const Abstract &a)
{
    ensureOpen();
    QSqlQuery query(db);
    query.prepare("INSERT OR IGNORE INTO abstracts(name,unfold_L,unfold_C,unfold_R) VALUES(?,?,?,?)");
    query.bindValue(0,a.name);
    query.bindValue(1,a.unfold_L_Predicate);
    query.bindValue(2,a.unfold_C_Idea);
    query.bindValue(3,a.unfold_R_Predicate);
    query.exec();

    
    query.prepare("SELECT id FROM abstracts WHERE name = ?");
    query.bindValue(0,a.name);
    query.exec();
    query.next();
    return query.value(0).toInt();
}

int KnowledgeDB::getAbstractId(const Abstract &a)
{
    ensureOpen();
    QSqlQuery query(db);
    query.prepare("SELECT id FROM abstracts WHERE name = ?");
    query.bindValue(0,a.name);
    query.exec();
    return query.next() ? query.value(0).toInt() : -1;
}

void KnowledgeDB::insertPhenomenon(int predId, int subjId, int objId)
{
    ensureOpen();
    QSqlQuery query(db);
    query.prepare("INSERT OR IGNORE INTO phenomena(predicate,subject,object) VALUES(?,?,?)");
    query.bindValue(0,predId);
    query.bindValue(1,subjId);
    query.bindValue(2,objId);
    query.exec();
}

void KnowledgeDB::insertPhenIntension(int condId, int preId)
{
    ensureOpen();
    QSqlQuery query(db);
    query.prepare("INSERT OR IGNORE INTO phenIntensions(conditioned,prerequisite) VALUES(?,?)");
    query.bindValue(0,condId);
    query.bindValue(1,preId);
    query.exec();
}

int KnowledgeDB::ensurePhenomenon(const Phenomenon& p)
{
    ensureOpen();
    int predId=ensureAbstract(p.predicate),
        subjId=ensureAbstract(p.subject),
        objId=ensureAbstract(p.object);

    insertPhenomenon(predId,subjId,objId);

    QSqlQuery query(db);
    query.prepare("SELECT id FROM phenomena WHERE predicate = ? AND subject = ? AND object = ?");
    query.bindValue(0,predId);
    query.bindValue(1,subjId);
    query.bindValue(2,objId);
    query.exec();
    query.next();
    return query.value(0).toInt();
}

void KnowledgeDB::addPhenomenon(const Phenomenon &p)
{
    ensurePhenomenon(p);

    Phenomenon rev_p{revPredicate(p.predicate),p.object,p.subject};
    ensurePhenomenon(rev_p);
}

Abstract KnowledgeDB::getDerivedAbs(const Phenomenon& p){
    bool isIntensionalPred = isRExistAbsPred(p.predicate) || isLExistAbsPred(p.predicate);
    Abstract DerivedAbs{""}; // 视角是本现象 p：它是 p 派生的抽象，故此处称 DerivedAbs
    if(isIntensionalPred){
        bool atFlag = isLExistAbsPred(p.predicate);
        Abstract barePred = atFlag ? getAbstract(p.predicate.unfold_R_Predicate)
                                   : revPredicate(getAbstract(p.predicate.unfold_L_Predicate));
        DerivedAbs = TempRelated(barePred.name, p.subject.name, p.object.name, atFlag);
    }
    return DerivedAbs;
}

QList<int> KnowledgeDB::getDerivedPhenIds(const Phenomenon& p){
    QList<int> DerivedPhenIds;
    Abstract DerivedAbs = getDerivedAbs(p);
    int DerivedAbsId = getAbstractId(DerivedAbs);

    if(DerivedAbsId >=0){
        DerivedPhenIds = queryPhenomenonIds("s.name = ?", {DerivedAbs.name});
    }
    return DerivedPhenIds;
}

// 只取一层：以 a 为主语的现象（子行 / 锚点行）各派生一个抽象
QList<int> KnowledgeDB::getDirectDerivedAbsIds(const Abstract& a)
{
    ensureOpen();
    QList<int> derivedAbsIds;
    const QList<int> childPhenIds = queryPhenomenonIds("s.name = ?", {a.name});
    for(const int childPhenId : childPhenIds){
        const Abstract childAbs = getDerivedAbs(getPhenomenon(childPhenId));
        if(childAbs.name.isEmpty())
            continue; // 普通现象不派生抽象
        const int childAbsId = getAbstractId(childAbs);
        if(childAbsId < 0 || derivedAbsIds.contains(childAbsId))
            continue; // 同名抽象可能由多条现象共享
        derivedAbsIds.append(childAbsId);
    }
    return derivedAbsIds;
}

// 以直接派生为原语逐层展开：后代抽象名逐层加长，故按 id 去重即可终止
QList<int> KnowledgeDB::getDerivedAbsIds(const Abstract& a)
{
    ensureOpen();
    QList<int> derivedAbsIds;
    QSet<int> collected;
    QList<QString> pendingAbsNames{a.name};
    while(!pendingAbsNames.isEmpty()){
        const QList<int> childAbsIds = getDirectDerivedAbsIds(getAbstract(pendingAbsNames.takeFirst()));
        for(const int childAbsId : childAbsIds){
            if(collected.contains(childAbsId))
                continue; // 菱形链会对同一抽象重复到访
            collected.insert(childAbsId);
            derivedAbsIds.append(childAbsId);
            pendingAbsNames.append(getAbstract(childAbsId).name);
        }
    }
    return derivedAbsIds;
}

void KnowledgeDB::delPhenomenon(const Phenomenon& p){
    ensureOpen();

    // 是 : / @ 行主现象则级联删除：它派生的 DerivedAbs 是子行/锚点对的依附主体，缺了它那些现象无意义（正逆归同一 DerivedAbs）
    const bool isLine = isRExistAbsPred(p.predicate) || isLExistAbsPred(p.predicate);
    Abstract derivedAbs;
    if(isLine)
        derivedAbs = TempRelated(barePredicateOf(p).name, p.subject.name, p.object.name, isLExistAbsPred(p.predicate));

    int pId = phenomenonId(p);
    Phenomenon revP{revPredicate(p.predicate), p.object, p.subject};
    int revPId = phenomenonId(revP);

    QSqlQuery query(db);

    if(pId >= 0){
        query.prepare("DELETE FROM phenomena WHERE id = ?");
        query.addBindValue(pId);
        query.exec();
    }
    if(revPId >= 0 && revPId != pId){ // 逆现象；自反时与正向同 id，避免重复删除
        query.prepare("DELETE FROM phenomena WHERE id = ?");
        query.addBindValue(revPId);
        query.exec();
    }

    // 级联：以 DerivedAbs 为主语的现象逐层递归（每条又派生新抽象）；DerivedAbs 名逐层加长，故递归必终止
    if(isLine && !derivedAbs.name.isEmpty()){
        const QList<int> childIds = queryPhenomenonIds("s.name = ?", {derivedAbs.name});
        for(const int childId : childIds){
            delPhenomenon(childId); // 走 id 重载：已被删除/不存在的 id 自动跳过
        }
    }
}

// 按现象 id 删除：先还原三元组（queryPhenomena 回填的 unfold 字段是构造逆现象所必需）
void KnowledgeDB::delPhenomenon(int id)
{
    if(id < 0)
        return;
    Phenomenon p = getPhenomenon(id);
    if(p.predicate.name.isEmpty()) // 该 id 不存在
        return;
    delPhenomenon(p);
}

// 换三元组的四个薄包装：只把"要改的那一部分"装配进新三元组，其余照旧；装配成同一三元组时由底层直通、不写库
bool KnowledgeDB::changePhenomenonSubj(int phenId, const QString &newSubj)
{
    const Phenomenon p = getPhenomenon(phenId);
    if(p.predicate.name.isEmpty())
        return false;
    if(p.subject.name == newSubj)
        return true; // 同名＝没换：直通，且不补声明（否则会给 world 之类补出多余的声明锚点）
    if(newSubj != QStringLiteral("world") && !isChainName(newSubj))
        ensureDeclaration(newSubj); // 裸名新端：补 world 锚点四件套，与录入边界同口径（world 本身不是个体，不声明）
    // 宾语形态由父级决定（createLine 口径：world 存裸名、嵌套存限定名）——跨边界换主语须连带重限定，否则同一个体以两种名字并存
    QString newObj = p.object.name;
    const QString individual = isQualified(newObj) ? expandByRegex(newObj) : newObj; // 与换宾语同口径取"裸名个体"
    if(newSubj == QStringLiteral("world")){
        if(isQualified(newObj))
            newObj = individual; // 回 world 级：剥一层壳
    }else if(!isChainName(newObj) && newObj != QStringLiteral("world")){
        newObj = qualify(newObj); // 进嵌套：裸名补壳
    }
    const bool moved = changePhenomenon(phenId, Phenomenon{p.predicate, Abstract{newSubj}, Abstract{newObj}});
    if(moved && !isChainName(individual) && individual != QStringLiteral("world"))
        ensureDeclaration(individual); // 声明行可能随本体被搬走（world 级行本身就是声明行），换完补回
    return moved;
}

// 换宾语同理（: 行的宾语是被关联概念、@ 行的宾语是锚定侧）；同样只做就地换名、保 id
bool KnowledgeDB::changePhenomenonObj(int phenId, const QString &newObj)
{
    const Phenomenon p = getPhenomenon(phenId);
    if(p.predicate.name.isEmpty())
        return false;
    if(p.object.name == newObj)
        return true; // 同名＝没换：直通，且不补声明
    // 与 createLine 同口径：宾语无论传裸名还是限定名，都补其"裸名个体"的 world 声明（world 本身不是个体，不声明）
    const QString individual = isQualified(newObj) ? expandByRegex(newObj) : newObj;
    if(individual != QStringLiteral("world") && !isChainName(individual))
        ensureDeclaration(individual);
    return changePhenomenon(phenId, Phenomenon{p.predicate, p.subject, Abstract{newObj}});
}

// 换谓词传的是书写谓词（has / is …），内部按本现象原有的 : / @ 形态换成新的 (∃)形式复合谓词
bool KnowledgeDB::changePhenomenonPred(int phenId, const QString &newPred)
{
    const Phenomenon p = getPhenomenon(phenId);
    if(p.predicate.name.isEmpty())
        return false;
    return changePhenomenon(phenId, Phenomenon{existAbsPredicateOf(p, newPred), p.subject, p.object});
}

// 换 (∃) 形式：同一书写谓词与两端下把 : 行（rEAbs）与 @ 行（lEAbs）互换——存在量化落点换到另一端（《01》性质 1.13）
bool KnowledgeDB::changePhenomenonPredType(int phenId)
{
    const Phenomenon p = getPhenomenon(phenId);
    const bool wasAt = isLExistAbsPred(p.predicate);
    if(!wasAt && !isRExistAbsPred(p.predicate))
        return false; // 只有 : / @ 行有 (∃) 形式
    const Abstract bare = barePredicateOf(p);
    return changePhenomenon(phenId, Phenomenon{wasAt ? rExistAbsPredicate(bare) : lExistAbsPredicate(bare),
                                               p.subject, p.object});
}

// 底层：把现象就地换成新三元组（保 id、含逆；派生名由三元组决定，故连带删建锚点对与整族级联换名；名字只接受库内形态）
bool KnowledgeDB::changePhenomenon(int phenId, const Phenomenon &newPhenomenon)
{
    ensureOpen();
    const Phenomenon p = getPhenomenon(phenId);
    if(p.predicate.name.isEmpty())
        return false;
    if(p == newPhenomenon) // 没变：直通（否则会被"目标三元组已存在"分支当成重复边，把本行连同整族删掉）
        return true;

    const Abstract oldDerived = getDerivedAbs(p);
    if(oldDerived.name.isEmpty()){
        rehangPhenomenon(p, newPhenomenon);
        return true; // 非 : / @ 行：没有派生名与锚点族，换完本行即可
    }

    const Abstract newDerived = getDerivedAbs(newPhenomenon);
    if(newDerived.name.isEmpty()){
        rehangPhenomenon(p, newPhenomenon);
        return true; // 换成非 : / @ 谓词：旧族留在旧派生名下作孤儿，不构造空名抽象
    }

    delAnchorsOf(p, oldDerived.name);
    const int oldDerivedId = getAbstractId(oldDerived);
    if(oldDerivedId >= 0) // 抽象记录不存在即无人引用它
        changeAbstract(oldDerivedId, newDerived.name);
    rehangPhenomenon(p, newPhenomenon); // 排在级联之后：先换的话"新端正取旧派生名整名"会被级联当成族员误改
    addAnchorsOf(newPhenomenon);
    return true;
}

// 抽象换名级联：主/宾列引用该名字的现象就地改引用、派生名随各行三元组继续下探（只用于派生名；端名/谓词名是共享名不能全局扫）
void KnowledgeDB::changeAbstract(int absId, const QString &newAbs)
{
    const Abstract oldAbs = getAbstract(absId);
    if(oldAbs.name.isEmpty() || newAbs.isEmpty() || oldAbs.name == newAbs)
        return;
    const int newId = ensureAbstract(Abstract{newAbs});
    const QList<int> refIds = queryPhenomenonIds("s.name = ? OR o.name = ?", {oldAbs.name, oldAbs.name}); // 谓词列是对偶名不扫；行与逆行的引用互为对偶列，成对改完即自洽
    QSqlQuery query(db);
    for(const int refId : refIds){
        const QList<Phenomenon> found = queryPhenomena("p.id = ?", {refId});
        if(found.isEmpty())
            continue; // 已被前一趟合并删掉
        const Phenomenon row = found.first();
        const QString newSubj = (row.subject.name == oldAbs.name) ? newAbs : row.subject.name;
        const QString newObj = (row.object.name == oldAbs.name) ? newAbs : row.object.name;
        if(newSubj == row.subject.name && newObj == row.object.name)
            continue; // 对偶的那半先被处理过

        const int predId = getAbstractId(row.predicate);
        const int subjId = getAbstractId(row.subject);
        const int objId = getAbstractId(row.object);
        const int newSubjId = (newSubj == newAbs) ? newId : subjId;
        const int newObjId = (newObj == newAbs) ? newId : objId;

        query.prepare("SELECT COUNT(*) FROM phenomena WHERE predicate = ? AND subject = ? AND object = ?");
        query.addBindValue(predId);
        query.addBindValue(newSubjId);
        query.addBindValue(newObjId);
        query.exec();
        if(query.next() && query.value(0).toInt() > 0){ // 已有等价边：删本行，族随递归改挂新名（与 rehangPhenomenon 同策略）
            query.prepare("DELETE FROM phenomena WHERE predicate = ? AND subject = ? AND object = ?");
            query.addBindValue(predId);
            query.addBindValue(subjId);
            query.addBindValue(objId);
            query.exec();
        }else{
            query.prepare("UPDATE phenomena SET subject = ?, object = ? WHERE predicate = ? AND subject = ? AND object = ?");
            query.addBindValue(newSubjId);
            query.addBindValue(newObjId);
            query.addBindValue(predId);
            query.addBindValue(subjId);
            query.addBindValue(objId);
            query.exec();
        }

        const Abstract dOld = getDerivedAbs(row);
        const Abstract dNew = getDerivedAbs(Phenomenon{row.predicate, Abstract{newSubj}, Abstract{newObj}});
        if(dOld.name.isEmpty() || dOld.name == dNew.name)
            continue; // 非 : / @ 行无派生名；没变则不下探
        const int dOldId = getAbstractId(dOld);
        if(dOldId >= 0)
            changeAbstract(dOldId, dNew.name);
    }
}

// 本行锚点对（∀is-a 与内属关系现象，见《01》定义 1.12）：按旧派生名与旧形态定位两条后删除，各自逆行随 delPhenomenon 一并删
void KnowledgeDB::delAnchorsOf(const Phenomenon &p, const QString &derivedAbs)
{
    const bool atFlag = isLExistAbsPred(p.predicate);
    const Abstract bare = barePredicateOf(p);
    delPhenomenon(phenomenonId(QStringLiteral("is-a"), derivedAbs, atFlag ? p.subject.name : p.object.name));
    delPhenomenon(phenomenonId((atFlag ? bare : revPredicate(bare)).name, derivedAbs, atFlag ? p.object.name : p.subject.name));
}

// 按本现象当前三元组重建锚点对：: 行锚点归宾端、@ 行归主端；constructAbs 顺带落派生名与两条逆行
void KnowledgeDB::addAnchorsOf(const Phenomenon &p)
{
    const Abstract bare = barePredicateOf(p);
    if(isLExistAbsPred(p.predicate))
        constructAbs(p.subject, bare, p.object);
    else
        constructAbs(p.object, revPredicate(bare), p.subject);
}

// 顶层孤立概念：world 级概念 X（束 C = world→has:[X]）参与的现象若全部是内属现象，即下面这张"允许表"：
//   ① X 的 is-a 自反 ｢is-a, X, X｣（及逆行 ｢~is-a, X, X｣——自反的两端相同，逆行是另一条而不是同一条）
//   ② world 级声明语句 ｢has:[Abstract]→is-a, world, X｣ 及其逆行      —— 声明 step2 落的 : 行（也就是树上那行）
//   ③ 束的 ∀is-a 锚点 ｢is-a, C, X｣ 及其逆行                         —— 声明 step4
//   ④ 束的内属关系 ｢~has, C, world｣ 及其逆行                        —— 声明 step4
// 则它只是 ensureDeclaration 顺带造出的空壳：没有子行、没有被别的行引用、也没有别的 world 级语句指向它。
// 只要它还出现在任何一条别的现象里（子行、被引用、别的谓词的顶层语句），就不算孤立——故本判据是保守的。
// 全量载入一次后纯内存判定（同 buildTree 的做法），不按候选逐个查库。
QList<QString> KnowledgeDB::orphanTopLevelConceptNames()
{
    ensureOpen();

    const Abstract world{scopePrefix()};
    const Abstract isa{QStringLiteral("is-a")};
    const Abstract has{QStringLiteral("has")};
    const Abstract tildeHas{QStringLiteral("~has")};
    const Abstract rEAbsHas = rExistAbsPredicate(has);   // has:[Abstract]→is-a
    const Abstract revMain = revPredicate(rEAbsHas);     // ~is-a:[Abstract]→~has
    const Abstract revIsa = revPredicate(isa);           // ~is-a
    const Abstract revTildeHas = revPredicate(tildeHas); // has

    const QList<Phenomenon> all = phenomena();
    QHash<QString, QList<int>> involving; // 名字 → 涉及它的现象下标（主语或宾语）
    QList<int> candidates;                // world 级 has 语句的下标（候选）
    for(int i = 0; i < all.size(); ++i)
    {
        const Phenomenon &p = all.at(i);
        involving[p.subject.name].append(i);
        if(p.object.name != p.subject.name)
            involving[p.object.name].append(i);
        if(p.subject.name == world.name && p.predicate.name == rEAbsHas.name)
            candidates.append(i);
    }

    auto key = [](const Abstract &pred, const QString &subj, const QString &obj) {
        return pred.name + QChar(0x1f) + subj + QChar(0x1f) + obj;
    };

    QList<QString> orphans;
    for(const int candidate : candidates)
    {
        const QString x = all.at(candidate).object.name;
        if(x == world.name || isChainName(x))
            continue; // world 本身不是个体；链名宾语是"结构引用"而非个体声明
        const QString chain = qualify(x);

        QSet<QString> allowed;
        allowed.insert(key(isa, x, x));
        allowed.insert(key(revIsa, x, x)); // 自反锚点的逆行（两端相同，是另一条现象）
        allowed.insert(key(rEAbsHas, world.name, x));
        allowed.insert(key(revMain, x, world.name));
        allowed.insert(key(isa, chain, x));
        allowed.insert(key(revIsa, x, chain));
        allowed.insert(key(tildeHas, chain, world.name));
        allowed.insert(key(revTildeHas, world.name, chain));

        bool isolated = true;
        for(const int phenId : involving.value(x)) // 涉及裸名 X 的现象
            if(!allowed.contains(key(all.at(phenId).predicate, all.at(phenId).subject.name, all.at(phenId).object.name)))
            {
                isolated = false;
                break;
            }
        if(isolated)
            for(const int phenId : involving.value(chain)) // 涉及束名 C 的现象
                if(!allowed.contains(key(all.at(phenId).predicate, all.at(phenId).subject.name, all.at(phenId).object.name)))
                {
                    isolated = false;
                    break;
                }
        if(isolated)
            orphans.append(x);
    }
    return orphans;
}

// 删除一个顶层孤立概念：先删 world 级声明语句（delPhenomenon 级联清掉整条束下的锚点与内属关系及其逆行），
// 再删该个体自己的 is-a 自反锚点（它挂在裸名 X 上，不在级联范围内）。抽象记录不删（与本系统一贯口径一致：名字按需保留）
bool KnowledgeDB::removeOrphanTopLevelConcept(const QString &bareName)
{
    ensureOpen();
    if(bareName.isEmpty())
        return false;

    const Abstract world{scopePrefix()};
    const int statementId = phenomenonId(rExistAbsPredicate(Abstract{QStringLiteral("has")}).name, world.name, bareName);
    if(statementId < 0)
        return false; // 不是 world 级 statements（或已不在库内）
    if(!orphanTopLevelConceptNames().contains(bareName))
        return false; // 有实际内容（或已不是孤立概念）：拒绝删除，防止误删

    delPhenomenon(statementId);
    delPhenomenon(Phenomenon{Abstract{QStringLiteral("is-a")}, Abstract{bareName}, Abstract{bareName}});
    return true;
}

// 按本现象原有的 : / @ 形态把书写谓词包成同形态的 (∃)形式复合谓词
Abstract KnowledgeDB::existAbsPredicateOf(const Phenomenon &p, const QString &bareName)
{
    const Abstract bare{bareName};
    ensureAbstract(bare);
    return isLExistAbsPred(p.predicate) ? lExistAbsPredicate(bare) : rExistAbsPredicate(bare);
}

// 姐妹函数：只新建不删除——把该 : / @ 行及其下整棵子树复制到 newSubj 下，原树原样保留
bool KnowledgeDB::copyPhenomenonSubj(int phenId, const QString &newSubj)
{
    ensureOpen();
    const Phenomenon p = getPhenomenon(phenId);
    if(p.predicate.name.isEmpty() || (!isRExistAbsPred(p.predicate) && !isLExistAbsPred(p.predicate)))
        return false; // 只有行主现象成树，锚点对等普通现象没有可复制的子树
    if(newSubj != QStringLiteral("world") && !isChainName(newSubj))
        ensureDeclaration(newSubj); // 裸名新端：补 world 锚点四件套（world 本身不是个体，不声明）

    const bool atFlag = isLExistAbsPred(p.predicate);
    const Abstract oldDerived = getDerivedAbs(p);
    // 与 changePhenomenonSubj 同口径：world 级存裸名（限定名剥一层壳）、嵌套下存限定名，链名（结构引用）原样保留
    const QString relObj = (newSubj == QStringLiteral("world"))
                               ? (isQualified(p.object.name) ? expandByRegex(p.object.name) : p.object.name)
                               : (isChainName(p.object.name) ? p.object.name : qualify(p.object.name));
    // 快照必须先取：宾语=旧派生名时，relate 落下的副本逆行会以旧派生名为主语，改完再查就会把副本自己当成子行无限复制
    const QList<int> oldFamily = queryPhenomenonIds("s.name = ?", {oldDerived.name});
    const Abstract newDerived = relate(barePredicateOf(p).name, newSubj, relObj, atFlag); // 复用写入链路：主现象 + 逆 + 裸名端自反边 + 派生抽象与锚点对
    for(const int childId : oldFamily){
        const Phenomenon child = getPhenomenon(childId);
        if(isRExistAbsPred(child.predicate) || isLExistAbsPred(child.predicate))
            copyPhenomenonSubj(childId, newDerived.name); // 只复制行；锚点对已由 relate 在新名下重建
    }
    return true;
}

// 主现象整行换到新三元组、逆现象换到其逆（逆谓词取逆、两端互换）；目标三元组已存在时按"已有一条等价边"删旧行（表上有 UNIQUE(predicate,subject,object)）
void KnowledgeDB::rehangPhenomenon(const Phenomenon &p, const Phenomenon &newPhenomenon)
{
    const int oldPredicateId = ensureAbstract(p.predicate);
    const int oldSubjectId = ensureAbstract(p.subject);
    const int oldObjectId = ensureAbstract(p.object);
    const int oldReverseId = getAbstractId(revPredicate(p.predicate));
    const int newPredicateId = ensureAbstract(newPhenomenon.predicate);
    const int newSubjectId = ensureAbstract(newPhenomenon.subject);
    const int newObjectId = ensureAbstract(newPhenomenon.object);
    const int newReverseId = getAbstractId(revPredicate(newPhenomenon.predicate));

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM phenomena WHERE predicate = ? AND subject = ? AND object = ?");
    query.addBindValue(newPredicateId);
    query.addBindValue(newSubjectId);
    query.addBindValue(newObjectId);
    query.exec();
    if(query.next() && query.value(0).toInt() > 0){ // 已有等价边：旧行与逆行直接删，整族随后统一挂到新派生名下
        query.prepare("DELETE FROM phenomena WHERE predicate = ? AND subject = ? AND object = ?");
        query.addBindValue(oldPredicateId);
        query.addBindValue(oldSubjectId);
        query.addBindValue(oldObjectId);
        query.exec();
        query.prepare("DELETE FROM phenomena WHERE predicate = ? AND subject = ? AND object = ?");
        query.addBindValue(oldReverseId);
        query.addBindValue(oldObjectId);
        query.addBindValue(oldSubjectId);
        query.exec();
        return;
    }

    query.prepare("UPDATE phenomena SET predicate = ?, subject = ?, object = ? WHERE predicate = ? AND subject = ? AND object = ?");
    query.addBindValue(newPredicateId);
    query.addBindValue(newSubjectId);
    query.addBindValue(newObjectId);
    query.addBindValue(oldPredicateId);
    query.addBindValue(oldSubjectId);
    query.addBindValue(oldObjectId);
    query.exec();
    query.prepare("UPDATE phenomena SET predicate = ?, subject = ?, object = ? WHERE predicate = ? AND subject = ? AND object = ?");
    query.addBindValue(newReverseId);
    query.addBindValue(newObjectId);
    query.addBindValue(newSubjectId);
    query.addBindValue(oldReverseId);
    query.addBindValue(oldObjectId);
    query.addBindValue(oldSubjectId);
    query.exec();
}

// 由存储的 (∃)形式谓词还原书写谓词：lEAbs 的 unfold_R 就是原谓词，rEAbs 的 unfold_L 是原谓词的逆（再逆回来）
Abstract KnowledgeDB::barePredicateOf(const Phenomenon &p)
{
    return isLExistAbsPred(p.predicate) ? getAbstract(p.predicate.unfold_R_Predicate)
                                        : revPredicate(getAbstract(p.predicate.unfold_L_Predicate));
}

void KnowledgeDB::addPhenIntension(const PhenIntension &pi)
{
    ensureOpen();
    int condId=ensurePhenomenon(pi.conditioned),
        preId=ensurePhenomenon(pi.prerequisite);

    insertPhenIntension(condId,preId);
}

Abstract KnowledgeDB::getAbstract(const QString &name)
{
    ensureOpen();
    QSqlQuery query(db);
    query.prepare("SELECT name,unfold_L,unfold_C,unfold_R FROM abstracts WHERE name = ?");
    query.bindValue(0,name);
    query.exec();
    Abstract a={""};
    if(query.next()){
        a.name = name;
        a.unfold_L_Predicate = query.value(1).toString();
        a.unfold_C_Idea = query.value(2).toString();
        a.unfold_R_Predicate = query.value(3).toString();
    }
    return a;
}

Abstract KnowledgeDB::getAbstract(int id)
{
    ensureOpen();
    QSqlQuery query(db);
    query.prepare("SELECT name,unfold_L,unfold_C,unfold_R FROM abstracts WHERE id = ?");
    query.bindValue(0,id);
    query.exec();
    Abstract a={""};
    if(query.next()){
        a.name = query.value(0).toString();
        a.unfold_L_Predicate = query.value(1).toString();
        a.unfold_C_Idea = query.value(2).toString();
        a.unfold_R_Predicate = query.value(3).toString();
    }
    return a;
}

void KnowledgeDB::genFoldedPhenomenon(int depth)
{
    ensureOpen();
    for(int i = 0;i < depth;i++){
        QSqlQuery query(db);
        query.exec("SELECT name FROM abstracts ORDER BY id");
        QList<QString> names;
        while(query.next())
            names.append(query.value(0).toString());
        for(auto &name : names){
            _genFoldedPhenomenon(Abstract{name},depth);
        }
    }
}

//｢<∃F∃,I,∃G∃(∃)> A B｣ Being-∀is-a->I,A-∃F∃->Being-∃G∃(∃)>B（Being=通用具象，本系统以通用抽象 Abstract 顶替；见 docs/00-核心原理）
//<F I G(∃)> <F I <~G,Abstract,is-a>>
void KnowledgeDB::_genFoldedPhenomenon(Abstract a,int depth)
{
    const QString scope = scopePrefix();

           //p0: ｢is-a, a, I｣
    QList<Phenomenon> p0List = queryPhenomena("s.name = ? AND pr.name = 'is-a'",{a.name});
    for(auto p0 : p0List){
        if(p0.object == Abstract{scope}) continue;
        Abstract I = p0.object;
        Abstract innerSubject = p0.subject;

               //p1: 主体为 innerSubject，谓词左右展开均非 is-a，宾语非 innerSubject/I/world
        QList<Phenomenon> p1List = queryPhenomena(
            "s.name = ? AND o.name != ? AND o.name != ? AND o.name != ? "
            "AND pr.unfold_L != 'is-a' AND pr.unfold_R != 'is-a'",
            {innerSubject.name,innerSubject.name,I.name,scope});
        for(auto p1 : p1List){
            //p2: 主体为 innerSubject，宾语非 innerSubject/I/p1.object/world，谓词右展开为 is-a
            QList<Phenomenon> p2List = queryPhenomena(
                "s.name = ? AND o.name != ? AND o.name != ? AND o.name != ? AND o.name != ? "
                "AND pr.unfold_R = 'is-a'",
                {innerSubject.name,innerSubject.name,I.name,p1.object.name,scope});
            for(auto p2 : p2List){
                Abstract p_R{p2.predicate.unfold_L_Predicate};
                p_R = getAbstract(p_R.name);

                Abstract innerSubject2 = constructTempAbs(p2.object,p_R,p2.subject);

                if(innerSubject2 == p1.object) continue;
                if(!innerSubject.name.startsWith(p1.object.name)
                   && innerSubject2.name.startsWith(p1.object.name)) continue;
                if(innerSubject2 == a) continue;


                p_R = revPredicate(p_R);

                if(innerSubject.name.startsWith(p1.object.name)){//p1.object 在 innersubject 上层
                    auto it = p1.object.name.end()-1;
                    int count = 0;
                    QString inner_I_name = "";
                    while(*it == ']'){
                        count++;
                        inner_I_name.prepend(*it);
                        it--;
                    }
                    while(count > 0){
                        inner_I_name.prepend(*it);
                        if(*it == '[') count--;
                        it--;
                    }

                    if(inner_I_name != ""){
                        QString prefix = p1.predicate.name+":"+inner_I_name;
                        if(p_R.name.startsWith(prefix)){
                            continue;
                        }
                    }
                }
                if(predicateDepth(p_R)+predicateDepth(p1.predicate) > depth) continue;


                Abstract new_pred = rExistAbsPredicate(constructPred(p1.predicate,I,p_R));
                addPhenomenon(Phenomenon{new_pred,p1.object,p2.object});
            }
        }
    }
}




Abstract KnowledgeDB::revPredicate(const Abstract &pred){
    Abstract revPred;
    if(!isChainName(pred.name)){
        if(pred.name.startsWith('~')){
            revPred.name=pred.name.sliced(1);
        }else{
            revPred.name = "~" + pred.name;
        }
    }else{
        Abstract RP = getAbstract(pred.unfold_R_Predicate);
        revPred.name = QString("%1:[%2]→%3").arg(revPredicate(RP).name,pred.unfold_C_Idea,pred.unfold_L_Predicate);
    }

    revPred.unfold_L_Predicate = pred.unfold_R_Predicate;
    revPred.unfold_C_Idea = pred.unfold_C_Idea;
    revPred.unfold_R_Predicate = pred.unfold_L_Predicate;

    ensureAbstract(revPred);
    return revPred;
}

Abstract KnowledgeDB::revTempPredicate(const Abstract &pred){
    Abstract revPred;
    if(!isChainName(pred.name)){
        if(pred.name.startsWith('~')){
            revPred.name=pred.name.sliced(1);
        }else{
            revPred.name = "~" + pred.name;
        }
    }else{
        Abstract RP = getAbstract(pred.unfold_R_Predicate);
        revPred.name = QString("%1:[%2]→%3").arg(revTempPredicate(RP).name,pred.unfold_C_Idea,pred.unfold_L_Predicate);
    }

    revPred.unfold_L_Predicate = pred.unfold_R_Predicate;
    revPred.unfold_C_Idea = pred.unfold_C_Idea;
    revPred.unfold_R_Predicate = pred.unfold_L_Predicate;

    return revPred;
}

Abstract KnowledgeDB::constructPred(const Abstract &pred_L, const Abstract &idea_C, const Abstract &pred_R){
    Abstract newPred;
    newPred.name = QString("%1:[%2]→%3").arg(revPredicate(pred_L).name,idea_C.name,pred_R.name);
    newPred.unfold_L_Predicate = pred_L.name;
    newPred.unfold_C_Idea = idea_C.name;
    newPred.unfold_R_Predicate = pred_R.name;

    ensureAbstract(newPred);
    return newPred;
}

Abstract KnowledgeDB::constructTempPred(const Abstract &pred_L, const Abstract &idea_C, const Abstract &pred_R){
    Abstract newPred;
    newPred.name = QString("%1:[%2]→%3").arg(revTempPredicate(pred_L).name,idea_C.name,pred_R.name);
    newPred.unfold_L_Predicate = pred_L.name;
    newPred.unfold_C_Idea = idea_C.name;
    newPred.unfold_R_Predicate = pred_R.name;

    return newPred;
}

Abstract KnowledgeDB::lExistAbsPredicate(const Abstract &pred){
    return constructPred(Abstract{"is-a"},Abstract{"Abstract"},pred);
}
Abstract KnowledgeDB::rExistAbsPredicate(const Abstract &pred){
    return constructPred(revPredicate(pred),Abstract{"Abstract"},Abstract{"is-a"});
}
Abstract KnowledgeDB::rExistAbsTempPredicate(const Abstract &pred){
    return constructTempPred(revTempPredicate(pred),Abstract{"Abstract"},Abstract{"is-a"});
}
Abstract KnowledgeDB::lExistAbsTempPredicate(const Abstract &pred){
    return constructTempPred(Abstract{"is-a"},Abstract{"Abstract"},pred);
}

bool KnowledgeDB::isLExistAbsPred(Abstract pred){
    if(pred.unfold_L_Predicate == "")
        return false;
    if(pred.unfold_C_Idea == "Abstract" && pred.unfold_L_Predicate == "is-a")
        return true;
    return false;
}

bool KnowledgeDB::isRExistAbsPred(Abstract pred){
    if(pred.unfold_R_Predicate == "")
        return false;
    if(pred.unfold_C_Idea == "Abstract" && pred.unfold_R_Predicate == "is-a")
        return true;
    return false;
}

bool KnowledgeDB::isRExistAbsPred(int pred_id){
    Abstract pred = getAbstract(pred_id);
    return isRExistAbsPred(pred);
}

bool KnowledgeDB::isLExistAbsPred(int pred_id){
    Abstract pred = getAbstract(pred_id);
    return isLExistAbsPred(pred);
}

Abstract KnowledgeDB::constructAbs(const Abstract &subject_idea, const Abstract &predicate, const Abstract &object_idea){
    Abstract newAbs,rev_pred=revPredicate(predicate);

    newAbs.name = QString("%3→%2:[%1]").arg(subject_idea.name,rev_pred.name,object_idea.name);

    addPhenomenon(Phenomenon{Abstract{"is-a"}, newAbs, subject_idea});
    addPhenomenon(Phenomenon{predicate, newAbs, object_idea});
    return newAbs;
}

Abstract KnowledgeDB::constructTempAbs(const Abstract &subject_idea, const Abstract &predicate, const Abstract &object_idea){
    Abstract newAbs,rev_pred=revTempPredicate(predicate);

    newAbs.name = QString("%3→%2:[%1]").arg(subject_idea.name,rev_pred.name,object_idea.name);
    return newAbs;
}

// 名字层（转义 / 限定壳 / 链名扫描 / DerivedAbs 末段取用）与库无关，实现见 core/knowledgename.cpp

QList<Abstract> KnowledgeDB::abstract()
{
    ensureOpen();
    QList<Abstract> result;
    QSqlQuery query(db);
    query.exec("SELECT name,unfold_L,unfold_C,unfold_R FROM abstracts ORDER BY id");
    while(query.next()){
        Abstract a{query.value(0).toString()};
        a.unfold_L_Predicate = query.value(1).toString();
        a.unfold_C_Idea = query.value(2).toString();
        a.unfold_R_Predicate = query.value(3).toString();
        result.append(a);
    }
    return result;
}

QList<Phenomenon> KnowledgeDB::phenomena()
{
    return queryPhenomena("");
}

// 指象 [pred subj]：满足｢pred, subj, ?｣的全部宾词（pred 为 is 时即 identify；见 docs/00-核心原理）
QList<Abstract> KnowledgeDB::findObjects(const QString &pred, const QString &subj)
{
    QList<Abstract> result;
    auto phenList = queryPhenomena("pr.name = ? AND s.name = ?",{pred,subj});
    for(auto p : phenList){
        result.append(p.object);
    }
    return result;
}

// 带域的指象：[pred subj] 在给定域下求值（主体先 qualify(subj, scope)）
QList<Abstract> KnowledgeDB::findObjsScoped(const QString &pred, const QString &subj,const Scope &scope)
{
    QString qualifiedSubj = qualify(subj,scope);
    QList<Abstract> result;
    auto Pred = getAbstract(pred);
    auto phenList = queryPhenomena("pr.name = ? AND s.name = ?",{Pred.name,qualifiedSubj});
    for(auto p : phenList){
        result.append(p.object);
    }
    return result;
}

QList<Abstract> KnowledgeDB::findObjsScoped(const QString &pred, const QString &subj, const QString &scope_has)
{
    return findObjsScoped(pred,subj,{scope_has,"has"});
}


void KnowledgeDB::clear()
{
    ensureOpen();
    QSqlQuery query(db);
    query.exec("DELETE FROM phenomena");
    query.exec("DELETE FROM abstracts");
}

QList<Phenomenon> KnowledgeDB::queryPhenomena(const QString &where, const QList<QVariant> &binds)
{
    ensureOpen();
    QList<Phenomenon> result;
    QSqlQuery query(db);
    QString sql = "SELECT pr.name,pr.unfold_L,pr.unfold_C,pr.unfold_R,s.name,o.name "
                  "FROM phenomena p "
                  "JOIN abstracts pr ON pr.id = p.predicate "
                  "JOIN abstracts s ON s.id = p.subject "
                  "JOIN abstracts o ON o.id = p.object ";
    if(!where.isEmpty()) sql += "WHERE " + where + " ";
    sql += "ORDER BY p.id";
    query.prepare(sql);
    for(int i = 0;i < binds.size();i++)
        query.bindValue(i,binds[i]);
    query.exec();
    while(query.next()){
        Phenomenon p;
        p.predicate.name = query.value(0).toString();
        p.predicate.unfold_L_Predicate = query.value(1).toString();
        p.predicate.unfold_C_Idea = query.value(2).toString();
        p.predicate.unfold_R_Predicate = query.value(3).toString();
        p.subject = Abstract{query.value(4).toString()};
        p.object = Abstract{query.value(5).toString()};
        result.append(p);
    }
    return result;
}


// 按三元组名回查现象 id（替代 NoteParser::recordLinePhen）。
int KnowledgeDB::phenomenonId(const QString& pred, const QString& subj, const QString& obj)
{
    ensureOpen();
    QSqlQuery q(db);
    q.prepare("SELECT ph.id FROM phenomena ph "
              "JOIN abstracts pr ON pr.id=ph.predicate "
              "JOIN abstracts s ON s.id=ph.subject "
              "JOIN abstracts o ON o.id=ph.object "
              "WHERE pr.name=? AND s.name=? AND o.name=?");
    q.addBindValue(pred); q.addBindValue(subj); q.addBindValue(obj);
    q.exec();
    return q.next() ? q.value(0).toInt() : -1;
}

int KnowledgeDB::phenomenonId(const Phenomenon &phen)
{
    return phenomenonId(phen.predicate.name,phen.subject.name,phen.object.name);
}

Phenomenon KnowledgeDB::getPhenomenon(int id){
    Phenomenon result;
    if(!queryPhenomena("p.id = ?",{id}).isEmpty())
        result = queryPhenomena("p.id = ?",{id}).first();
    return result;
}

QList<int> KnowledgeDB::queryPhenomenonIds(const QString &where, const QList<QVariant> &binds){
    ensureOpen();
    QList<int> result;
    QSqlQuery query(db);
    QString sql = "SELECT p.id FROM phenomena p "
                  "JOIN abstracts pr ON pr.id = p.predicate "
                  "JOIN abstracts s ON s.id = p.subject "
                  "JOIN abstracts o ON o.id = p.object ";
    if(!where.isEmpty()) sql += "WHERE " + where + " ";
    sql += "ORDER BY p.id";
    query.prepare(sql);
    for(int i = 0;i < binds.size();i++)
        query.bindValue(i,binds[i]);
    query.exec();

    while(query.next()){
        result.append(query.value(0).toInt());
    }
    return result;
}

QList<int> KnowledgeDB::queryLines(const int &rootSubject_id){
    return queryPhenomenonIds("subject = ?",{rootSubject_id});
}

QList<int> KnowledgeDB::queryELines(const int &rootSubject_id){
    auto phenIds = queryLines(rootSubject_id);
    QList<int> rList; // 右(∃)（: 行）优先建树
    QList<int> lList; // 左(∃)（@ 行）随后
    for(auto phenId : phenIds){
        Phenomenon phen = getPhenomenon(phenId);
        if(isRExistAbsPred(phen.predicate))
            rList.append(phenId);
        else if(isLExistAbsPred(phen.predicate))
            lList.append(phenId);
    }
    // 组内按 unqualify 后宾语名本地化字典序（中文按拼音）稳定排序；: 行在前 @ 行在后不变
    const auto objNameUnEncoded = [this](int phenId){
        return unqualify(getPhenomenon(phenId).object.name);
    };
    const auto cmp = [&objNameUnEncoded](int a, int b){
        return QString::localeAwareCompare(objNameUnEncoded(a), objNameUnEncoded(b)) < 0;
    };
    std::stable_sort(rList.begin(), rList.end(), cmp);
    std::stable_sort(lList.begin(), lList.end(), cmp);
    return rList + lList;
}

// 批量建树：两条 SQL 全量载入后内存建树（零 SQL/零写库）；结构/排序同 queryELines（: 行前 @ 行后、宾语本地化字典序），path 防环
namespace {
struct AbsRec
{
    QString l, c, r; // unfold_L / unfold_C / unfold_R
};

struct LoadedEdge
{
    int phenId = -1;
    int subjectId = -1;
    int objectId = -1;
    bool atFlag = false; // true=@ 行（lEAbs），false=: 行（rEAbs）
    QString predUnfoldL;
    QString predUnfoldC;
    QString predUnfoldR;
    QString subjectName;
    QString objectName;
};

// revPredicate 的纯内存版本：命名规则相同，不查库/不 ensureAbstract；map 缺失（异常数据）时该层原样返回
struct RevNamer
{
    const QHash<QString, AbsRec> &absByName;

    QString rev(const QString &name) const
    {
        if(!KnowledgeDB::isChainName(name)) // 叶子谓词名可含转义箭头，不能用裸 contains 判定
        {
            if(name.startsWith('~'))
                return name.sliced(1);
            return '~' + name;
        }
        auto it = absByName.constFind(name);
        if(it == absByName.constEnd())
            return name;
        // 与 revPredicate 同构：rev(R):[C]→L
        return rev(it->r) + QStringLiteral(":[") + it->c + QStringLiteral("]→") + it->l;
    }
};

struct TreeBuilder
{
    const QList<LoadedEdge> &edges;
    const QHash<int, QList<int>> &bySubject;
    const QHash<QString, int> &idByName;
    const RevNamer &revNamer;

    void build(int subjectId, TreeNode *parent, QSet<int> &path)
    {
        const auto it = bySubject.constFind(subjectId);
        if(it == bySubject.constEnd())
            return;
        for(int idx : it.value())
        {
            const LoadedEdge &e = edges[idx];

            // 与入库侧 constructAbs 同构（纯字符串不写库）：: 行 constructAbs(O,unfold_L,S)→S→rev(L):[O]；@ 行→O→rev(R):[S]
            const QString revInner = e.atFlag ? revNamer.rev(e.predUnfoldR)
                                              : revNamer.rev(e.predUnfoldL);
            const QString derivedAbs = e.atFlag
                ? QStringLiteral("%3→%2:[%1]").arg(e.subjectName, revInner, e.objectName)
                : QStringLiteral("%3→%2:[%1]").arg(e.objectName, revInner, e.subjectName);

            TreeNode *node = new TreeNode(e.phenId);
            node->atFlag = e.atFlag;
            node->subjectName = e.subjectName;
            node->objectName = e.objectName;
            node->unfoldedPred = e.atFlag ? e.predUnfoldR : e.predUnfoldL;
            node->revUnfoldedPred = revInner;
            node->derivedAbs = derivedAbs;

            // 递归到派生的抽象（DerivedAbs）；缺失（异常数据）或命中当前路径（环）时退化为叶边
            const int derivedAbsId = idByName.value(derivedAbs, -1);
            if(derivedAbsId >= 0 && !path.contains(derivedAbsId))
            {
                path.insert(derivedAbsId);
                build(derivedAbsId, node, path);
                path.remove(derivedAbsId);
            }
            parent->children.append(node);
        }
    }
};
}

TreeNode *KnowledgeDB::buildTree(int rootSubject_id)
{
    return buildTree(QList<int>{rootSubject_id});
}

TreeNode *KnowledgeDB::buildTree(const QList<int>& rootSubjectIds)
{
    ensureOpen();

    // 1) 全量 abstracts：名→id（递归定位下一层主体）、名→unfold 三元组（纯内存 rev）
    QHash<QString, int> idByName;
    QHash<QString, AbsRec> absByName;
    {
        QSqlQuery q(db);
        q.exec("SELECT id,name,unfold_L,unfold_C,unfold_R FROM abstracts");
        idByName.reserve(40000);
        absByName.reserve(40000);
        while(q.next())
        {
            const int id = q.value(0).toInt();
            const QString name = q.value(1).toString();
            idByName.insert(name, id);
            absByName.insert(name, AbsRec{q.value(2).toString(),
                                          q.value(3).toString(),
                                          q.value(4).toString()});
        }
    }

    // 2) 全量现象边（谓词/主体/宾语名称与谓词 unfold 三元组一并取出）
    QList<LoadedEdge> edges;
    edges.reserve(40000);
    {
        QSqlQuery q(db);
        q.exec("SELECT p.id, p.subject, p.object, "
               "pr.unfold_L, pr.unfold_C, pr.unfold_R, s.name, o.name "
               "FROM phenomena p "
               "JOIN abstracts pr ON pr.id = p.predicate "
               "JOIN abstracts s  ON s.id  = p.subject "
               "JOIN abstracts o  ON o.id  = p.object");
        while(q.next())
        {
            LoadedEdge e;
            e.phenId = q.value(0).toInt();
            e.subjectId = q.value(1).toInt();
            e.objectId = q.value(2).toInt();
            e.predUnfoldL = q.value(3).toString();
            e.predUnfoldC = q.value(4).toString();
            e.predUnfoldR = q.value(5).toString();
            e.subjectName = q.value(6).toString();
            e.objectName = q.value(7).toString();
            // (∃)形式分类（与 isRExistAbsPred/isLExistAbsPred 同判据，依赖 unfold_C）
            const bool isR = (e.predUnfoldC == QStringLiteral("Abstract")
                              && e.predUnfoldR == QStringLiteral("is-a"));
            const bool isL = (e.predUnfoldC == QStringLiteral("Abstract")
                              && e.predUnfoldL == QStringLiteral("is-a"));
            if(!isR && !isL)
                continue; // 非 : / @ 边不入树（queryELines 同样只收 (∃)形式边）
            e.atFlag = isL;
            edges.append(e);
        }
    }

    // 3) 邻接表 subjectId -> 边索引；: 行在前 @ 行在后，组内按 unqualify(宾语) 本地化字典序
    QHash<int, QList<int>> bySubject;
    bySubject.reserve(edges.size() * 2);
    for(int i = 0; i < edges.size(); ++i)
        bySubject[edges[i].subjectId].append(i);

    const auto cmp = [this, &edges](int a, int b)
    {
        return QString::localeAwareCompare(unqualify(edges[a].objectName),
                                           unqualify(edges[b].objectName)) < 0;
    };
    for(auto it = bySubject.begin(); it != bySubject.end(); ++it)
    {
        QList<int> r, l;
        for(int idx : it.value())
            (edges[idx].atFlag ? l : r).append(idx);
        std::stable_sort(r.begin(), r.end(), cmp);
        std::stable_sort(l.begin(), l.end(), cmp);
        it.value() = r + l;
    }

    // 4) 内存递归建树（零 SQL/零写库），path 防环
    TreeNode *root = new TreeNode(-1);
    QSet<int> path;
    RevNamer revNamer{absByName};
    TreeBuilder builder{edges, bySubject, idByName, revNamer};
    for(int rid : rootSubjectIds)
        builder.build(rid, root, path);
    return root;
}

QList<int> KnowledgeDB::queryDirectPrerequisites(int phenId){
    ensureOpen();
    QList<int> result;

    QSqlQuery qurey(db);
    qurey.prepare("SELECT pi.prerequisite FROM phenIntensions pi "
                  "WHERE pi.conditioned=?");
    qurey.addBindValue(phenId);
    qurey.exec();
    while(qurey.next()){
        result.append(qurey.value(0).toInt());
    }

    return result;
}

QList<int> KnowledgeDB::queryDirectPrerequisites(Phenomenon phen){
    return queryDirectPrerequisites(phenomenonId(phen));
}

// 名字层转义与限定壳（encodeName/decodeName/isEscapedAt/containsUnescaped/isChainName/isQualified/qualify/unqualify/expand*）
// 均为纯字符串操作，实现见 core/knowledgename.cpp

// 构造关系存在断言并返回其派生抽象（DerivedAbs，下一段主体）；幂等，展开为：主现象 + 逆 + 裸名端 is-a 自反边 + constructAbs 派生
Abstract KnowledgeDB::relate(const QString& pred, const QString& subj, const QString& obj,
                             bool atFlag)
{
    Abstract predicate{pred}, subject{subj}, object{obj};
    ensureAbstract(predicate);
    ensureAbstract(subject);
    ensureAbstract(object);

    if(!atFlag){
        const Abstract newPred = rExistAbsPredicate(predicate); // "pred:[Abstract]→is-a"
        addPhenomenon(Phenomenon{newPred, subject, object});
        if (!isChainName(obj))
            addPhenomenon(Phenomenon{Abstract{"is-a"}, object, object}); // 裸名宾语 is-a 自反边
        return constructAbs(object, revPredicate(predicate), subject); // → "subj→pred:[obj]"
    }else{
        const Abstract newPred = lExistAbsPredicate(predicate);     // "is-a:[Abstract]→pred"
        addPhenomenon(Phenomenon{newPred, subject, object});
        if (!isChainName(subj))
            addPhenomenon(Phenomenon{Abstract{"is-a"}, subject, subject}); // 裸名主语 is-a 自反边
        return constructAbs(subject, predicate, object);                   // → "obj→~pred:[subj]"
    }
}

Abstract KnowledgeDB::TempRelated(const QString& pred, const QString& subj, const QString& obj,
                             bool atFlag)
{
    Abstract predicate{pred}, subject{subj}, object{obj};

    if(!atFlag){
        const Abstract newPred = rExistAbsTempPredicate(predicate); // "pred:[Abstract]→is-a"
        return constructTempAbs(object, revPredicate(predicate), subject); // → "subj→pred:[obj]"
    }else{
        const Abstract newPred = lExistAbsTempPredicate(predicate);     // "is-a:[Abstract]→pred"
        return constructTempAbs(subject, predicate, object);        // → "obj→~pred:[subj]"
    }
}

//scope

// 确保个体限定声明存在（world→has:[X] 锚点对）；幂等由底层 INSERT OR IGNORE 落到三元组，故每次完整执行四条、不按抽象名提前返回（否则删现象后补不回声明）
void KnowledgeDB::ensureDeclaration(const QString& individualName)
{
    const QString qualified = qualify(individualName);
    if (qualified == individualName)
        return; // 已是限定名或无法限定（如含 →）

    ensureOpen();

    const Abstract world{scopePrefix()};
    const Abstract isa{QStringLiteral("is-a")};
    const Abstract has{QStringLiteral("has")};
    const Abstract X{individualName};

    ensureAbstract(world);
    addPhenomenon(Phenomenon{isa, world, world});       // 1)
    ensureAbstract(has);
    addPhenomenon(Phenomenon{rExistAbsPredicate(has), world, X}); // 2)
    addPhenomenon(Phenomenon{isa, X, X});               // 3)
    constructAbs(X, Abstract{QStringLiteral("~has")}, world);     // 4)
}

// rootsubject+pred+obj+at_flag → 逐段 relate → 返回行主现象 id + DerivedAbs（下一层嵌套行的父主体）。
LineResult KnowledgeDB::createLine(const QString& rootSubject,
                                   const QString& pred,
                                   const QString& obj,
                                   const bool& atFlag)
{
    LineResult result;

    QString cur = rootSubject;
    ensureDeclaration(obj);
    // 根 world 下宾语用裸名（产 world→has:[obj]）；子级下宾语用限定名（产 subj→has:[world→has:[obj]]）
    const QString relObj = (cur == QStringLiteral("world")) ? obj : qualify(obj);
    const Abstract derivedAbs = relate(pred, cur, relObj, atFlag);
    const QString mainSubj = cur;
    cur = derivedAbs.name;
    const QString mainPred = atFlag
                                 ? lExistAbsPredicate(Abstract{pred}).name
                                 : rExistAbsPredicate(Abstract{pred}).name;
    // 行主现象 = (∃)形式复合谓词 + 主语 + relate 实际所用宾语 relObj
    result.phenomenonId = phenomenonId(mainPred, mainSubj, relObj);
    if (result.phenomenonId < 0)
        return result;

    result.derivedAbs = cur;
    return result;
}

LineResult KnowledgeDB::createLine(const QString& rootSubject, const QList<LineSegment>& segments)
{
    LineResult result;
    if (segments.isEmpty())
        return result;

    QString cur = rootSubject;
    for (const LineSegment& seg : segments) {
        ensureDeclaration(seg.obj);
        const QString relObj = (cur == QStringLiteral("world")) ? seg.obj : qualify(seg.obj);
        const Abstract derivedAbs = relate(seg.pred, cur, relObj, seg.atFlag);
        const QString mainSubj = cur;
        cur = derivedAbs.name;
        const QString mainPred = seg.atFlag
                                     ? lExistAbsPredicate(Abstract{seg.pred}).name
                                     : rExistAbsPredicate(Abstract{seg.pred}).name;
        result.phenomenonId = phenomenonId(mainPred, mainSubj, relObj);
        if (result.phenomenonId < 0)
            return result;
    }
    result.derivedAbs = cur;
    return result;
}

QString KnowledgeDB::generateBody(const Abstract &subject, QSet<QString> &visited, const int& level)
{
    QString body="";

    if(visited.contains(subject.name)){
        return body;
    }
    visited.insert(subject.name);

    int times = 0;
    Phenomenon last_p;

    for(auto x : queryPhenomena("s.name = ?",{subject.name})){
        if(isRExistAbsPred(x.predicate)){
            times++,last_p=x;
            Abstract p_L = getAbstract(x.predicate.unfold_L_Predicate);
            Abstract rev_pred = revPredicate(p_L);
            Abstract innerSubject = constructAbs(x.object,p_L,subject);
            QString subBody = generateBody(innerSubject, visited, level+1);

            body += "\n";
            for(int i = 0;i < level;i++)
                body += "  ";
            body += "- ";

            if(rev_pred == Abstract{"has"}){
                body += unqualify(x.object.name);
            }else{
                body += unqualify(rev_pred.name) + " : " + unqualify(x.object.name);
            }

            body += subBody;

        }else if(isLExistAbsPred(x.predicate)){
            times++,last_p=x;
            Abstract p_R = getAbstract(x.predicate.unfold_R_Predicate);
            Abstract innerSubject = constructAbs(subject,p_R,x.object);
            QString subBody = generateBody(innerSubject, visited, level+1);

            body += "\n";
            for(int i = 0;i < level;i++)
                body += "  ";
            body += "- ";

            if(p_R == Abstract{"~has"}){
                body += "@ " + unqualify(x.object.name);
            }else{
                body += unqualify(p_R.name) + " @ " + unqualify(x.object.name);
            }

            body += subBody;
        }
    }

    if(times == 1){
        if(isRExistAbsPred(last_p.predicate)){
            Abstract p_L = getAbstract(last_p.predicate.unfold_L_Predicate);
            Abstract rev_pred = revPredicate(p_L);

            if(rev_pred == Abstract{"is"}){
                QString prefix = "";
                prefix += "\n";
                for(int i = 0;i < level;i++)
                    prefix += "  ";
                prefix += "- is : ";
                body.remove(0,prefix.size());
                body.insert(0," ⇒ ");
            }
        }
    }
    return body;
}

QString KnowledgeDB::generateNote(const Abstract &subject)
{
    QSet<QString> v;
    QString body="- "+unqualify(subject.name);
    QString subBody = generateBody(subject,v,1);
    if(subBody!="") body+=  subBody;
    return body;
}