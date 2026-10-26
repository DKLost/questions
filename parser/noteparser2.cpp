#include "noteparser2.h"

namespace
{
constexpr int kSpacesPerLevel = 2;
constexpr int kRootLevel = 0;
constexpr int kFirstBodyLevel = 1;
constexpr char kSpaceSymbol = ' ';
constexpr char kAtSymbol = '@';
constexpr char kColonSymbol = ':';
constexpr char kNewLineSymbol = '\n';
const char *const kListMarker = "-";
const char *const kImpliesText = "⇒";
const char *const kLineSeparator = "\n";
const QString kIsA = QStringLiteral("is-a");
const QString kWorld = QStringLiteral("world");
const QString kHas = QStringLiteral("has");
const QString kIs = QStringLiteral("is");
}

// 行首空格换算缩进层级：每 kSpacesPerLevel 个空格一级，奇数个空格视为非法返回 -1
int NoteParser::lineIndentLevel(const QString &line) const
{
    int spaceCount = 0;
    for(int position = 0; line.at(position) == kSpaceSymbol; ++position)
        ++spaceCount;
    return spaceCount % kSpacesPerLevel == 0 ? spaceCount / kSpacesPerLevel : -1;
}

void NoteParser::skipSpaces()
{
    while(m_position < m_input.size() && m_input.at(m_position).isSpace())
        ++m_position;
}

// 只跳过水平空白，遇换行停住以免跨行吞并
void NoteParser::skipSpacesNoEnter()
{
    while(m_position < m_input.size() && m_input.at(m_position).isSpace())
    {
        if(m_input.at(m_position) != kNewLineSymbol)
            ++m_position;
    }
}

Abstract NoteParser::readAbs()
{
    skipSpacesNoEnter();
    const int start = m_position;
    while(m_position < m_input.size() && !m_input.at(m_position).isSpace())
        ++m_position;
    const int end = m_position;
    skipSpacesNoEnter();
    return Abstract{KnowledgeDB::encodeName(m_input.mid(start, end - start))}; // 叶子名转义后入库，结构元符号不再冲突
}

// 跳过触发本语句的初始谓词；若跟 @ 则取逆谓词并返回被否定
bool NoteParser::readPredicatePrefix(Abstract &predicate)
{
    skipSpacesNoEnter();
    ++m_position;
    skipSpacesNoEnter();
    if(m_input.at(m_position) != kAtSymbol)
        return false;

    predicate = KnowledgeDB::instance()->revPredicate(predicate);
    ++m_position;
    return true;
}

// 读取宾语：临时名后接 :/@ 时按主语/否定形式改写谓词，否则临时名即宾语
Abstract NoteParser::readObject(Abstract &predicate, bool &isNegated)
{
    const Abstract temporaryObject = readAbs();
    skipSpacesNoEnter();
    if(m_position >= m_input.size())
        return temporaryObject;

    const QChar separator = m_input.at(m_position);
    if(separator == kColonSymbol)
    {
        ++m_position;
        predicate = temporaryObject;
        return readAbs();
    }
    if(separator == kAtSymbol)
    {
        isNegated = true;
        ++m_position;
        return readAbs();
    }
    return temporaryObject;
}

// 解析一条 body 语句并返回派生的抽象（DerivedAbs），供下级缩进继续挂接
Abstract NoteParser::parseChunk(Abstract subject, Abstract defaultPred)
{
    KnowledgeDB *db = KnowledgeDB::instance();
    Abstract predicate = defaultPred;
    db->ensureAbstract(predicate);

    bool isNegated = readPredicatePrefix(predicate);
    const Abstract object = readObject(predicate, isNegated);
    if(!KnowledgeDB::isChainName(object.name))
        db->addPhenomenon(Phenomenon{Abstract{kIsA}, object, object});

    Abstract newSubject = db->createLine(subject.name, {{predicate.name, object.name, isNegated}}).derivedAbs;
    if(m_position < m_input.size() && m_input.at(m_position) == kImpliesText)
        newSubject = parseChunk(newSubject, Abstract{kIs});

    return newSubject;
}

void NoteParser::parseBody(Abstract &subject, int level)
{
    while(m_lineIndex < m_lines.size())
    {
        if(m_lines[m_lineIndex].isEmpty())
        {
            ++m_lineIndex;
            continue;
        }
        if(lineIndentLevel(m_lines[m_lineIndex]) < level)
            return;

        m_input = m_lines[m_lineIndex];
        m_position = 0;
        skipSpaces();
        Abstract newSubject = parseChunk(subject, Abstract{kHas});
        skipSpaces();
        ++m_lineIndex;
        parseBody(newSubject, level + 1);
    }
}

// 解析根行：声明主体，再把其后缩进体递归挂到它下面
void NoteParser::parseRootLine()
{
    KnowledgeDB *db = KnowledgeDB::instance();
    m_position = 0;
    m_input = m_lines[m_lineIndex];
    if(m_input.at(m_position) == kListMarker)
        ++m_position;

    Abstract subject = readAbs();
    db->ensureAbstract(subject);
    if(!KnowledgeDB::isChainName(subject.name))
        db->addPhenomenon(Phenomenon{Abstract{kIsA}, subject, subject});
    ++m_lineIndex;

    db->ensureDeclaration(subject.name);
    Abstract qualifiedSubject{db->qualify(subject.name)};
    parseBody(qualifiedSubject, kFirstBodyLevel);
}

void NoteParser::parse(const QString &input)
{
    KnowledgeDB *db = KnowledgeDB::instance();
    db->clear();
    db->ensureAbstract(Abstract{kIsA});
    db->ensureAbstract(Abstract{kWorld});

    m_lines = input.split(kLineSeparator);
    while(m_lineIndex < m_lines.size())
    {
        if(m_lines[m_lineIndex].isEmpty())
        {
            ++m_lineIndex;
            continue;
        }
        if(lineIndentLevel(m_lines[m_lineIndex]) == kRootLevel)
            parseRootLine();
    }
}