%top{

#include "../../support/type/CompilationStatus.h"
#include "../../support/type/FlexContext.h"
#include "../../support/type/TokenLabel.h"
#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include "../syntactic-analysis/BisonParser.h"

}

%{

/**
 * @see https://westes.github.io/flex/manual/Definitions-Section.html#Definitions-Section
 */

extern CompilationStatus DateLexemeAction();
extern CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context);
extern CompilationStatus EOFLexemeAction();
extern CompilationStatus IdentifierLexemeAction();
extern CompilationStatus IgnoredLexemeAction();
extern CompilationStatus IntegerLexemeAction();
extern CompilationStatus KeywordLexemeAction(TokenLabel label);
extern CompilationStatus LeaveMultilineCommentLexemeAction();
extern CompilationStatus MonthLexemeAction(const int month);
extern CompilationStatus OperatorLexemeAction(TokenLabel label);
extern CompilationStatus PunctuationLexemeAction(TokenLabel label);
extern CompilationStatus StringLexemeAction();
extern CompilationStatus UnknownLexemeAction();
extern CompilationStatus WeekdayLexemeAction(const Weekday weekday);

%}

/**
 * Flex contexts (a.k.a. start conditions).
 *
 * @see https://westes.github.io/flex/manual/Start-Conditions.html
 */
%x MULTILINE_COMMENT

/**
 * Reusable patterns.
 *
 * @see https://westes.github.io/flex/manual/Matching.html
 * @see https://westes.github.io/flex/manual/Patterns.html
 */
blank				[ \t]+
digit				[0-9]
identifier			[A-Za-z_][A-Za-z0-9_]*
utf8				[\xC2-\xF4][\x80-\xBF]{1,3}
whitespace			[[:space:]]+

%%

%{

/**
 * @see https://westes.github.io/flex/manual/Rules-Section.html#Rules-Section
 */

%}

"/*"											{ return EnterMultilineCommentLexemeAction(MULTILINE_COMMENT); }
<MULTILINE_COMMENT>"*/"							{ return LeaveMultilineCommentLexemeAction(); }
<MULTILINE_COMMENT>[^*]+						{ return IgnoredLexemeAction(); }
<MULTILINE_COMMENT>.							{ return IgnoredLexemeAction(); }

"//"[^\n]*										{ return IgnoredLexemeAction(); }

	/* Declarations. */

"federation"									{ return KeywordLexemeAction(FEDERATION); }
"league"										{ return KeywordLexemeAction(LEAGUE); }
"tier"											{ return KeywordLexemeAction(TIER); }
"tournament"									{ return KeywordLexemeAction(TOURNAMENT); }
"template"										{ return KeywordLexemeAction(TEMPLATE); }
"use"											{ return KeywordLexemeAction(USE); }

	/* Tournament structure. */

"teams"											{ return KeywordLexemeAction(TEAMS); }
"format"										{ return KeywordLexemeAction(FORMAT); }
"round"{blank}"robin"							{ return KeywordLexemeAction(ROUND_ROBIN); }
"round-robin"									{ return KeywordLexemeAction(ROUND_ROBIN); }
"knockout"										{ return KeywordLexemeAction(KNOCKOUT); }
"groups"										{ return KeywordLexemeAction(GROUPS); }
"advance"										{ return KeywordLexemeAction(ADVANCE); }
"then"											{ return KeywordLexemeAction(THEN); }
"single"										{ return KeywordLexemeAction(SINGLE); }
"double"										{ return KeywordLexemeAction(DOUBLE); }

	/* Schedule and periodicity. */

"schedule"										{ return KeywordLexemeAction(SCHEDULE); }
"starting"										{ return KeywordLexemeAction(STARTING); }
"every"											{ return KeywordLexemeAction(EVERY); }
"day"s?											{ return KeywordLexemeAction(DAY); }
"week"s?										{ return KeywordLexemeAction(WEEK); }
"year"s?										{ return KeywordLexemeAction(YEAR); }
"season"s?										{ return KeywordLexemeAction(SEASON); }

"monday"										{ return WeekdayLexemeAction(MONDAY); }
"tuesday"										{ return WeekdayLexemeAction(TUESDAY); }
"wednesday"										{ return WeekdayLexemeAction(WEDNESDAY); }
"thursday"										{ return WeekdayLexemeAction(THURSDAY); }
"friday"										{ return WeekdayLexemeAction(FRIDAY); }
"saturday"										{ return WeekdayLexemeAction(SATURDAY); }
"sunday"										{ return WeekdayLexemeAction(SUNDAY); }

"january"										{ return MonthLexemeAction(1); }
"february"										{ return MonthLexemeAction(2); }
"march"											{ return MonthLexemeAction(3); }
"april"											{ return MonthLexemeAction(4); }
"may"											{ return MonthLexemeAction(5); }
"june"											{ return MonthLexemeAction(6); }
"july"											{ return MonthLexemeAction(7); }
"august"										{ return MonthLexemeAction(8); }
"september"										{ return MonthLexemeAction(9); }
"october"										{ return MonthLexemeAction(10); }
"november"										{ return MonthLexemeAction(11); }
"december"										{ return MonthLexemeAction(12); }

	/* Table zones and team selection. */

"zone"											{ return KeywordLexemeAction(ZONE); }
"zones"											{ return KeywordLexemeAction(ZONES); }
"champion"										{ return KeywordLexemeAction(CHAMPION); }
"relegate"										{ return KeywordLexemeAction(RELEGATE); }
"promote"										{ return KeywordLexemeAction(PROMOTE); }
"qualify"										{ return KeywordLexemeAction(QUALIFY); }
"first"											{ return KeywordLexemeAction(FIRST); }
"last"											{ return KeywordLexemeAction(LAST); }
"top"											{ return KeywordLexemeAction(TOP); }
"bottom"										{ return KeywordLexemeAction(BOTTOM); }
"position"s?									{ return KeywordLexemeAction(POSITION); }
"by"											{ return KeywordLexemeAction(BY); }
"average"										{ return KeywordLexemeAction(AVERAGE); }
"winner"										{ return KeywordLexemeAction(WINNER); }

	/* Points, results and tiebreakers (optional extension). */

"points"										{ return KeywordLexemeAction(POINTS); }
"win"											{ return KeywordLexemeAction(WIN); }
"draw"											{ return KeywordLexemeAction(DRAW); }
"loss"											{ return KeywordLexemeAction(LOSS); }
"results"										{ return KeywordLexemeAction(RESULTS); }
"penalties"										{ return KeywordLexemeAction(PENALTIES); }
"tiebreak"										{ return KeywordLexemeAction(TIEBREAK); }
"goal"{blank}"difference"						{ return KeywordLexemeAction(GOAL_DIFFERENCE); }
"goals"{blank}"for"								{ return KeywordLexemeAction(GOALS_FOR); }
"goals"{blank}"against"							{ return KeywordLexemeAction(GOALS_AGAINST); }
"head"{blank}"to"{blank}"head"					{ return KeywordLexemeAction(HEAD_TO_HEAD); }
"away"{blank}"goals"							{ return KeywordLexemeAction(AWAY_GOALS); }
"wins"											{ return KeywordLexemeAction(WINS); }

	/* Federation: procedures, movement and calendar. */

"cycle"											{ return KeywordLexemeAction(CYCLE); }
"movement"										{ return KeywordLexemeAction(MOVEMENT); }
"calendar"										{ return KeywordLexemeAction(CALENDAR); }
"host"											{ return KeywordLexemeAction(HOST); }
"in"											{ return KeywordLexemeAction(IN); }
"before"										{ return KeywordLexemeAction(BEFORE); }
"after"											{ return KeywordLexemeAction(AFTER); }
"if"											{ return KeywordLexemeAction(IF); }
"else"											{ return KeywordLexemeAction(ELSE); }
"otherwise"										{ return KeywordLexemeAction(OTHERWISE); }
"as"											{ return KeywordLexemeAction(AS); }
"from"											{ return KeywordLexemeAction(FROM); }
"to"											{ return KeywordLexemeAction(TO); }
"of"											{ return KeywordLexemeAction(OF); }
"and"											{ return KeywordLexemeAction(AND); }
"or"											{ return KeywordLexemeAction(OR); }
"not"											{ return KeywordLexemeAction(NOT); }

	/* Operators. */

"+"												{ return OperatorLexemeAction(ADD); }
"-"												{ return OperatorLexemeAction(SUB); }
"="												{ return OperatorLexemeAction(ASSIGN); }
"=="											{ return OperatorLexemeAction(EQUAL); }
"!="											{ return OperatorLexemeAction(NOT_EQUAL); }
"<"												{ return OperatorLexemeAction(LESS_THAN); }
"<="											{ return OperatorLexemeAction(LESS_THAN_OR_EQUAL); }
">"												{ return OperatorLexemeAction(GREATER_THAN); }
">="											{ return OperatorLexemeAction(GREATER_THAN_OR_EQUAL); }

	/* Punctuation. */

"{"												{ return PunctuationLexemeAction(OPEN_BRACE); }
"}"												{ return PunctuationLexemeAction(CLOSE_BRACE); }
"("												{ return PunctuationLexemeAction(OPEN_PARENTHESIS); }
")"												{ return PunctuationLexemeAction(CLOSE_PARENTHESIS); }
","												{ return PunctuationLexemeAction(COMMA); }

	/* Literals. */

{digit}{4}"-"{digit}{2}"-"{digit}{2}			{ return DateLexemeAction(); }
{digit}+										{ return IntegerLexemeAction(); }
\"([^"\\\n]|\\[\\"])*\"							{ return StringLexemeAction(); }
{identifier}									{ return IdentifierLexemeAction(); }

{whitespace}									{ return IgnoredLexemeAction(); }
{utf8}											{ return UnknownLexemeAction(); }
.												{ return UnknownLexemeAction(); }

<<EOF>>											{ return EOFLexemeAction(); }

%%

/**
 * @see https://westes.github.io/flex/manual/User-Code-Section.html#User-Code-Section
 */

#include "FlexActions.h"
#include "FlexExport.h"
