#include "json_lite.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <locale.h>
#include <utility>
#include <vector>

namespace
{
_locale_t JsonNumericLocale()
{
    static _locale_t locale = _create_locale(LC_NUMERIC, "C");
    return locale;
}

struct JsonValue
{
    enum Type { Null, Bool, Number, String, Array, Object } type;
    bool boolean;
    double number;
    std::string text;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string, JsonValue>> object;

    JsonValue() : type(Null), boolean(false), number(0.0) {}
};

class JsonParser
{
public:
    JsonParser(const char* text, size_t len, std::string* error)
        : m_start(text), m_ptr(text), m_end(text + len), m_error(error), m_depth(0)
    {
        if (len >= 3 && (unsigned char)m_ptr[0] == 0xEF &&
            (unsigned char)m_ptr[1] == 0xBB && (unsigned char)m_ptr[2] == 0xBF) {
            m_ptr += 3;
            m_start = m_ptr;
        }
    }

    bool ParseDocument()
    {
        JsonValue root;
        SkipSpace();
        if (m_ptr == m_end) return Fail("空文档");
        if (!ParseValue(root)) return false;
        SkipSpace();
        if (m_ptr != m_end) return Fail("根值后存在多余内容");
        return true;
    }

private:
    const char* m_start;
    const char* m_ptr;
    const char* m_end;
    std::string* m_error;
    int m_depth;

    bool Fail(const char* message)
    {
        return FailAt(m_ptr, message);
    }

    bool FailAt(const char* at, const char* message)
    {
        if (m_error) {
            char buf[160] = {0};
            snprintf(buf, sizeof(buf) - 1, "偏移 %ld: %s", (long)(at - m_start), message);
            *m_error = buf;
        }
        return false;
    }

    void SkipSpace()
    {
        while (m_ptr < m_end) {
            char c = *m_ptr;
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') m_ptr++;
            else break;
        }
    }

    bool ParseValue(JsonValue& out)
    {
        SkipSpace();
        if (m_ptr >= m_end) return Fail("值缺失");
        char c = *m_ptr;
        if (c == '{') return ParseObject(out);
        if (c == '[') return ParseArray(out);
        if (c == '"') {
            out.type = JsonValue::String;
            return ParseString(out.text);
        }
        if (c == 't') {
            if (!Consume("true")) return false;
            out.type = JsonValue::Bool;
            out.boolean = true;
            return true;
        }
        if (c == 'f') {
            if (!Consume("false")) return false;
            out.type = JsonValue::Bool;
            out.boolean = false;
            return true;
        }
        if (c == 'n') {
            if (!Consume("null")) return false;
            out.type = JsonValue::Null;
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber(out);
        return Fail("未知值类型");
    }

    bool Consume(const char* literal)
    {
        size_t len = strlen(literal);
        if ((size_t)(m_end - m_ptr) < len || memcmp(m_ptr, literal, len) != 0) {
            return Fail("字面量不匹配");
        }
        m_ptr += len;
        return true;
    }

    bool ParseObject(JsonValue& out)
    {
        if (++m_depth > 256) return Fail("嵌套过深");
        out.type = JsonValue::Object;
        m_ptr++;
        SkipSpace();
        if (m_ptr < m_end && *m_ptr == '}') {
            m_ptr++;
            m_depth--;
            return true;
        }
        for (;;) {
            SkipSpace();
            if (m_ptr >= m_end || *m_ptr != '"') return Fail("对象键必须是字符串");
            std::string key;
            if (!ParseString(key)) return false;
            SkipSpace();
            if (m_ptr >= m_end || *m_ptr != ':') return Fail("键后缺少冒号");
            m_ptr++;
            JsonValue value;
            if (!ParseValue(value)) return false;
            out.object.emplace_back(std::move(key), std::move(value));
            SkipSpace();
            if (m_ptr >= m_end) return Fail("对象未闭合");
            if (*m_ptr == '}') {
                m_ptr++;
                m_depth--;
                return true;
            }
            if (*m_ptr != ',') return Fail("对象成员之间缺少逗号");
            m_ptr++;
        }
    }

    bool ParseArray(JsonValue& out)
    {
        if (++m_depth > 256) return Fail("嵌套过深");
        out.type = JsonValue::Array;
        m_ptr++;
        SkipSpace();
        if (m_ptr < m_end && *m_ptr == ']') {
            m_ptr++;
            m_depth--;
            return true;
        }
        for (;;) {
            JsonValue value;
            if (!ParseValue(value)) return false;
            out.array.emplace_back(std::move(value));
            SkipSpace();
            if (m_ptr >= m_end) return Fail("数组未闭合");
            if (*m_ptr == ']') {
                m_ptr++;
                m_depth--;
                return true;
            }
            if (*m_ptr != ',') return Fail("数组元素之间缺少逗号");
            m_ptr++;
        }
    }

    static void AppendUtf8(std::string& out, unsigned int cp)
    {
        if (cp <= 0x7F) {
            out.push_back((char)cp);
        } else if (cp <= 0x7FF) {
            out.push_back((char)(0xC0 | (cp >> 6)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back((char)(0xE0 | (cp >> 12)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else {
            out.push_back((char)(0xF0 | (cp >> 18)));
            out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }

    bool ParseHex4(unsigned int& value)
    {
        if ((size_t)(m_end - m_ptr) < 4) return Fail("\\u 转义不完整");
        value = 0;
        for (int i = 0; i < 4; i++) {
            char c = *m_ptr++;
            value <<= 4;
            if (c >= '0' && c <= '9') value |= (unsigned int)(c - '0');
            else if (c >= 'a' && c <= 'f') value |= (unsigned int)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= (unsigned int)(c - 'A' + 10);
            else return Fail("\\u 转义含非十六进制字符");
        }
        return true;
    }

    bool ParseString(std::string& out)
    {
        m_ptr++;
        while (m_ptr < m_end) {
            const char* at = m_ptr;
            unsigned char c = (unsigned char)*m_ptr++;
            if (c == '"') return true;
            if (c < 0x20) return FailAt(at, "字符串包含未转义控制字符");
            if (c != '\\') {
                out.push_back((char)c);
                continue;
            }
            if (m_ptr >= m_end) return Fail("转义不完整");
            char esc = *m_ptr++;
            switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned int cp = 0;
                    if (!ParseHex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF && (size_t)(m_end - m_ptr) >= 6 &&
                        m_ptr[0] == '\\' && m_ptr[1] == 'u') {
                        const char* save = m_ptr;
                        m_ptr += 2;
                        unsigned int low = 0;
                        if (ParseHex4(low) && low >= 0xDC00 && low <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        } else {
                            m_ptr = save;
                        }
                    }
                    AppendUtf8(out, cp);
                    break;
                }
                default: return Fail("未知转义序列");
            }
        }
        return Fail("字符串未闭合");
    }

    bool ParseNumber(JsonValue& out)
    {
        const char* start = m_ptr;
        if (m_ptr < m_end && *m_ptr == '-') m_ptr++;
        if (m_ptr >= m_end) return Fail("数字不完整");
        if (*m_ptr == '0') {
            m_ptr++;
        } else if (*m_ptr >= '1' && *m_ptr <= '9') {
            while (m_ptr < m_end && *m_ptr >= '0' && *m_ptr <= '9') m_ptr++;
        } else {
            return Fail("数字整数部分不合法");
        }
        if (m_ptr < m_end && *m_ptr == '.') {
            m_ptr++;
            if (m_ptr >= m_end || *m_ptr < '0' || *m_ptr > '9') return Fail("小数部分不合法");
            while (m_ptr < m_end && *m_ptr >= '0' && *m_ptr <= '9') m_ptr++;
        }
        if (m_ptr < m_end && (*m_ptr == 'e' || *m_ptr == 'E')) {
            m_ptr++;
            if (m_ptr < m_end && (*m_ptr == '+' || *m_ptr == '-')) m_ptr++;
            if (m_ptr >= m_end || *m_ptr < '0' || *m_ptr > '9') return Fail("指数部分不合法");
            while (m_ptr < m_end && *m_ptr >= '0' && *m_ptr <= '9') m_ptr++;
        }
        size_t tokenLength = (size_t)(m_ptr - start);
        if (tokenLength >= 64) return Fail("数字过长");
        char token[64] = {0};
        memcpy(token, start, tokenLength);
        token[tokenLength] = '\0';
        char* numberEnd = NULL;
        _locale_t numericLocale = JsonNumericLocale();
        double value = numericLocale ? _strtod_l(token, &numberEnd, numericLocale)
                                     : strtod(token, &numberEnd);
        if (!numberEnd || numberEnd != token + tokenLength || !std::isfinite(value)) {
            return Fail("数字超出范围");
        }
        out.type = JsonValue::Number;
        out.number = value;
        return true;
    }
};
}

bool Json_Validate(const char* text, size_t len, std::string* error)
{
    if (!text) {
        if (error) *error = "文本为空";
        return false;
    }
    JsonParser parser(text, len, error);
    return parser.ParseDocument();
}
