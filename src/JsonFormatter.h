// SPDX-License-Identifier: MIT
// JSON formatter for QingMo.
// Parses strict JSON and re-serializes it with stable indentation.
#ifndef JSONFORMATTER_H
#define JSONFORMATTER_H

#include <cstddef>
#include <string>
#include <string_view>

namespace JsonFormatter {

struct Options {
	int indent = 2;                  // spaces per nesting level, 0..8
	std::string eol = "\n";          // output line ending
	bool keepTrailingNewline = true; // preserve a final newline from the input
	bool unwrapString = true;        // unwrap a JSON string that contains JSON
};

struct Result {
	bool ok = false;
	bool unwrappedString = false; // the outer JSON string was decoded
	std::string text;
	std::string error;
	std::size_t position = 0; // byte offset where parsing stopped
	int line = 1;
	int column = 1;
};

// Formats a complete JSON value. Strings, escapes and number lexemes are
// preserved as written; only insignificant whitespace is rewritten.
Result Format(std::string_view source, const Options &options);

}

#endif
