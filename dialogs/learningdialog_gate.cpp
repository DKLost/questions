#include "learningdialog.h"
#include "ui_learningdialog.h"
#include "learningdialog_internal.h"
#include "utils/toolfunctions.h"
#include "core/fsrs.h"
#include "core/knowledgedb.h"
#include "widgets/knowledgetreehelper.h"
#include "widgets/questiontableviewhelper.h"
#include <QSet>
#include <QImage>
#include <QUrl>
#include <QPainter>
#include <QTextImageFormat>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QRegularExpressionValidator>
#include <QMessageBox>
#include <QFile>
#include <QSqlError>
#include <QToolButton>
#include <QPushButton>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTextDocument>
#include <QTextBlock>
#include <algorithm>
#include <functional>
#include <QTextFragment>
#include <QTextDocumentFragment>

using namespace learningdialog_internal;

namespace {
constexpr int kDrillStateFilled = 1;
// 诊断开关（默认关）：排查组大提交的记录归属与记账时 dump 批次构成到 fsrs_debug.log，平常保持关闭
constexpr bool kGateDebugTrace = false;
const QString kGateWrongRevealArrow = QStringLiteral(" ✗ → ");
const QString kGateRollHint = QStringLiteral("已提交，再次点击「提交」继续同组下一题");
const QString kGateGroupDoneHint = QStringLiteral("本组完成：%1/%2 对应上（回车切下一题，或自选下一行）");
const QString kGateFinishTitle = QStringLiteral("概念重放");
const QString kGateFinishText = QStringLiteral("本轮完成：共 %1 组，%2 题未对应上（已按 FSRS 安排在当天重来）。\n"
                                               "是否重新复习本轮全部到期题目？");
const QString kGateRetryButton = QStringLiteral("再练");
const QString kGateStopButton = QStringLiteral("结束本轮");
const QString kGateSaveFailedTitle = QStringLiteral("保存失败");
const QString kGateSaveFailedText = QStringLiteral("本组概念重放的保存未完成（已回滚），请再次提交重试。");

// 本题链卡是否按 typst 公式回传（用户只输入公式体）：答案为 auto(chain-typst)
bool chainCardIsTypst(const QJsonArray &answers)
{
    for(const QJsonValue &answer : answers)
        if(answer.toObject().value(AnswerStore::FieldType).toString() == AnswerStore::TypeAutoChainTypst)
            return true;
    return false;
}

// 链卡题的"作答键"（用户该输入什么）：链卡 = 末段宾词；链卡(typst) = 宾词的公式体（两端 $ 与紧贴空白不参与）
QString chainAnswerKeyOf(const QString &derivedAbs, bool typst)
{
    const QString objectText = KnowledgeDB::lastObjectOfDerivedAbs(derivedAbs);
    return typst ? KnowledgeDB::chainTypstBodyOf(objectText) : objectText;
}
}

// ===== 阶段二：分组概念重放（方案见 docs/02） =====

// set_items_table 中调用（取全量后）：扫描链卡题 → 隐藏其子行并锁展开 → 其余即"要复习的现象行"（分组在 buildGateGroups）
void LearningDialog::buildGatePlan()
{
    gateBaseFilter.clear(); // 清空上一轮（含再练重跑）的全部门控状态
    gateHiddenIds.clear();
    gateCardIds.clear();
    gateCards.clear();
    gateAnsweredIds.clear();
    gateLockDerivedAbs.clear();
    gateGroups.clear();
    gateGroupOfQuestion.clear();
    gateRecords.clear();
    gateWrongQIds.clear();
    chainAidByQId.clear();
    gateActive = false;
    gatePendingRollGroup = kNoGroup;

    const QList<ChainCard> cards = scanChainCards();
    const QSet<int> hiddenIds = hideChildRowsOfCards(cards);
    for(int questionId : hiddenIds)
        gateHiddenIds << QString::number(questionId);
    selectGateReviewRows(cards, hiddenIds);
    gateBaseFilter = questionTableModel->filter();
    if(!gateHiddenIds.isEmpty())
        rebuildGateFilter();
}

// 复习集内每题读一次 answer.json：含链卡者记为 ChainCard（顺带填充链卡缓存）
QList<LearningDialog::ChainCard> LearningDialog::scanChainCards()
{
    QList<ChainCard> cards;
    const int absCol = QuestionSql::headerColumnMap[kAbstractNameField];
    for(int row = 0; row < questionTableModel->rowCount(); ++row)
    {
        const int questionId = questionTableModel->index(row, 0).data().toInt();
        const QJsonArray answers = answerStore->answers(questionId);
        const int aid = AnswerStore::chainCardAnswerId(answers);
        chainCardByQId.insert(questionId, aid > 0);
        if(aid > 0)
        {
            chainAidByQId.insert(questionId, aid);
            cards.append({questionId, aid, questionTableModel->index(row, absCol).data().toString(), chainCardIsTypst(answers)});
        }
    }
    return cards;
}

// 链卡题的子行（段边界前缀精确，不用 LIKE——DerivedAbs 名可能含 % _）：子行表行隐藏；有子行者锁定树行展开（子行不显示）
QSet<int> LearningDialog::hideChildRowsOfCards(const QList<ChainCard> &cards)
{
    QSet<int> hiddenIds;
    const int absCol = QuestionSql::headerColumnMap[kAbstractNameField];
    for(const ChainCard &card : cards)
    {
        if(card.derivedAbs.isEmpty())
            continue;
        bool hasChild = false;
        for(int row = 0; row < questionTableModel->rowCount(); ++row)
        {
            const int childId = questionTableModel->index(row, 0).data().toInt();
            const QString childDerivedAbs = questionTableModel->index(row, absCol).data().toString();
            if(childId == card.questionId)
                continue;
            if(ToolFunctions::isDerivedAbsUnder(childDerivedAbs, card.derivedAbs))
            {
                hiddenIds.insert(childId);
                hasChild = true;
            }
        }
        if(hasChild)
            gateLockDerivedAbs.insert(card.derivedAbs);
    }
    return hiddenIds;
}

// 可见链卡题 = 本次的复习行（被上层链卡题隐藏的卡留待上层复习良好后的下次复习）
void LearningDialog::selectGateReviewRows(const QList<ChainCard> &cards, const QSet<int> &hiddenIds)
{
    for(const ChainCard &card : cards)
    {
        if(hiddenIds.contains(card.questionId))
            continue;
        gateCardIds.insert(card.questionId);
        gateCards.append(card);
    }
}

// 把可见链卡题按"同一主词 + 同一关系段"分组；题目表不按组收窄，用户可自选想复习的组与行
void LearningDialog::buildGateGroups()
{
    gateGroups.clear();
    gateGroupOfQuestion.clear();
    if(gateCards.isEmpty())
        return;

    QStringList derivedAbsList;
    QHash<QString, int> questionIdByDerivedAbs;
    for(const ChainCard &card : gateCards)
    {
        derivedAbsList << card.derivedAbs;
        questionIdByDerivedAbs.insert(card.derivedAbs, card.questionId);
    }

    const QList<QStringList> derivedAbsGroups = groupDerivedAbsBySubjectRelation(ui->phenTreeView, derivedAbsList);
    for(const QStringList &derivedAbsGroup : derivedAbsGroups)
    {
        QList<int> group;
        for(const QString &derivedAbs : derivedAbsGroup)
        {
            const int questionId = questionIdByDerivedAbs.value(derivedAbs, kNoQuestion);
            if(questionId > 0)
                group << questionId;
        }
        if(group.isEmpty())
            continue;
        const int groupIndex = gateGroups.size();
        for(int questionId : group)
            gateGroupOfQuestion.insert(questionId, groupIndex);
        gateGroups.append(group);
    }
}

// 按 原始 filter + 被隐藏子行 + 已答离表题 重载题目表（普通题与其余组的复习行始终可见）
void LearningDialog::rebuildGateFilter()
{
    QStringList exclude = gateHiddenIds;
    for(int questionId : gateAnsweredIds)
        exclude << QString::number(questionId);
    questionTableModel->setFilter(ToolFunctions::andFilters(
        gateBaseFilter, ToolFunctions::idNotInFilter(QStringLiteral("questions.id"), exclude)));
    reloadQuestionTable();
}

// 正式提交的链卡记录：预提交只锁输入不揭示不记录；组满立刻对该组大提交，组未满则挂起"待回车切换"
void LearningDialog::gateRecordAnswer()
{
    if(currentQuestionId < 0 || !gateCardIds.contains(currentQuestionId)
       || gateAnsweredIds.contains(currentQuestionId))
        return;

    GateRecord record;
    record.questionId = currentQuestionId;
    record.groupIndex = gateGroupOfQuestion.value(currentQuestionId, kNoGroup);
    record.chainAid = chainAidByQId.value(currentQuestionId, kNoQuestion);
    record.derivedAbs = questionSql->get_value(kQuestionsTable, currentQuestionId, kAbstractNameField).toString();
    record.input = gateInput ? gateInput->text().trimmed() : QString();
    record.typst = chainCardIsTypst(currentAnswers);
    record.rebuilt = record.input.isEmpty() ? QString()
                                            : KnowledgeDB::replaceLastObjectOfDerivedAbs(record.derivedAbs, record.input);
    record.timeMs = answerElapsed.msecsSinceStartOfDay();
    gateRecords.append(record);

    writeGateRowText(record.derivedAbs, record.input.isEmpty() ? kMaskedBlankText : record.input, kDrillStateFilled); // 回填不揭示对错
    gateAnsweredIds.insert(currentQuestionId);
    gateActive = false;
    stop_timer();
    lastAnsweredRowIndex = ui->tableView->currentIndex().row(); // 与普通题一致：先记行号，供"提交后再回车"切换

    rebuildGateFilter(); // 本题离表；同组其余行与别的组仍在表内
    refreshAnswerMasks();

    if(gateMaybeSubmitGroup(record.groupIndex))
    {
        gatePendingRollGroup = kNoGroup;
        if(lastAnsweredRowIndex < 0 || lastAnsweredRowIndex >= questionTableModel->rowCount()) // 行号可能越界：与普通题一致夹紧
            lastAnsweredRowIndex = 0;
        if(questionTableModel->rowCount() > 0)
            ui->tableView->selectRow(lastAnsweredRowIndex);
        return; // 组满：组大提交后停在收尾界面，用户可自选下一组、或再回车切下一题
    }
    gatePendingRollGroup = record.groupIndex; // 组未满：挂起，下一次点击「提交/回车」才切到同组下一未答行
    if(gateInput)
        gateInput->setPlaceholderText(kGateRollHint);
}

// 载入同组下一未答行（组内树序）：交给 set_question 展开选中并重入作答态，故无需再点一次表行
bool LearningDialog::gateReviewNextRow(int groupIndex)
{
    if(groupIndex < 0 || groupIndex >= gateGroups.size())
        return false;
    for(int questionId : gateGroups[groupIndex])
    {
        if(gateAnsweredIds.contains(questionId))
            continue;
        const int row = ToolFunctions::rowOfValue(questionTableModel, questionId);
        if(row < 0)
            continue; // 该行不在表内（尚未成为可见复习行）
        ui->tableView->selectRow(row);
        set_question(questionId);
        return true;
    }
    return false;
}

// 该组是否已全部作答：是则立刻组大提交（把整轮大提交切成多次小组提交），返回是否已提交
bool LearningDialog::gateMaybeSubmitGroup(int groupIndex)
{
    if(groupIndex < 0 || groupIndex >= gateGroups.size())
        return false;
    for(int questionId : gateGroups[groupIndex])
        if(!gateAnsweredIds.contains(questionId))
            return false; // 本组还有未作答的行
    gateSubmitGroup(groupIndex);

    for(const QList<int> &group : gateGroups) // 全部组都完成后收尾
        for(int questionId : group)
            if(!gateAnsweredIds.contains(questionId))
                return true;
    gateFinishRolling();
    return true;
}

// 显示名（lastObjectOfDerivedAbs，与揭示口径一致）→ 题 id；链卡(typst) 改用公式体（用户输入的就是公式体）；questionIds 为空 = 本轮全部复习行
QHash<QString, int> LearningDialog::gateAnswerOwners(const QList<int> &questionIds) const
{
    QHash<QString, int> owners;
    for(const ChainCard &card : gateCards)
    {
        if(card.derivedAbs.isEmpty() || (!questionIds.isEmpty() && !questionIds.contains(card.questionId)))
            continue;
        owners.insert(chainAnswerKeyOf(card.derivedAbs, card.typst), card.questionId);
    }
    return owners;
}

QString LearningDialog::gateDerivedAbsOfQuestion(int questionId) const
{
    for(const ChainCard &card : gateCards)
        if(card.questionId == questionId && !card.derivedAbs.isEmpty())
            return card.derivedAbs;
    return questionSql->get_value(kQuestionsTable, questionId, kAbstractNameField).toString();
}

// 实际输入归属：① 本组显示名 ② 本轮复习行显示名 ③ 重建名全表精确查找；都未命中（含留空）→ 判本题 wrong
QList<LearningDialog::GateLanding> LearningDialog::resolveGateLandings(const QList<GateRecord> &records, int groupIndex)
{
    const QHash<QString, int> groupOwners = gateAnswerOwners(gateGroups.value(groupIndex));
    const QHash<QString, int> reviewOwners = gateAnswerOwners({});
    QList<GateLanding> landings;
    for(const GateRecord &record : records)
    {
        GateLanding landing;
        if(!record.input.isEmpty())
        {
            int ownerId = groupOwners.value(record.input, kNoQuestion);
            if(ownerId < 0)
                ownerId = reviewOwners.value(record.input, kNoQuestion);
            if(ownerId < 0)
                ownerId = record.typst ? questionIdByChainTypstBody(record.input) // 公式体没有唯一拼法，只能全表按体比对
                                       : questionIdByDerivedAbsName(record.rebuilt);
            if(ownerId > 0)
            {
                landing.questionId = ownerId;
                landing.aid = AnswerStore::chainCardAnswerId(answerStore->answers(ownerId)); // 记账对象 = 命中题目的链卡
                landing.matched = landing.aid > 0;
            }
        }
        landings.append(landing);
    }
    return landings;
}

// 单事务批量记账：命中 → 命中题的链卡按本题用时评级；未命中 → 本题自己的链卡判 wrong（用时 0）
bool LearningDialog::writeGateGroupToDatabase(const QList<GateRecord> &records, const QList<GateLanding> &landings)
{
    QSqlDatabase db = questionSql->getDb();
    bool ok = db.transaction();
    QSet<int> touchedQuestionIds;
    QSet<int> writtenAids;      // 同一空在一批里只写一次
    QSet<int> creditedQIds;     // 本批"答案已由某条记录给出"的题（含兄弟行帮忙答的情形）
    for(const GateLanding &landing : landings)
        if(landing.matched)
            creditedQIds.insert(landing.questionId);
    // 三趟写，保证"记对"永远优先于"记错"、且同一个空只落一次：
    //   ① 命中且就是本题 → 按本题用时记对（正常情形）
    //   ② 命中但是兄弟行 → 按归属给兄弟记对（输入对应的名字命中谁的卡就记谁）
    //   ③ 本题没答对（留空 / 未命中 / 答的是兄弟）**且本批没人给出它的答案** → 本题链卡按 wrong、用时 0 记
    // ③ 必须排除"已被兄弟认领"的题：否则兄弟刚记对的同一行会被自己的空记录判错（见 docs/05 #37）
    for(int phase = 0; phase < 3 && ok; ++phase)
    {
        for(int i = 0; i < records.size() && ok; ++i)
        {
            const GateRecord &record = records[i];
            const GateLanding &landing = landings[i];
            const bool matchedSelf = landing.matched && landing.questionId == record.questionId;
            if(phase < 2)
            {
                if(!landing.matched || matchedSelf != (phase == 0))
                    continue;
                if(landing.aid <= 0 || writtenAids.contains(landing.aid))
                    continue;
                writtenAids.insert(landing.aid);
                questionSql->update_cloze_state(landing.aid, QTime::fromMSecsSinceStartOfDay(record.timeMs));
                touchedQuestionIds.insert(landing.questionId);
                continue;
            }
            if(matchedSelf || creditedQIds.contains(record.questionId)
               || record.chainAid <= 0 || writtenAids.contains(record.chainAid))
                continue;
            writtenAids.insert(record.chainAid);
            questionSql->update_cloze_state(record.chainAid, QTime::fromMSecsSinceStartOfDay(0));
            touchedQuestionIds.insert(record.questionId);
        }
    }
    if(ok)
        for(int questionId : touchedQuestionIds)
            questionSql->update_question_state(questionId);
    if(ok)
        ok = db.commit();
    if(!ok)
        db.rollback();
    return ok;
}

// 揭示写在"实际记账的那一行"：命中行绿；本题没答对且本批没人给出它的答案时本题行红
void LearningDialog::revealGateGroupResults(const QList<GateRecord> &records, const QList<GateLanding> &landings)
{
    QSet<int> creditedQIds; // 本批答案已给出的题（含"在兄弟行上作答"的）
    for(const GateLanding &landing : landings)
        if(landing.matched)
            creditedQIds.insert(landing.questionId);
    for(int i = 0; i < records.size(); ++i)
    {
        const GateRecord &record = records[i];
        const GateLanding &landing = landings[i];
        const bool matchedSelf = landing.matched && landing.questionId == record.questionId;
        const QString objectText = record.input.isEmpty() ? kMaskedBlankText : record.input;
        if(landing.matched) // 命中（自己或兄弟）：命中行绿显示输入
            writeGateRowText(gateDerivedAbsOfQuestion(landing.questionId), objectText, kDrillStateCorrect);
        if(matchedSelf || creditedQIds.contains(record.questionId))
            continue; // 本题的答案本批已给出（自己答的或兄弟行代答的）：不标红，避免把刚记对的行覆盖成错
        writeGateRowText(record.derivedAbs, objectText + kGateWrongRevealArrow + KnowledgeDB::lastObjectOfDerivedAbs(record.derivedAbs),
                         kDrillStateWrong);
        gateWrongQIds << record.questionId;
    }
}

// 组大提交：按实际输入归属记账并在实际记账行上揭示；失败回滚保留记录可重试
void LearningDialog::gateSubmitGroup(int groupIndex)
{
    if(groupIndex < 0 || groupIndex >= gateGroups.size())
        return;

    QList<GateRecord> records; // 只取本组记录
    for(const GateRecord &record : gateRecords)
        if(record.groupIndex == groupIndex)
            records.append(record);
    if(records.isEmpty())
        return;

    if(kGateDebugTrace) // 批次构成：该组成员 + 每条记录的目标（判断"某空为何没被写"）
    {
        QStringList members;
        for(int questionId : gateGroups.value(groupIndex))
            members << QString::number(questionId);
        QFile log(QStringLiteral("fsrs_debug.log"));
        if(log.open(QIODevice::Append | QIODevice::Text))
        {
            log.write(QStringLiteral("[batch] group=%1 members=[%2] records=%3\n")
                          .arg(groupIndex).arg(members.join(QLatin1Char(','))).arg(records.size()).toUtf8());
            for(const GateRecord &record : records)
                log.write(QStringLiteral("    qid=%1 chainAid=%2 timeMs=%3 typst=%4 input=%5\n")
                              .arg(record.questionId).arg(record.chainAid).arg(record.timeMs)
                              .arg(record.typst).arg(record.input.isEmpty() ? QStringLiteral("(空)") : record.input)
                              .toUtf8());
        }
    }

    const QList<GateLanding> landings = resolveGateLandings(records, groupIndex);
    const int groupCorrect = int(std::count_if(landings.cbegin(), landings.cend(),
                                               [](const GateLanding &landing) { return landing.matched; }));
    if(!writeGateGroupToDatabase(records, landings))
    {
        QMessageBox::warning(this, kGateSaveFailedTitle, kGateSaveFailedText);
        return; // 记录不丢，保持待提交状态
    }
    revealGateGroupResults(records, landings);
    updateAccuracyLabel(groupCorrect, records.size());
    if(gateInput)
        gateInput->setPlaceholderText(kGateGroupDoneHint.arg(groupCorrect).arg(records.size()));

    for(int i = gateRecords.size() - 1; i >= 0; --i) // 本组记录已写回，移出待提交队列
        if(gateRecords[i].groupIndex == groupIndex)
            gateRecords.removeAt(i);
}

// 全部组完成：解锁展开（可展开查看红绿揭示）、刷新题目表；判错题已按 FSRS 安排当天到期，可重跑本轮复习集再练
void LearningDialog::gateFinishRolling()
{
    gateActive = false;
    gatePendingRollGroup = kNoGroup;
    gateLockDerivedAbs.clear(); // 解锁：子行题仍在表外，展开只用于查看揭示结果

    answerStage = AnswerStage::Submitted;
    lastAnsweredRowIndex = ui->tableView->currentIndex().row();
    reloadQuestionTable();
    refreshAnswerMasks();
    if(lastAnsweredRowIndex < 0 || lastAnsweredRowIndex >= questionTableModel->rowCount())
        lastAnsweredRowIndex = 0;
    if(questionTableModel->rowCount() > 0)
        ui->tableView->selectRow(lastAnsweredRowIndex);

    QMessageBox box(this);
    box.setWindowTitle(kGateFinishTitle);
    box.setText(kGateFinishText.arg(gateGroups.size()).arg(gateWrongQIds.size()));
    QPushButton *retryButton = box.addButton(kGateRetryButton, QMessageBox::AcceptRole);
    box.addButton(kGateStopButton, QMessageBox::RejectRole);
    box.exec();
    if(box.clickedButton() == retryButton)
        restartReview();
}

// 再练：重跑本轮复习集；判错题已被 FSRS 排到当天，复习内容全交给 FSRS 决定，不另建集合
void LearningDialog::restartReview()
{
    set_items_table(gateBaseFilter);
}

// 关闭对话框：本轮未完成——已答但未组大提交的记录全部作废（零 FSRS 写入），树行回填恢复 "?"（已揭示的不会走到这里）
void LearningDialog::discardGateCollection()
{
    for(const GateRecord &record : gateRecords)
        if(!record.input.isEmpty())
            writeGateRowText(record.derivedAbs, kMaskedBlankText, kDrillStateNone);
    gateRecords.clear();
    gateCardIds.clear();
    gateCards.clear();
    gateAnsweredIds.clear();
    gateHiddenIds.clear();
    gateLockDerivedAbs.clear();
    gateGroups.clear();
    gateGroupOfQuestion.clear();
    gateWrongQIds.clear();
    gateActive = false;
    gateInput = nullptr;
    gateChainRow = kNoGateRow;
    gatePendingRollGroup = kNoGroup;
}