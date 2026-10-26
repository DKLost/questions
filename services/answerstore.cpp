#include "answerstore.h"

#include "core/questionsql.h"

#include <QDir>
#include <QFile>

namespace {
QString questionDir(int questionId) // 题内资源目录（data/<id>/）
{
    return QStringLiteral("./data/%1").arg(questionId);
}
QString withoutDotSlash(const QString &path) // content 存题内相对路径时可能带或不带前导 "./"，比对/落盘前统一去掉
{
    return path.startsWith(QLatin1String("./")) ? path.mid(2) : path;
}
}

AnswerStore::AnswerStore(QuestionSql *sqlConnection) : questionSql(sqlConnection) {}

QJsonArray AnswerStore::answers(int questionId) const
{
    return questionSql->read_answerJSON(questionId);
}

QJsonObject AnswerStore::answer(int questionId, int row) const
{
    const QJsonArray array = answers(questionId);
    if(row < 0 || row >= array.count())
        return {};
    return array[row].toObject();
}

void AnswerStore::saveAnswers(int questionId, const QJsonArray &answers) const
{
    questionSql->write_answerJSON(questionId, answers);
}

int AnswerStore::rowOfAnswer(int questionId, int answerId) const
{
    const QJsonArray array = answers(questionId);
    for(int row = 0; row < array.count(); ++row)
        if(array[row].toObject().value(FieldId).toInt() == answerId)
            return row;
    return -1;
}

bool AnswerStore::isChainCardType(const QString &type)
{
    return type == TypeAutoChain || type == TypeAutoChainTypst;
}

bool AnswerStore::hasChainCard(const QJsonArray &answers)
{
    return chainCardAnswerId(answers) > 0;
}

int AnswerStore::chainCardAnswerId(const QJsonArray &answers)
{
    for(const QJsonValue &answer : answers)
        if(isChainCardType(answer.toObject().value(FieldType).toString()))
            return answer.toObject().value(FieldId).toInt();
    return -1;
}

int AnswerStore::insertAnswer(int questionId, int row, const QString &content)
{
    QJsonArray array = answers(questionId);
    if(row < 0 || row > array.count())
        row = array.count();

    const int answerId = questionSql->get_max_id("clozes") + 1;
    questionSql->add_cloze(answerId);
    questionSql->inc_cloze_bind_count(answerId);

    QJsonObject answer;
    answer[FieldContent] = content;
    answer[FieldDisplay] = QString();
    answer[FieldId] = answerId;
    answer[FieldPool] = 0;
    answer[FieldType] = TypeAuto;
    array.insert(row, answer);
    saveAnswers(questionId, array);
    questionSql->update_question_state(questionId);
    return answerId;
}

void AnswerStore::removeAnswerAt(int questionId, int row)
{
    QJsonArray array = answers(questionId);
    if(row < 0 || row >= array.count())
        return;
    questionSql->del_cloze(array[row].toObject().value(FieldId).toInt());
    array.removeAt(row);
    saveAnswers(questionId, array);
    questionSql->update_question_state(questionId);
}

// 写回一行；若该行 aid 被改动（改绑到别的 cloze），同步引用计数：新 aid +1、旧 aid 释放
void AnswerStore::replaceAnswerAt(int questionId, int row, const QJsonObject &value)
{
    QJsonArray array = answers(questionId);
    if(row < 0 || row >= array.count())
        return;
    const int oldAnswerId = array[row].toObject().value(FieldId).toInt();
    const int newAnswerId = value.value(FieldId).toInt();
    if(newAnswerId > 0 && newAnswerId != oldAnswerId)
    {
        questionSql->inc_cloze_bind_count(newAnswerId);
        if(oldAnswerId > 0)
            questionSql->del_cloze(oldAnswerId);
    }
    array[row] = value;
    saveAnswers(questionId, array);
}

void AnswerStore::moveAnswerWithinQuestion(int questionId, int fromRow, int toRow)
{
    QJsonArray array = answers(questionId);
    if(fromRow < 0 || fromRow >= array.count())
        return;
    if(toRow >= array.count())
        toRow = array.count() - 1;
    if(toRow < 0)
        toRow = 0;
    const QJsonValue moved = array.at(fromRow);
    array.removeAt(fromRow);
    array.insert(toRow, moved);
    saveAnswers(questionId, array);
}

// 迁移前置校验：目标题已存在同 aid，或迁入链卡而目标已有链卡（每题至多一张链卡）
bool AnswerStore::moveIsBlocked(const QJsonObject &movedAnswer, const QJsonArray &targetAnswers, QString *errorMessage)
{
    const int answerId = movedAnswer.value(FieldId).toInt();
    const bool movingChainCard = isChainCardType(movedAnswer.value(FieldType).toString());
    for(const QJsonValue &answer : targetAnswers)
    {
        const QJsonObject existing = answer.toObject();
        if(existing.value(FieldId).toInt() == answerId)
        {
            *errorMessage = QStringLiteral("目标题目中已存在该答案。");
            return true;
        }
        if(movingChainCard && isChainCardType(existing.value(FieldType).toString()))
        {
            *errorMessage = QStringLiteral("目标题目已有一个链卡，不能再次迁入。");
            return true;
        }
    }
    return false;
}

// 题内附属资源随迁：typst 图按 data/<qId>/<aId>.png 存放；图片答案的 content 是题内相对路径
void AnswerStore::moveAnswerResources(int fromQuestionId, int toQuestionId, int answerId, QJsonObject &movedAnswer)
{
    const QString sourceDir = questionDir(fromQuestionId);
    const QString targetDir = questionDir(toQuestionId);
    QDir().mkpath(targetDir);

    const QString imageName = QStringLiteral("%1.png").arg(answerId);
    if(QFile::exists(sourceDir + QLatin1Char('/') + imageName))
        QFile::rename(sourceDir + QLatin1Char('/') + imageName, targetDir + QLatin1Char('/') + imageName);

    const QString content = movedAnswer.value(FieldContent).toString();
    const QString relativeContent = withoutDotSlash(content); // 归一化后再判断是否指向源题目录
    const QString relativePrefix = withoutDotSlash(sourceDir + QLatin1Char('/'));
    if(relativeContent.isEmpty() || !relativeContent.startsWith(relativePrefix))
        return;
    const QString fileName = relativeContent.mid(relativePrefix.size());
    if(QFile::exists(relativeContent))
        QFile::rename(relativeContent, withoutDotSlash(targetDir + QLatin1Char('/') + fileName));
    const QString dotSlash = content.startsWith(QLatin1String("./")) ? QStringLiteral("./") : QString();
    movedAnswer[FieldContent] = dotSlash + withoutDotSlash(targetDir) + QLatin1Char('/') + fileName;
}

bool AnswerStore::moveAnswerToQuestion(int fromQuestionId, int row, int toQuestionId, QString *errorMessage)
{
    QJsonArray sourceAnswers = answers(fromQuestionId);
    if(row < 0 || row >= sourceAnswers.count())
        return false;

    QJsonObject movedAnswer = sourceAnswers[row].toObject();
    QJsonArray targetAnswers = answers(toQuestionId);
    if(moveIsBlocked(movedAnswer, targetAnswers, errorMessage))
        return false;

    moveAnswerResources(fromQuestionId, toQuestionId, movedAnswer.value(FieldId).toInt(), movedAnswer);

    sourceAnswers.removeAt(row);
    saveAnswers(fromQuestionId, sourceAnswers);
    targetAnswers.append(movedAnswer);
    saveAnswers(toQuestionId, targetAnswers);
    questionSql->update_question_state(fromQuestionId);
    questionSql->update_question_state(toQuestionId);
    return true;
}