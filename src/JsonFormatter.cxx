// JSON formatter for QingMo.
#include "JsonFormatter.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr int maxNestingDepth = 512;

bool isJsonWhitespace(char c) noexcept {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool isValueDelimiter(char c) noexcept {
	return isJsonWhitespace(c) || c == ',' || c == ']' || c == '}';
}

void appendUtf8(std::string &out, unsigned int codePoint) {
	if (codePoint <= 0x7F) {
		out.push_back(static_cast<char>(codePoint));
	} else if (codePoint <= 0x7FF) {
		out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
		out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
	} else if (codePoint <= 0xFFFF) {
		out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
		out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
	} else {
		out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
		out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
	}
}

// Decodes a document that consists of a single JSON string. Returns false when
// the document is not a string, is malformed, or has trailing content.
bool decodeRootJsonString(std::string_view src, std::string &out) {
	std::size_t pos = 0;
	if (src.starts_with("\xEF\xBB\xBF")) {
		pos = 3;
	}
	while (pos < src.size() && isJsonWhitespace(src[pos])) {
		++pos;
	}
	if (pos >= src.size() || src[pos] != '"') {
		return false;
	}
	++pos;
	out.clear();

	const auto hex4 = [&src](std::size_t at) -> int {
		int value = 0;
		for (int i = 0; i < 4; ++i) {
			const char c = src[at + i];
			value <<= 4;
			if (c >= '0' && c <= '9') {
				value |= c - '0';
			} else if (c >= 'a' && c <= 'f') {
				value |= c - 'a' + 10;
			} else if (c >= 'A' && c <= 'F') {
				value |= c - 'A' + 10;
			} else {
				return -1;
			}
		}
		return value;
	};

	while (pos < src.size()) {
		const unsigned char ch = static_cast<unsigned char>(src[pos]);
		if (ch == '"') {
			++pos;
			while (pos < src.size() && isJsonWhitespace(src[pos])) {
				++pos;
			}
			return pos == src.size();
		}
		if (ch < 0x20) {
			return false;
		}
		if (ch != '\\') {
			out.push_back(static_cast<char>(ch));
			++pos;
			continue;
		}

		++pos;
		if (pos >= src.size()) {
			return false;
		}
		switch (src[pos]) {
		case '"':
			out.push_back('"');
			++pos;
			break;
		case '\\':
			out.push_back('\\');
			++pos;
			break;
		case '/':
			out.push_back('/');
			++pos;
			break;
		case 'b':
			out.push_back('\b');
			++pos;
			break;
		case 'f':
			out.push_back('\f');
			++pos;
			break;
		case 'n':
			out.push_back('\n');
			++pos;
			break;
		case 'r':
			out.push_back('\r');
			++pos;
			break;
		case 't':
			out.push_back('\t');
			++pos;
			break;
		case 'u': {
			if (pos + 5 > src.size()) {
				return false;
			}
			int codePoint = hex4(pos + 1);
			if (codePoint < 0) {
				return false;
			}
			pos += 5;
			if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
				if (pos + 6 <= src.size() && src[pos] == '\\' && src[pos + 1] == 'u') {
					const int low = hex4(pos + 2);
					if (low < 0xDC00 || low > 0xDFFF) {
						return false;
					}
					pos += 6;
					codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
				} else {
					return false;
				}
			} else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
				return false;
			}
			appendUtf8(out, static_cast<unsigned int>(codePoint));
			break;
		}
		default:
			return false;
		}
	}
	return false;
}

class Parser {
public:
	Parser(std::string_view source, const JsonFormatter::Options &options) noexcept
		: src(source), opt(options) {
	}

	JsonFormatter::Result Parse() {
		if (src.starts_with("\xEF\xBB\xBF")) {
			output.append("\xEF\xBB\xBF");
			pos = 3;
		}
		hasTrailingNewline = detectTrailingNewline(src);

		skipWhitespace();
		if (pos >= src.size()) {
			fail("JSON 内容为空", pos);
			return makeError();
		}
		if (!parseValue(0)) {
			return makeError();
		}
		skipWhitespace();
		if (pos != src.size()) {
			fail("JSON 结束后存在多余内容", pos);
			return makeError();
		}
		if (opt.keepTrailingNewline && hasTrailingNewline) {
			output.append(opt.eol);
		}

		JsonFormatter::Result result;
		result.ok = true;
		result.text = std::move(output);
		return result;
	}

private:
	std::string_view src;
	const JsonFormatter::Options &opt;
	std::size_t pos = 0;
	std::string output;
	std::string error;
	std::size_t errorPos = 0;
	bool failed = false;
	bool hasTrailingNewline = false;

	static bool detectTrailingNewline(std::string_view text) noexcept {
		std::size_t end = text.size();
		while (end > 0 && (text[end - 1] == ' ' || text[end - 1] == '\t')) {
			--end;
		}
		return end > 0 && (text[end - 1] == '\n' || text[end - 1] == '\r');
	}

	bool fail(std::string message, std::size_t at) {
		if (!failed) {
			failed = true;
			error = std::move(message);
			errorPos = std::min(at, src.size());
		}
		return false;
	}

	JsonFormatter::Result makeError() const {
		JsonFormatter::Result result;
		result.ok = false;
		result.error = error;
		result.position = errorPos;
		result.line = 1;
		result.column = 1;
		for (std::size_t i = 0; i < errorPos && i < src.size(); ++i) {
			const unsigned char ch = static_cast<unsigned char>(src[i]);
			if (ch == '\n') {
				++result.line;
				result.column = 1;
			} else if ((ch & 0xC0) != 0x80) {
				++result.column;
			}
		}
		return result;
	}

	void skipWhitespace() noexcept {
		while (pos < src.size() && isJsonWhitespace(src[pos])) {
			++pos;
		}
	}

	void indent(int depth) {
		if (opt.indent > 0 && depth > 0) {
			output.append(static_cast<std::size_t>(depth) * static_cast<std::size_t>(opt.indent), ' ');
		}
	}

	bool parseValue(int depth) {
		if (depth > maxNestingDepth) {
			return fail("JSON 嵌套层级过深", pos);
		}
		skipWhitespace();
		if (pos >= src.size()) {
			return fail("缺少 JSON 值", pos);
		}
		switch (src[pos]) {
		case '{':
			return parseObject(depth);
		case '[':
			return parseArray(depth);
		case '"':
			return parseString();
		case 't':
			return parseLiteral("true");
		case 'f':
			return parseLiteral("false");
		case 'n':
			return parseLiteral("null");
		default:
			break;
		}
		if (src[pos] == '-' || (src[pos] >= '0' && src[pos] <= '9')) {
			return parseNumber();
		}
		return fail("无法识别的 JSON 值", pos);
	}

	bool parseObject(int depth) {
		++pos; // '{'
		output.push_back('{');
		skipWhitespace();
		if (pos < src.size() && src[pos] == '}') {
			++pos;
			output.push_back('}');
			return true;
		}
		if (pos >= src.size()) {
			return fail("对象缺少结束的 '}'", pos);
		}

		output.append(opt.eol);
		while (true) {
			skipWhitespace();
			if (pos >= src.size()) {
				return fail("对象缺少结束的 '}'", pos);
			}
			if (src[pos] != '"') {
				return fail("对象的键必须是字符串", pos);
			}
			indent(depth + 1);
			if (!parseString()) {
				return false;
			}
			skipWhitespace();
			if (pos >= src.size() || src[pos] != ':') {
				return fail("键后缺少冒号", pos);
			}
			++pos;
			output.append(": ");
			if (!parseValue(depth + 1)) {
				return false;
			}
			skipWhitespace();
			if (pos >= src.size()) {
				return fail("对象缺少结束的 '}'", pos);
			}
			if (src[pos] == ',') {
				++pos;
				output.push_back(',');
				output.append(opt.eol);
				skipWhitespace();
				if (pos < src.size() && src[pos] == '}') {
					return fail("对象末尾存在多余逗号", pos);
				}
				continue;
			}
			if (src[pos] == '}') {
				++pos;
				output.append(opt.eol);
				indent(depth);
				output.push_back('}');
				return true;
			}
			return fail("对象成员之间缺少逗号", pos);
		}
	}

	bool parseArray(int depth) {
		++pos; // '['
		output.push_back('[');
		skipWhitespace();
		if (pos < src.size() && src[pos] == ']') {
			++pos;
			output.push_back(']');
			return true;
		}
		if (pos >= src.size()) {
			return fail("数组缺少结束的 ']'", pos);
		}

		output.append(opt.eol);
		while (true) {
			skipWhitespace();
			if (pos >= src.size()) {
				return fail("数组缺少结束的 ']'", pos);
			}
			indent(depth + 1);
			if (!parseValue(depth + 1)) {
				return false;
			}
			skipWhitespace();
			if (pos >= src.size()) {
				return fail("数组缺少结束的 ']'", pos);
			}
			if (src[pos] == ',') {
				++pos;
				output.push_back(',');
				output.append(opt.eol);
				skipWhitespace();
				if (pos < src.size() && src[pos] == ']') {
					return fail("数组末尾存在多余逗号", pos);
				}
				continue;
			}
			if (src[pos] == ']') {
				++pos;
				output.append(opt.eol);
				indent(depth);
				output.push_back(']');
				return true;
			}
			return fail("数组成员之间缺少逗号", pos);
		}
	}

	bool parseString() {
		const std::size_t begin = pos;
		++pos; // '"'
		while (pos < src.size()) {
			const unsigned char ch = static_cast<unsigned char>(src[pos]);
			if (ch == '"') {
				++pos;
				output.append(src.substr(begin, pos - begin));
				return true;
			}
			if (ch < 0x20) {
				return fail("字符串中包含未转义的控制字符", pos);
			}
			if (ch == '\\') {
				++pos;
				if (pos >= src.size()) {
					return fail("字符串转义不完整", pos);
				}
				const char escape = src[pos];
				if (escape == 'u') {
					if (pos + 4 >= src.size()) {
						return fail("\\u 转义需要 4 位十六进制数", pos);
					}
					for (int i = 1; i <= 4; ++i) {
						if (!std::isxdigit(static_cast<unsigned char>(src[pos + i]))) {
							return fail("\\u 转义需要 4 位十六进制数", pos + i);
						}
					}
					pos += 5;
				} else if (escape == '"' || escape == '\\' || escape == '/' ||
					   escape == 'b' || escape == 'f' || escape == 'n' ||
					   escape == 'r' || escape == 't') {
					++pos;
				} else {
					return fail("无效的字符串转义", pos);
				}
			} else {
				++pos;
			}
		}
		return fail("字符串缺少结束引号", begin);
	}

	bool parseNumber() {
		const std::size_t begin = pos;
		if (src[pos] == '-') {
			++pos;
			if (pos >= src.size()) {
				return fail("无效的数字", pos);
			}
		}
		if (src[pos] == '0') {
			++pos;
		} else if (src[pos] >= '1' && src[pos] <= '9') {
			while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') {
				++pos;
			}
		} else {
			return fail("无效的数字", pos);
		}

		if (pos < src.size() && src[pos] == '.') {
			++pos;
			if (pos >= src.size() || src[pos] < '0' || src[pos] > '9') {
				return fail("小数点后必须有数字", pos);
			}
			while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') {
				++pos;
			}
		}

		if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
			++pos;
			if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) {
				++pos;
			}
			if (pos >= src.size() || src[pos] < '0' || src[pos] > '9') {
				return fail("指数部分必须有数字", pos);
			}
			while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') {
				++pos;
			}
		}

		if (pos < src.size() && !isValueDelimiter(src[pos])) {
			return fail("数字后存在无效字符", pos);
		}
		output.append(src.substr(begin, pos - begin));
		return true;
	}

	bool parseLiteral(std::string_view literal) {
		if (src.substr(pos, literal.size()) != literal) {
			return fail("无效的 JSON 字面量", pos);
		}
		pos += literal.size();
		if (pos < src.size() && !isValueDelimiter(src[pos])) {
			return fail("字面量后存在无效字符", pos);
		}
		output.append(literal);
		return true;
	}
};

}

JsonFormatter::Result JsonFormatter::Format(std::string_view source, const Options &options) {
	Options safe = options;
	safe.indent = std::clamp(safe.indent, 0, 8);
	if (safe.eol.empty()) {
		safe.eol = "\n";
	}

	Parser parser(source, safe);
	Result outer = parser.Parse();
	if (!outer.ok || !safe.unwrapString) {
		return outer;
	}

	std::string inner;
	if (!decodeRootJsonString(source, inner)) {
		return outer;
	}

	// Only unwrap when the decoded text is valid JSON; otherwise keep the
	// original string value untouched.
	Options innerOptions = safe;
	innerOptions.unwrapString = false;
	Parser innerParser(inner, innerOptions);
	Result innerResult = innerParser.Parse();
	if (!innerResult.ok) {
		return outer;
	}
	innerResult.unwrappedString = true;
	return innerResult;
}
