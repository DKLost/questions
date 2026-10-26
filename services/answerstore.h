#ifndef ANSWERSTORE_H
#define ANSWERSTORE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class QuestionSql;

// 答案（cloze）与题目 answer.json 的唯一读写出口（避免各处重复"读—改—写"导致口径漂移）；元素形如 {content, display, id, pool, type}，id 是 clozes 表主键。
class AnswerStore
{
public:
    // answer.json 的 type 取值：编辑对话框写入、复习判分读取都引用同一套常量
    inline static const QString TypeAuto = QStringLiteral("auto");
    inline static const QString TypeManual = QStringLiteral("manual");
    inline static const QString TypeManualImage = QStringLiteral("manual(image)");
    inline static const QString TypeAutoTypst = QStringLiteral("auto(typst)");
    inline static const QString TypeAutoChain = QStringLiteral("auto(chain)"); // 链卡：以图谱挂载的 DerivedAbs 裁决答案
    // 链卡(typst)：判分同 auto(chain)（以图谱末段宾词裁决），但输入按 typst 公式对待（实时预览，回传时自动补 `$ … $`）
    inline static const QString TypeAutoChainTypst = QStringLiteral("auto(chain-typst)");

    // answer.json 元素的字段名（文件格式的一部分）
    inline static const QString FieldContent = QStringLiteral("content");
    inline static const QString FieldDisplay = QStringLiteral("display");
    inline static const QString FieldId = QStringLiteral("id");
    inline static const QString FieldPool = QStringLiteral("pool");
    inline static const QString FieldType = QStringLiteral("type");

    explicit AnswerStore(QuestionSql *sqlConnection);

    QJsonArray answers(int questionId) const;      // 该题答案列表（文件缺失时为空数组）
    QJsonObject answer(int questionId, int row) const; // 取一行，越界返回空对象
    void saveAnswers(int questionId, const QJsonArray &answers) const;

    int rowOfAnswer(int questionId, int answerId) const; // aid → 行号，-1 未命中
    static bool isChainCardType(const QString &type); // 是否按"链卡（图谱位置裁决）"对待：auto(chain)、auto(chain-typst)
    static bool hasChainCard(const QJsonArray &answers);
    static int chainCardAnswerId(const QJsonArray &answers); // 链卡 aid，-1 表示无

    // 在 row 处插入一条普通答案（新建 cloze 行，pool=0、type=auto），返回新 aid
    int insertAnswer(int questionId, int row, const QString &content);
    void removeAnswerAt(int questionId, int row);                             // 删除并释放该 cloze 的引用
    void replaceAnswerAt(int questionId, int row, const QJsonObject &value);  // 写回一行；aid 改动时同步引用计数
    void moveAnswerWithinQuestion(int questionId, int fromRow, int toRow);

    // 把第 row 行答案整体迁到另一题（同一 cloze id、FSRS 状态保留），题内附属资源（typst 图、指向本题目录的图片）随迁；失败返回 false 并写 errorMessage
    bool moveAnswerToQuestion(int fromQuestionId, int row, int toQuestionId, QString *errorMessage);

private:
    static bool moveIsBlocked(const QJsonObject &movedAnswer, const QJsonArray &targetAnswers, QString *errorMessage);
    static void moveAnswerResources(int fromQuestionId, int toQuestionId, int answerId, QJsonObject &movedAnswer);

    QuestionSql *questionSql;
};

#endif // ANSWERSTORE_H