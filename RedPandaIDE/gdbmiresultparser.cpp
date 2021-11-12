#include "gdbmiresultparser.h"

#include <QList>

static GDBMIResultParser::ParseValue EMPTY_PARSE_VALUE;

GDBMIResultParser::GDBMIResultParser()
{
    mResultTypes.insert("bkpt",GDBMIResultType::Breakpoint);
    mResultTypes.insert("BreakpointTable",GDBMIResultType::BreakpointTable);
    mResultTypes.insert("stack",GDBMIResultType::FrameStack);
    mResultTypes.insert("variables", GDBMIResultType::LocalVariables);
    mResultTypes.insert("frame",GDBMIResultType::Frame);
    mResultTypes.insert("asm_insns",GDBMIResultType::Disassembly);
    mResultTypes.insert("value",GDBMIResultType::Evaluation);
    mResultTypes.insert("register-names",GDBMIResultType::RegisterNames);
    mResultTypes.insert("register-values",GDBMIResultType::RegisterValues);
    mResultTypes.insert("memory",GDBMIResultType::Memory);
}

bool GDBMIResultParser::parse(const QByteArray &record, GDBMIResultType &type, ParseValue& value)
{
    const char* p = record.data();
    QByteArray name;
    bool result = parseNameAndValue(p,name,value);
    if (!result)
        return false;
//    if (*p!=0)
//        return false;
    if (!mResultTypes.contains(name))
        return false;
    type = mResultTypes[name];
    return true;
}

bool GDBMIResultParser::parseNameAndValue(char *&p, QByteArray &name, ParseValue &value)
{
    skipSpaces(p);
    char* nameStart =p;
    while (*p!=0 && isNameChar(*p)) {
        p++;
    }
    if (*p==0)
        return false;
    skipSpaces(p);
    if (*p!='=')
        return false;
    return parseValue(p,value);
}

bool GDBMIResultParser::parseValue(char *&p, ParseValue &value)
{
    skipSpaces(p);
    bool result;
    switch (*p) {
    case '{': {
        ParseObject obj;
        result = parseObject(p,obj);
        value = obj;
        break;
    }
    case '[': {
        QList<ParseObject> array;
        result = parseArray(p,array);
        value = array;
        break;
    }
    case '"': {
        QByteArray s;
        result = parseStringValue(p,s);
        value = s;
        break;
    }
    default:
        return false;
    }
    if (!result)
        return false;
    skipSpaces(p);
    if (*p!=0 && *p!=',')
        return false;
    return true;
}

bool GDBMIResultParser::parseStringValue(char *&p, QByteArray& stringValue)
{
    if (*p!='"')
        return false;
    p++;
    char* valueStart = p;
    while (*p!=0) {
        if (*p == '"') {
            break;
        } else if (*p=='\\' && *(p+1)!=0) {
            p+=2;
        } else {
            p++;
        }
    }
    if (*p=='"') {
        stringValue = QByteArray(valueStart,p-valueStart);
        p++; //skip '"'
        return true;
    }
    return false;
}

bool GDBMIResultParser::parseObject(char *&p, ParseObject &obj)
{
    if (*p!='{')
        return false;
    p++;

    if (*p!='}') {
        while (*p!=0) {
            QByteArray propName;
            ParseValue propValue;
            bool result = parseNameAndValue(p,propName,propValue);
            if (result) {
                obj[propName]=propValue;
            } else {
                return false;
            }
            skipSpaces(p);
            if (*p=='}')
                break;
            if (*p!=',')
                return false;
            p++; //skip ','
            skipSpaces(p);
        }
    }
    if (*p=='}') {
        p++; //skip '}'
        return true;
    }
    return false;
}

bool GDBMIResultParser::parseArray(char *&p, ParseValue &value)
{
    if (*p!='[')
        return false;
    p++;
    QList<ParseObject> array;
    if (*p!=']') {
        while (*p!=0) {
            skipSpaces(p);
            QObject obj;
            bool result = parseObject(p,obj);
            if (result) {
                array.append(obj);
            } else {
                return false;
            }
            skipSpaces(p);
            if (*p==']')
                break;
            if (*p!=',')
                return false;
            p++; //skip ','
            skipSpaces(p);
        }
    }
    if (*p==']') {
        value = array;
        p++; //skip ']'
        return true;
    }
    return false;
}

bool GDBMIResultParser::isNameChar(char ch)
{
    if (ch=='-')
        return true;
    if (ch>='a' && ch<='z')
        return true;
    if (ch>='A' && ch<='Z')
        return true;
    return false;
}

bool GDBMIResultParser::isSpaceChar(char ch)
{
    switch(ch) {
    case ' ':
    case '\t':
        return true;
    }
    return false;
}

void GDBMIResultParser::skipSpaces(char *&p)
{
    while (*p!=0 && isSpaceChar(*p))
        p++;
}

const QString &GDBMIResultParser::ParseValue::value() const
{
    return mValue;
}

const QList<GDBMIResultParser::ParseObject> &GDBMIResultParser::ParseValue::array() const
{
    return mArray;
}

const GDBMIResultParser::ParseObject &GDBMIResultParser::ParseValue::object() const
{
    return mObject;
}

GDBMIResultParser::ParseValueType GDBMIResultParser::ParseValue::type() const
{
    return mType;
}

GDBMIResultParser::ParseValue::ParseValue():
    mType(ParseValueType::NotAssigned) {

}

GDBMIResultParser::ParseValue::ParseValue(const QString &value):
    mValue(value),
    mType(ParseValueType::Value)
{
}

GDBMIResultParser::ParseValue::ParseValue(const PParseObject &object):
    mObject(object),
    mType(ParseValueType::Object)
{
}

GDBMIResultParser::ParseValue::ParseValue(const QList<PParseObject> &array):
    mArray(array),
    mType(ParseValueType::Array)
{
}

void GDBMIResultParser::ParseValue::addObject(const PParseObject &object)
{
    Q_ASSERT(mType == ParseValueType::Array || mType == ParseValueType::NotAssigned);
    mType = ParseValueType::Array;
    mArray.append(object);
}

GDBMIResultParser::ParseValue &GDBMIResultParser::ParseValue::operator=(const QString &value)
{
    Q_ASSERT(mType == ParseValueType::NotAssigned);
    mType = ParseValueType::Value;
    mValue = value;
}

GDBMIResultParser::ParseValue &GDBMIResultParser::ParseValue::operator=(const ParseObject& object)
{
    Q_ASSERT(mType == ParseValueType::NotAssigned);
    mType = ParseValueType::Object;
    mObject = object;
}

GDBMIResultParser::ParseValue &GDBMIResultParser::ParseValue::operator=(const QList<ParseObject>& array)
{
    Q_ASSERT(mType == ParseValueType::NotAssigned);
    mType = ParseValueType::Array;
    mArray = array;
}


const GDBMIResultParser::ParseValue &GDBMIResultParser::ParseObject::operator[](const QByteArray &name) const
{
    if (mProps.contains(name))
        return mProps[name];
    return EMPTY_PARSE_VALUE;
}

GDBMIResultParser::ParseObject &GDBMIResultParser::ParseObject::operator=(const ParseObject &object)
{
    mProps = object.mProps;
}

GDBMIResultParser::ParseValue &GDBMIResultParser::ParseObject::operator[](const QByteArray &name) {
    if (!mProps.contains(name))
        mProps[name]=ParseValue();
    return mProps[name];
}
