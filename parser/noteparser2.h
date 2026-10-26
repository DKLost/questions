#ifndef NOTEPARSER2_H
#define NOTEPARSER2_H
#include <QString>
#include "core/knowledgedb.h"

class NoteParser
{
public:
    NoteParser() = default;

    void parse(const QString &input); // 不做错误处理
    Abstract readAbs();

private:
    bool readPredicatePrefix(Abstract &predicate); // 跳过初始谓词并按 @ 判断否定形式
    Abstract readObject(Abstract &predicate, bool &isNegated); // 读取宾语，必要时改写谓词为临时名
    Abstract parseChunk(Abstract subject, Abstract defaultPred);
    void parseBody(Abstract &subject, int level);
    void parseRootLine();
    int lineIndentLevel(const QString &line) const; // 行首空格换算缩进层级，非法返回 -1
    void skipSpaces();
    void skipSpacesNoEnter();

    QString m_input;
    int m_position = 0;
    QList<QString> m_lines;
    int m_lineIndex = 0;
};

#endif // NOTEPARSER2_H