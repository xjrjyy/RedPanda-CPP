#ifndef SYNHIGHLIGTERBASE_H
#define SYNHIGHLIGTERBASE_H

#include <QColor>
#include <QObject>
#include <memory>
#include <QMap>
#include <QSet>
#include <QVector>
#include <QVector>
#include "../Types.h"

typedef struct {
    int state;
    int spaceState;
} SynRangeState;

typedef int SynTokenKind;

enum class SynHighlighterTokenType {
    Default, Space, Comment,
    PreprocessDirective, String, StringEscapeSequence,
    Identifier, Symbol,
    Character, Keyword, Number};

enum class SynHighlighterClass {
    Composition,
    CppHighlighter,
};

class SynHighlighterAttribute : public QObject{
    Q_OBJECT
public:
    explicit SynHighlighterAttribute(const QString& name, QObject* parent = nullptr);
    QColor background() const;
    void setBackground(const QColor &background);

    QColor foreground() const;
    void setForeground(const QColor &foreground);

    QString name() const;
    void setName(const QString &name);

    SynFontStyles styles() const;
    void setStyles(const SynFontStyles &styles);

signals:
    void changed();
private:
    void setChanged();
private:
    QColor mBackground;
    QColor mForeground;
    QString mName;
    SynFontStyles mStyles;
};

typedef std::shared_ptr<SynHighlighterAttribute> PSynHighlighterAttribute;
using SynHighlighterAttributeList = QVector<PSynHighlighterAttribute>;

class SynHighlighter : public QObject
{
    Q_OBJECT
public:
    explicit SynHighlighter(QObject *parent = nullptr);

    const QMap<QString, PSynHighlighterAttribute>& attributes() const;

    const QSet<QChar>& wordBreakChars() const;



    PSynHighlighterAttribute commentAttribute() const;

    PSynHighlighterAttribute identifierAttribute() const;

    PSynHighlighterAttribute keywordAttribute() const;

    PSynHighlighterAttribute stringAttribute() const;

    PSynHighlighterAttribute whitespaceAttribute() const;

    PSynHighlighterAttribute symbolAttribute() const;

    virtual bool isIdentChar(const QChar& ch) const;

    virtual SynHighlighterClass getClass() const = 0;
    virtual QString getName() const = 0;

    void beginUpdate();
    void endUpdate();
    virtual bool getTokenFinished() const = 0;
    virtual bool isLastLineCommentNotFinished(int state) const = 0;
    virtual bool isLastLineStringNotFinished(int state) const = 0;
    virtual bool eol() const = 0;
    virtual SynRangeState getRangeState() const;
    virtual int getBraceLevel() const;
    virtual int getBracketLevel() const;
    virtual int getParenthesisLevel() const;
    virtual QString getToken() const=0;
    virtual PSynHighlighterAttribute getTokenAttribute() const=0;
    virtual SynHighlighterTokenType getTokenType();
    virtual SynTokenKind getTokenKind() = 0;
    virtual int getTokenPos() = 0;
    virtual bool isKeyword(const QString& word);
    virtual void next() = 0;
    virtual void nextToEol();
    virtual void setState(SynRangeState rangeState, int braceLevel, int bracketLevel, int parenthesisLevel) = 0;
    virtual void setLine(const QString& newLine, int lineNumber) = 0;
    virtual void resetState() = 0;

    virtual QString languageName();

    static bool isSpaceChar(const QChar& ch);
    bool enabled() const;
    void setEnabled(bool value);

signals:
    void attributesChanged();

protected:
    void onAttributeChanged();
    void setAttributesChanged();

protected:
    PSynHighlighterAttribute mCommentAttribute;
    PSynHighlighterAttribute mIdentifierAttribute;
    PSynHighlighterAttribute mKeywordAttribute;
    PSynHighlighterAttribute mStringAttribute;
    PSynHighlighterAttribute mWhitespaceAttribute;
    PSynHighlighterAttribute mSymbolAttribute;

    void addAttribute(PSynHighlighterAttribute attribute);
    void clearAttributes();
    virtual int attributesCount() const;

    virtual PSynHighlighterAttribute getAttribute(const QString& name) const;

private:
    QMap<QString,PSynHighlighterAttribute> mAttributes;
    int mUpdateCount;
    bool mEnabled;
    QSet<QChar> mWordBreakChars;
};

using PSynHighlighter = std::shared_ptr<SynHighlighter>;
using SynHighlighterList = QVector<PSynHighlighter>;

#endif // SYNHIGHLIGTERBASE_H