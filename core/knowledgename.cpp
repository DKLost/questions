// 名字层（纯字符串，零库访问）：叶子名转义、限定壳包裹/剥壳、链名扫描、DerivedAbs 末段取用——从 knowledgedb.cpp 拆出，声明仍在 knowledgedb.h
#include "knowledgedb.h"

// 取括号深度 0 的最后一个 →：深层末段宾词是（含 → 的）限定名，lastIndexOf 会切到内层箭头；转义字符不参与计数
static int lastTopLevelArrow(const QString &derivedAbs)
{
    int depth = 0;
    int last = -1;
    for(int i = 0; i < derivedAbs.size(); ++i)
    {
        const QChar c = derivedAbs.at(i);
        if(c == QLatin1Char('\\')) // 跳过被转义字符，其字面量不参与计数
        {
            ++i;
            continue;
        }
        if(c == QLatin1Char('['))
            depth++;
        else if(c == QLatin1Char(']'))
            depth--;
        else if(depth == 0 && c == QChar(0x2192)) // →
            last = i;
    }
    return last;
}

// 结构段 →has: 缩写为 →；跳过被转义的箭头，避免误伤叶子名中的字面量 →has:
static QString abbreviateHas(const QString &text)
{
    QString result = text;
    const QString from = QStringLiteral("→has:");
    for(int pos = 0; (pos = result.indexOf(from, pos)) >= 0; )
    {
        if(KnowledgeDB::isEscapedAt(result, pos))
            pos += from.size();
        else
        {
            result.replace(pos, from.size(), QStringLiteral("→"));
            ++pos;
        }
    }
    return result;
}

QString KnowledgeDB::lastObjectOfDerivedAbs(const QString &derivedAbs)
{
    const int arrow = lastTopLevelArrow(derivedAbs);
    if(arrow < 0)
        return derivedAbs;
    const QString tail = derivedAbs.mid(arrow + 1); // 末段：关系段:[宾词]
    const int lb = tail.indexOf(QStringLiteral(":["));
    if(lb < 0 || !tail.endsWith(QStringLiteral("]")))
        return derivedAbs;
    const QString raw = tail.mid(lb + 2, tail.size() - lb - 3);
    // 末段宾词可能是限定名（如 world→has:[@ 名字]）；显示名须与建树一致做 unqualify：递归剥外壳并把 →has: 缩为 →
    return decodeName(abbreviateHas(expandRecursively(raw))); // 出口即显示名，调用方无需再解码
}

QString KnowledgeDB::replaceLastObjectOfDerivedAbs(const QString &derivedAbs, const QString &newObjectUnEncoded)
{
    const int arrow = lastTopLevelArrow(derivedAbs);
    if(arrow < 0)
        return {};
    const QString tail = derivedAbs.mid(arrow + 1);
    const int lb = tail.indexOf(QStringLiteral(":["));
    if(lb < 0 || !tail.endsWith(QStringLiteral("]")))
        return {};
    const QString raw = tail.mid(lb + 2, tail.size() - lb - 3);
    // 与写入侧 createLine 同构：原段为限定名时用户输入也须先包裹 world→has:[…] 才能重放出同名（输入是 unqualify 后裸名）
    const bool wasQualified = expandRecursively(raw) != raw;
    const QString encoded = encodeName(newObjectUnEncoded); // 用户输入为显示名，回填前先转义
    const QString spliced = wasQualified
                                ? QStringLiteral("world→has:[%1]").arg(encoded)
                                : encoded;
    return derivedAbs.left(arrow + 1) + tail.left(lb + 2) + spliced + QStringLiteral("]");
}

// 链卡(typst)：只认两端都被 $ 包裹的宾词名（`$ 公式体 $`）；`$ x $ y` 这类混排不做处理
bool KnowledgeDB::isChainTypstName(const QString &objectText)
{
    return objectText.size() > 2
           && objectText.startsWith(QLatin1Char('$'))
           && objectText.endsWith(QLatin1Char('$'));
}

QString KnowledgeDB::chainTypstBodyOf(const QString &objectText)
{
    return isChainTypstName(objectText) ? objectText.mid(1, objectText.size() - 2).trimmed() : objectText;
}

// 名字层转义：结构元符号 \ [ ] → 前加反斜杠（\ 自身也转义，保证单趟可逆）
QString KnowledgeDB::encodeName(const QString &nameUnEncoded)
{
    QString out;
    out.reserve(nameUnEncoded.size());
    for(const QChar c : nameUnEncoded)
    {
        if(c == QLatin1Char('\\') || c == QLatin1Char('[') || c == QLatin1Char(']') || c == QChar(0x2192))
            out += QLatin1Char('\\');
        out += c;
    }
    return out;
}

QString KnowledgeDB::decodeName(const QString &name)
{
    QString out;
    out.reserve(name.size());
    for(int i = 0; i < name.size(); ++i)
    {
        if(name.at(i) == QLatin1Char('\\') && i + 1 < name.size())
            ++i; // 跳过转义符，取被转义字符的字面量
        out += name.at(i);
    }
    return out;
}

// 前导反斜杠个数为奇数即被转义
bool KnowledgeDB::isEscapedAt(const QString &text, int index)
{
    int backslashes = 0;
    for(int i = index - 1; i >= 0 && text.at(i) == QLatin1Char('\\'); --i)
        ++backslashes;
    return backslashes % 2 == 1;
}

bool KnowledgeDB::containsUnescaped(const QString &text, QChar c)
{
    for(int i = 0; i < text.size(); ++i)
        if(text.at(i) == c && !isEscapedAt(text, i))
            return true;
    return false;
}

bool KnowledgeDB::isChainName(const QString &name)
{
    return containsUnescaped(name, QChar(0x2192));
}

bool KnowledgeDB::isQualified(const QString &name)
{
    const QString prefix = scopePrefix() + QStringLiteral("→has:[");
    if (!name.startsWith(prefix) || !name.endsWith(QLatin1Char(']')))
        return false;
    // 恰为一层包裹：去掉前缀后，首字符 "[" 的深度到结尾才归零（跳过被转义括号）
    int depth = 0;
    const int bodyStart = prefix.size() - 1; // 指向 '['
    for (int i = bodyStart; i < name.size(); ++i) {
        const QChar c = name.at(i);
        if (c == QLatin1Char('\\')) { ++i; continue; }
        if (c == QLatin1Char('[')) ++depth;
        else if (c == QLatin1Char(']')) {
            --depth;
            if (depth == 0)
                return i == name.size() - 1;
        }
    }
    return false;
}

QString KnowledgeDB::qualify(const QString &name)
{
    // 空名不可被包成 world→has:[]（畸形锚点）；扫描器不产空 token，此守卫只防御外部直接误用
    if (name.isEmpty())
        return name;
    if (isQualified(name))
        return name;
    return scopePrefix() + QStringLiteral("→has:[") + name + QStringLiteral("]");
}

QString KnowledgeDB::qualify(const QString &name,const Scope& scope)
{
    return scope.subj + QStringLiteral("→%1:[").arg(scope.pred) + name + QStringLiteral("]");
}

// 从 pos 处剥掉一层「prefix…]」外壳（prefix 末字符为 '['，内层仍为转义形式）。
// 调用方保证 pos 处确为 prefix 起点（或 pos<0 表示"无此壳"）；括号不配平时原样返回。
// 扫描跳过被转义括号，避免被叶子名里的字面 [ ] 提前截断。
static QString spliceShell(const QString &input, int pos, const QString &prefix)
{
    if(pos < 0)
        return input;
    int depth = 0;
    for(int i = pos + prefix.size() - 1; i < input.size(); ++i) // 从 prefix 末尾的 '[' 起
    {
        const QChar c = input.at(i);
        if(c == QLatin1Char('\\')) { ++i; continue; } // 跳过被转义字符
        if(c == QLatin1Char('[')) ++depth;
        else if(c == QLatin1Char(']'))
        {
            --depth;
            if(depth == 0)
                return input.left(pos) + input.mid(pos + prefix.size(), i - pos - prefix.size()) + input.mid(i + 1);
        }
    }
    return input;
}

QString KnowledgeDB::unqualify(const QString &name)
{
    return unqualify(name, Scope{}); // 默认域 D₀={world,has}，与旧行为逐字节相同
}

// 按域投影（出口即显示名）：① 先去一层 scope 外壳 scope.subj→scope.pred:[…]（仅当它是名字前缀时；不递归）；
// ② 再剥掉全部 world→has:[…]（expandRecursively）；③ 把 →has: 缩为 → 并解码。
QString KnowledgeDB::unqualify(const QString &name, const Scope &scope)
{
    const QString scopeShell = scope.subj + QStringLiteral("→%1:[").arg(scope.pred);
    const QString deScoped = spliceShell(name, name.startsWith(scopeShell) ? 0 : -1, scopeShell);
    // 逐层剥壳再缩写：链名末段宾词可能本身是限定名（多层 world→has:[…]），只剥一层会残留内层壳显示成「丙→[world→[乙]]」
    return decodeName(abbreviateHas(expandRecursively(deScoped)));
}

// 剥离一层 world→has:[…] 外壳（内层仍为转义形式）；无外壳/括号不配平原样返回，扫描跳过被转义括号以免被叶子名提前截断
QString KnowledgeDB::expandByRegex(const QString &input)
{
    const QString prefix = QStringLiteral("world→has:["); // 静态函数不能调 scopePrefix()
    return spliceShell(input, input.indexOf(prefix), prefix);
}

QString KnowledgeDB::expandRecursively(const QString &input)
{
    QString result = input;
    QString prev;
    do {
        prev = result;
        result = expandByRegex(prev);
    } while (result != prev);
    return result;
}
