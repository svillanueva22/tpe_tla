#include "FlexActions.h"

/* MODULE INTERNAL STATE */

static bool _logIgnoredLexemes = true;
static LexicalAnalyzer * _lexicalAnalyzer = NULL;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownFlexActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: FlexActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_lexicalAnalyzer = NULL;
}

ModuleDestructor initializeFlexActionsModule(LexicalAnalyzer * lexicalAnalyzer) {
	_lexicalAnalyzer = lexicalAnalyzer;
	_logger = createLogger("FlexActions");
	_logIgnoredLexemes = getBooleanOrDefault("LOG_IGNORED_LEXEMES", _logIgnoredLexemes);
	return _shutdownFlexActionsModule;
}

/* PRIVATE FUNCTIONS */

static int _daysInMonth(const int year, const int month);
static CompilationStatus _lexicalError(Token * token, const char * reason);
static void _logTokenAction(const char * actionName, Token * token);
static CompilationStatus _pushAndDestroy(const char * actionName, Token * token);
static bool _toInteger(const char * lexeme, int * value);
static char * _unquote(const char * lexeme, const unsigned int length);

/**
 * The amount of days in a month of the proleptic Gregorian calendar (the one
 * used by ISO 8601).
 */
static int _daysInMonth(const int year, const int month) {
	static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if (month == 2) {
		const bool isLeapYear = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
		return isLeapYear ? 29 : 28;
	}
	return days[month - 1];
}

/**
 * Reports a lexical error, and pushes an UNKNOWN token to the parser. Since no
 * rule of the grammar accepts it, the parser fails and releases the partial
 * AST that was built so far through its destructors (otherwise, the memory
 * held in the parser stack would be leaked).
 */
static CompilationStatus _lexicalError(Token * token, const char * reason) {
	char * lexeme = escape(token->lexeme);
	logError(_logger, "Lexical error at line %d: %s (lexeme \"%s\").", token->line, reason, lexeme);
	free(lexeme);
	token->label = UNKNOWN;
	pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return FAILED;
}

/**
 * Logs a lexical-analyzer action over a token in DEBUGGING level.
 */
static void _logTokenAction(const char * actionName, Token * token) {
	char * _lexeme = escape(token->lexeme);
	logDebugging(_logger, WARNING_COLOR "%s" DEFAULT_COLOR ": Token(context=%d, label=%d, length=%d, lexeme=%s\"%s\"%s, line=%d, semanticValue=%p)",
		actionName,
		token->context,
		token->label,
		token->length,
		INFORMATION_COLOR, _lexeme, DEFAULT_COLOR,
		token->line,
		token->semanticValue);
	free(_lexeme);
	_lexeme = NULL;
}

/**
 * Logs the token, pushes it to the parser, and releases it. The semantic
 * value is copied by the parser, so the ownership of any heap-memory inside
 * it (e.g., strings), is transferred to the parser.
 */
static CompilationStatus _pushAndDestroy(const char * actionName, Token * token) {
	_logTokenAction(actionName, token);
	CompilationStatus status = pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return status;
}

/**
 * Converts a sequence of digits into an integer. Returns false if the value
 * does not fit into a signed int.
 */
static bool _toInteger(const char * lexeme, int * value) {
	errno = 0;
	const long result = strtol(lexeme, NULL, 10);
	if (errno == ERANGE || INT_MAX < result) {
		return false;
	}
	*value = (int) result;
	return true;
}

/**
 * Removes the surrounding quotes of a string literal, and resolves its escape
 * sequences (\" and \\). The result uses heap-memory. Every other byte is
 * copied verbatim, so UTF-8 sequences (e.g., accents), are preserved.
 */
static char * _unquote(const char * lexeme, const unsigned int length) {
	char * string = calloc(length, sizeof(char));
	unsigned int size = 0;
	for (unsigned int k = 1; k < length - 1; ++k) {
		if (lexeme[k] == '\\') {
			++k;
		}
		string[size++] = lexeme[k];
	}
	string[size] = '\0';
	return string;
}

/* PUBLIC FUNCTIONS */

CompilationStatus DateLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, DATE);
	int year = 0;
	int month = 0;
	int day = 0;
	sscanf(token->lexeme, "%4d-%2d-%2d", &year, &month, &day);
	if (month < 1 || 12 < month) {
		return _lexicalError(token, "invalid month in ISO 8601 date");
	}
	if (day < 1 || _daysInMonth(year, month) < day) {
		return _lexicalError(token, "invalid day in ISO 8601 date");
	}
	token->semanticValue->date.year = year;
	token->semanticValue->date.month = month;
	token->semanticValue->date.day = day;
	return _pushAndDestroy(__FUNCTION__, token);
}

CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context) {
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, OPEN_COMMENT);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	enterLexicalAnalyzerContext(_lexicalAnalyzer, context);
	return IN_PROGRESS;
}

CompilationStatus EOFLexemeAction() {
	CompilationStatus status = IN_PROGRESS;
	Token * token = createToken(_lexicalAnalyzer, 0);
	_logTokenAction(__FUNCTION__, token);
	if (!popInputBuffer(_lexicalAnalyzer)) {
		status = pushToken(_lexicalAnalyzer, token);
		FlexContext context = currentLexicalAnalyzerContext(_lexicalAnalyzer);
		if (0 < context) {
			logError(_logger, "Lexical error at line %d: a multi-line comment was never closed.", token->line);
			status = FAILED;
		}
	}
	destroyToken(token);
	return status;
}

CompilationStatus IdentifierLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, IDENTIFIER);
	token->semanticValue->string = strdup(token->lexeme);
	return _pushAndDestroy(__FUNCTION__, token);
}

CompilationStatus IgnoredLexemeAction() {
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, IGNORED);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	return IN_PROGRESS;
}

CompilationStatus IntegerLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, INTEGER);
	if (!_toInteger(token->lexeme, &token->semanticValue->integer)) {
		return _lexicalError(token, "integer out of range");
	}
	return _pushAndDestroy(__FUNCTION__, token);
}

CompilationStatus KeywordLexemeAction(TokenLabel label) {
	Token * token = createToken(_lexicalAnalyzer, label);
	token->semanticValue->token = label;
	return _pushAndDestroy(__FUNCTION__, token);
}

CompilationStatus LeaveMultilineCommentLexemeAction() {
	leaveLexicalAnalyzerContext(_lexicalAnalyzer);
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, CLOSE_COMMENT);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	return IN_PROGRESS;
}

CompilationStatus MonthLexemeAction(const int month) {
	Token * token = createToken(_lexicalAnalyzer, MONTH);
	token->semanticValue->integer = month;
	return _pushAndDestroy(__FUNCTION__, token);
}

CompilationStatus OperatorLexemeAction(TokenLabel label) {
	Token * token = createToken(_lexicalAnalyzer, label);
	token->semanticValue->token = label;
	return _pushAndDestroy(__FUNCTION__, token);
}

CompilationStatus PunctuationLexemeAction(TokenLabel label) {
	Token * token = createToken(_lexicalAnalyzer, label);
	token->semanticValue->token = label;
	return _pushAndDestroy(__FUNCTION__, token);
}

CompilationStatus StringLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, STRING);
	token->semanticValue->string = _unquote(token->lexeme, token->length);
	return _pushAndDestroy(__FUNCTION__, token);
}

CompilationStatus UnknownLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, UNKNOWN);
	_logTokenAction(__FUNCTION__, token);
	return _lexicalError(token, "unknown symbol");
}

CompilationStatus WeekdayLexemeAction(const Weekday weekday) {
	Token * token = createToken(_lexicalAnalyzer, WEEKDAY);
	token->semanticValue->integer = weekday;
	return _pushAndDestroy(__FUNCTION__, token);
}
