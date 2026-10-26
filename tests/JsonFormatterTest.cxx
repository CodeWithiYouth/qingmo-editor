#include "JsonFormatter.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <utility>

namespace {

int failures = 0;

void expect(bool condition, const std::string &name) {
	if (!condition) {
		std::cerr << "FAIL: " << name << '\n';
		++failures;
	}
}

void expectEqual(const std::string &actual, const std::string &expected, const std::string &name) {
	if (actual != expected) {
		std::cerr << "FAIL: " << name << "\n  expected: " << std::quoted(expected)
			  << "\n  actual:   " << std::quoted(actual) << '\n';
		++failures;
	}
}

JsonFormatter::Options options(int indent = 2, std::string eol = "\n", bool keepTrailingNewline = false) {
	JsonFormatter::Options opt;
	opt.indent = indent;
	opt.eol = std::move(eol);
	opt.keepTrailingNewline = keepTrailingNewline;
	return opt;
}

void testPrettyPrint() {
	const auto result = JsonFormatter::Format(
		"{\"a\":1,\"b\":[true,null,\"x\\n\"]}", options());
	expect(result.ok, "pretty: parses");
	expectEqual(result.text,
		"{\n"
		"  \"a\": 1,\n"
		"  \"b\": [\n"
		"    true,\n"
		"    null,\n"
		"    \"x\\n\"\n"
		"  ]\n"
		"}",
		"pretty: output");

	const auto empty = JsonFormatter::Format("{\"a\":{},\"b\":[]}", options());
	expect(empty.ok, "empty containers: parses");
	expectEqual(empty.text,
		"{\n"
		"  \"a\": {},\n"
		"  \"b\": []\n"
		"}",
		"empty containers: output");
}

void testIndentAndNumbers() {
	const auto four = JsonFormatter::Format("{\"a\":[1]}", options(4));
	expect(four.ok, "indent 4: parses");
	expectEqual(four.text,
		"{\n"
		"    \"a\": [\n"
		"        1\n"
		"    ]\n"
		"}",
		"indent 4: output");

	const auto numbers = JsonFormatter::Format("[0,-1,1.5,2e10,-3.25E-2]", options());
	expect(numbers.ok, "numbers: parses");
	expectEqual(numbers.text,
		"[\n"
		"  0,\n"
		"  -1,\n"
		"  1.5,\n"
		"  2e10,\n"
		"  -3.25E-2\n"
		"]",
		"numbers: preserves lexemes");
}

void testStringsAndUnicode() {
	const auto unicode = JsonFormatter::Format("{\"中文\":\"值\"}", options());
	expect(unicode.ok, "unicode: parses");
	expectEqual(unicode.text,
		"{\n"
		"  \"中文\": \"值\"\n"
		"}",
		"unicode: output");

	const auto escapes = JsonFormatter::Format(
		"{\"a\":\"\\\\\\\"\\/\\b\\f\\n\\r\\t\\u4f60\"}", options());
	expect(escapes.ok, "escapes: parses");
	expect(escapes.text.find("\"\\\\\\\"\\/\\b\\f\\n\\r\\t\\u4f60\"") != std::string::npos,
		"escapes: preserved");
}

void testDoubleEncoded() {
	const auto result = JsonFormatter::Format(
		"\"{\\\"a\\\":1,\\\"b\\\":[true],\\\"name\\\":\\\"\\u8f7b\\u58a8\\\"}\"", options());
	expect(result.ok, "double encoded: parses");
	expect(result.unwrappedString, "double encoded: unwrapped");
	expectEqual(result.text,
		"{\n"
		"  \"a\": 1,\n"
		"  \"b\": [\n"
		"    true\n"
		"  ],\n"
		"  \"name\": \"轻墨\"\n"
		"}",
		"double encoded: output");

	auto noUnwrap = options();
	noUnwrap.unwrapString = false;
	const auto raw = JsonFormatter::Format("\"{\\\"a\\\":1}\"", noUnwrap);
	expect(raw.ok, "double encoded disabled: parses");
	expect(!raw.unwrappedString, "double encoded disabled: not unwrapped");
	expectEqual(raw.text, "\"{\\\"a\\\":1}\"", "double encoded disabled: output");
}

void testLineEndingsAndBom() {
	const auto trailing = JsonFormatter::Format("{\"a\":1}\n", options(2, "\n", true));
	expect(trailing.ok, "trailing newline: parses");
	expectEqual(trailing.text,
		"{\n"
		"  \"a\": 1\n"
		"}\n",
		"trailing newline: kept");

	const auto crlf = JsonFormatter::Format("{\"a\":1}", options(2, "\r\n"));
	expect(crlf.ok, "crlf: parses");
	expectEqual(crlf.text,
		"{\r\n"
		"  \"a\": 1\r\n"
		"}",
		"crlf: output");

	const auto bom = JsonFormatter::Format("\xEF\xBB\xBF{\"a\":1}", options());
	expect(bom.ok, "bom: parses");
	expect(bom.text.starts_with("\xEF\xBB\xBF"), "bom: preserved");
}

void testErrors() {
	struct ErrorCase {
		const char *name;
		const char *input;
		const char *messagePart;
		int line;
	};
	const ErrorCase cases[] = {
		{"trailing object comma", "{\n  \"a\": 1,\n}", "多余逗号", 3},
		{"trailing array comma", "[1,]", "多余逗号", 1},
		{"missing colon", "{\"a\" 1}", "冒号", 1},
		{"leading zero", "{\"a\":01}", "数字", 1},
		{"unterminated string", "{\"a\":\"x}", "结束引号", 1},
		{"extra content", "{\"a\":1} x", "多余内容", 1},
		{"empty input", "", "内容为空", 1},
		{"missing brace", "{\"a\":1", "缺少结束", 1},
		{"missing array comma", "[1 2]", "缺少逗号", 1},
		{"bad literal", "{\"a\":tru}", "字面量", 1},
		{"bad unicode escape", "{\"a\":\"\\u12\"}", "十六进制", 1},
	};
	for (const ErrorCase &test : cases) {
		const auto result = JsonFormatter::Format(test.input, options());
		expect(!result.ok, std::string("error: ") + test.name + " is rejected");
		if (result.ok) {
			continue;
		}
		expect(result.error.find(test.messagePart) != std::string::npos,
			std::string("error: ") + test.name + " message");
		expect(result.line == test.line, std::string("error: ") + test.name + " line");
	}

	const auto duplicateKeys = JsonFormatter::Format("{\"a\":1,\"a\":2}", options());
	expect(duplicateKeys.ok, "duplicate keys: allowed");
}

}

int main() {
	testPrettyPrint();
	testIndentAndNumbers();
	testStringsAndUnicode();
	testDoubleEncoded();
	testLineEndingsAndBom();
	testErrors();

	if (failures != 0) {
		std::cerr << failures << " test(s) failed\n";
		return 1;
	}
	std::cout << "All JsonFormatter tests passed\n";
	return 0;
}
