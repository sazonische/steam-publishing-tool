#include "utils/keyvalues.h"

#include <sstream>

namespace {

	bool EqualsIgnoreCase(std::string_view left, std::string_view right) {
		return std::ranges::equal(left, right, [](unsigned char a, unsigned char b) {
			return std::tolower(a) == std::tolower(b);
		});
	}

	class CKeyValuesParser {
	public:
		explicit CKeyValuesParser(std::string_view text) :
			_text(text) {
			// UTF-8 BOM: Steam sometimes writes it into acf files.
			if (_text.starts_with("\xEF\xBB\xBF")) {
				_position = 3;
			}
		}

		std::optional<KeyValuesNode> ParseRoot(std::string& errorMessage) {
			KeyValuesNode root;
			root.isBlock = true;
			if (!ParseBlockBody(root, true, errorMessage)) {
				return std::nullopt;
			}
			return root;
		}

	private:
		enum TokenKind : uint8_t {
			TOKEN_END = 0,
			TOKEN_STRING,
			TOKEN_OPEN_BRACE,
			TOKEN_CLOSE_BRACE,
		};

		struct Token {
			TokenKind kind{TOKEN_END};
			std::string text;
		};

		void SkipWhitespaceAndComments() {
			while (_position < _text.size()) {
				const char current = _text[_position];
				if (current == '\n') {
					++_line;
					++_position;
					continue;
				}
				if (std::isspace(static_cast<unsigned char>(current))) {
					++_position;
					continue;
				}
				if (current == '/' && _position + 1 < _text.size() && _text[_position + 1] == '/') {
					while (_position < _text.size() && _text[_position] != '\n') {
						++_position;
					}
					continue;
				}
				break;
			}
		}

		Token NextToken(std::string& errorMessage) {
			SkipWhitespaceAndComments();
			if (_position >= _text.size()) {
				return {TOKEN_END, {}};
			}

			const char current = _text[_position];
			if (current == '{') {
				++_position;
				return {TOKEN_OPEN_BRACE, {}};
			}
			if (current == '}') {
				++_position;
				return {TOKEN_CLOSE_BRACE, {}};
			}

			Token token{TOKEN_STRING, {}};
			if (current == '"') {
				++_position;
				while (_position < _text.size() && _text[_position] != '"') {
					char character = _text[_position];
					if (character == '\\' && _position + 1 < _text.size()) {
						++_position;
						switch (_text[_position]) {
							case 'n': character = '\n'; break;
							case 't': character = '\t'; break;
							case '"': character = '"'; break;
							case '\\': character = '\\'; break;
							default: character = _text[_position]; break;
						}
					} else if (character == '\n') {
						++_line;
					}
					token.text.push_back(character);
					++_position;
				}
				if (_position >= _text.size()) {
					errorMessage = std::format("unterminated string at line {}", _line);
					return {TOKEN_END, {}};
				}
				++_position;
				return token;
			}

			while (_position < _text.size()) {
				const char character = _text[_position];
				if (std::isspace(static_cast<unsigned char>(character)) || character == '{' || character == '}' || character == '"') {
					break;
				}
				token.text.push_back(character);
				++_position;
			}
			return token;
		}

		bool ParseBlockBody(KeyValuesNode& block, bool isRoot, std::string& errorMessage) {
			while (true) {
				Token keyToken = NextToken(errorMessage);
				if (!errorMessage.empty()) {
					return false;
				}
				if (keyToken.kind == TOKEN_END) {
					if (!isRoot) {
						errorMessage = std::format("unexpected end of file inside block '{}'", block.key);
						return false;
					}
					return true;
				}
				if (keyToken.kind == TOKEN_CLOSE_BRACE) {
					if (isRoot) {
						errorMessage = std::format("unexpected '}}' at line {}", _line);
						return false;
					}
					return true;
				}
				if (keyToken.kind != TOKEN_STRING) {
					errorMessage = std::format("expected key at line {}", _line);
					return false;
				}

				// #base / #include pull in other files — not needed for our purposes.
				if (keyToken.text.starts_with('#')) {
					Token pathToken = NextToken(errorMessage);
					if (pathToken.kind != TOKEN_STRING) {
						errorMessage = std::format("expected path after {} at line {}", keyToken.text, _line);
						return false;
					}
					continue;
				}

				Token valueToken = NextToken(errorMessage);
				if (!errorMessage.empty()) {
					return false;
				}

				KeyValuesNode child;
				child.key = std::move(keyToken.text);

				if (valueToken.kind == TOKEN_OPEN_BRACE) {
					child.isBlock = true;
					if (!ParseBlockBody(child, false, errorMessage)) {
						return false;
					}
				} else if (valueToken.kind == TOKEN_STRING) {
					child.value = std::move(valueToken.text);
					SkipConditional();
				} else {
					errorMessage = std::format("expected value or '{{' after key '{}' at line {}", child.key, _line);
					return false;
				}

				block.children.push_back(std::move(child));
			}
		}

		// A condition like [$WIN32] after the value: skipped entirely.
		void SkipConditional() {
			SkipWhitespaceAndComments();
			if (_position < _text.size() && _text[_position] == '[') {
				while (_position < _text.size() && _text[_position] != ']') {
					++_position;
				}
				if (_position < _text.size()) {
					++_position;
				}
			}
		}

		std::string_view _text;
		size_t _position{0};
		size_t _line{1};
	};

} // namespace

const KeyValuesNode* KeyValuesNode::Find(std::string_view childKey) const {
	for (const KeyValuesNode& child : children) {
		if (EqualsIgnoreCase(child.key, childKey)) {
			return &child;
		}
	}
	return nullptr;
}

std::string_view KeyValuesNode::GetString(std::string_view childKey, std::string_view fallback) const {
	const KeyValuesNode* child = Find(childKey);
	if (!child || child->isBlock) {
		return fallback;
	}
	return child->value;
}

namespace KeyValues {

	std::optional<KeyValuesNode> ParseText(std::string_view text, std::string& errorMessage) {
		errorMessage.clear();
		CKeyValuesParser parser(text);
		return parser.ParseRoot(errorMessage);
	}

	std::optional<KeyValuesNode> ParseFile(const std::filesystem::path& filePath, std::string& errorMessage) {
		std::ifstream stream(filePath, std::ios::binary);
		if (!stream) {
			errorMessage = std::format("cannot open {}", PathText::ToUtf8(filePath));
			return std::nullopt;
		}
		std::ostringstream buffer;
		buffer << stream.rdbuf();
		const std::string text = buffer.str();
		std::optional<KeyValuesNode> root = ParseText(text, errorMessage);
		if (!root.has_value()) {
			errorMessage = std::format("{}: {}", PathText::ToUtf8(filePath), errorMessage);
		}
		return root;
	}

} // namespace KeyValues
