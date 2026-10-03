%{

#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonActions.h"

/**
 * The error reporting function for Bison parser. The location is updated by
 * the "pushToken" function with the line of every token pushed.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Error-Reporting-Function.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Tracking-Locations.html
 */
void yyerror(const YYLTYPE * location, const char * message) {
	SyntaxErrorAction(location, message);
}

%}

// You touch this, and you die.
%define api.pure full
%define api.push-pull push
%define api.value.union.name SemanticValue
%define parse.error detailed
%locations

%union {
	/** Terminals. */

	Date date;
	signed int integer;
	char * string;
	TokenLabel token;

	/** Non-terminals. */

	Calendar * calendar;
	Condition * condition;
	Criterion * criterion;
	Cycle * cycle;
	Declaration * declaration;
	Federation * federation;
	FederationItem * federationItem;
	Format * format;
	Legs legs;
	Movement * movement;
	MovementRule * movementRule;
	MovementSubject * movementSubject;
	Period * period;
	Points * points;
	PointsAssignment * pointsAssignment;
	Position * position;
	Program * program;
	Ranking * ranking;
	Result * result;
	Results * results;
	Schedule * schedule;
	Selector * selector;
	Statement * statement;
	Teams * teams;
	Template * templateDefinition;
	Tiebreak * tiebreak;
	Tournament * tournament;
	TournamentItem * tournamentItem;
	YearExpression * yearExpression;
	Zone * zone;

	/** Lists (see the element type in the name of each field). */

	List * arguments;
	List * criteria;
	List * declarations;
	List * federationItems;
	List * movementRules;
	List * parameters;
	List * pointsAssignments;
	List * resultList;
	List * statements;
	List * teamNames;
	List * tournamentItems;
	List * zones;
}

/**
 * Destructors. This functions are executed when the parser discards a symbol
 * (e.g., after a syntax error), so the partial AST is released without leaks.
 * The AST root node ("program" non-terminal) must not have a destructor, or
 * it would drop the entire tree even if the parsing succeeds.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Destructor-Decl.html
 */
%destructor { free($$); } <string>
%destructor { destroyCalendar($$); } <calendar>
%destructor { destroyCondition($$); } <condition>
%destructor { destroyCriterion($$); } <criterion>
%destructor { destroyCycle($$); } <cycle>
%destructor { destroyDeclaration($$); } <declaration>
%destructor { destroyFederation($$); } <federation>
%destructor { destroyFederationItem($$); } <federationItem>
%destructor { destroyFormat($$); } <format>
%destructor { destroyMovement($$); } <movement>
%destructor { destroyMovementRule($$); } <movementRule>
%destructor { destroyMovementSubject($$); } <movementSubject>
%destructor { destroyPeriod($$); } <period>
%destructor { destroyPoints($$); } <points>
%destructor { destroyPointsAssignment($$); } <pointsAssignment>
%destructor { destroyPosition($$); } <position>
%destructor { destroyRanking($$); } <ranking>
%destructor { destroyResult($$); } <result>
%destructor { destroyResults($$); } <results>
%destructor { destroySchedule($$); } <schedule>
%destructor { destroySelector($$); } <selector>
%destructor { destroyStatement($$); } <statement>
%destructor { destroyTeams($$); } <teams>
%destructor { destroyTemplate($$); } <templateDefinition>
%destructor { destroyTiebreak($$); } <tiebreak>
%destructor { destroyTournament($$); } <tournament>
%destructor { destroyTournamentItem($$); } <tournamentItem>
%destructor { destroyYearExpression($$); } <yearExpression>
%destructor { destroyZone($$); } <zone>
%destructor { destroyArgumentList($$); } <arguments>
%destructor { destroyCriterionList($$); } <criteria>
%destructor { destroyDeclarationList($$); } <declarations>
%destructor { destroyFederationItemList($$); } <federationItems>
%destructor { destroyMovementRuleList($$); } <movementRules>
%destructor { destroyStringList($$); } <parameters>
%destructor { destroyPointsAssignmentList($$); } <pointsAssignments>
%destructor { destroyResultList($$); } <resultList>
%destructor { destroyStatementList($$); } <statements>
%destructor { destroyStringList($$); } <teamNames>
%destructor { destroyTournamentItemList($$); } <tournamentItems>
%destructor { destroyZoneList($$); } <zones>

/**
 * Terminals. The quoted aliases are only used to improve the messages of
 * syntax errors.
 */

/* Literals. */
%token <date> DATE "date"
%token <integer> INTEGER "integer"
%token <integer> MONTH "month"
%token <integer> WEEKDAY "weekday"
%token <string> IDENTIFIER "identifier"
%token <string> STRING "string"

/* Declarations. */
%token <token> FEDERATION "federation"
%token <token> LEAGUE "league"
%token <token> TEMPLATE "template"
%token <token> TIER "tier"
%token <token> TOURNAMENT "tournament"
%token <token> USE "use"

/* Tournament structure. */
%token <token> ADVANCE "advance"
%token <token> DOUBLE "double"
%token <token> FORMAT "format"
%token <token> GROUPS "groups"
%token <token> KNOCKOUT "knockout"
%token <token> ROUND_ROBIN "round robin"
%token <token> SINGLE "single"
%token <token> TEAMS "teams"
%token <token> THEN "then"

/* Schedule and periodicity. */
%token <token> DAY "day"
%token <token> EVERY "every"
%token <token> SCHEDULE "schedule"
%token <token> SEASON "season"
%token <token> STARTING "starting"
%token <token> WEEK "week"
%token <token> YEAR "year"

/* Table zones and team selection. */
%token <token> AVERAGE "average"
%token <token> BOTTOM "bottom"
%token <token> BY "by"
%token <token> CHAMPION "champion"
%token <token> FIRST "first"
%token <token> LAST "last"
%token <token> POSITION "position"
%token <token> PROMOTE "promote"
%token <token> QUALIFY "qualify"
%token <token> RELEGATE "relegate"
%token <token> TOP "top"
%token <token> WINNER "winner"
%token <token> ZONE "zone"
%token <token> ZONES "zones"

/* Points, results and tiebreakers. */
%token <token> AWAY_GOALS "away goals"
%token <token> DRAW "draw"
%token <token> GOAL_DIFFERENCE "goal difference"
%token <token> GOALS_AGAINST "goals against"
%token <token> GOALS_FOR "goals for"
%token <token> HEAD_TO_HEAD "head to head"
%token <token> LOSS "loss"
%token <token> PENALTIES "penalties"
%token <token> POINTS "points"
%token <token> RESULTS "results"
%token <token> TIEBREAK "tiebreak"
%token <token> WIN "win"
%token <token> WINS "wins"

/* Federation: procedures, movement and calendar. */
%token <token> AFTER "after"
%token <token> AND "and"
%token <token> AS "as"
%token <token> BEFORE "before"
%token <token> CALENDAR "calendar"
%token <token> CYCLE "cycle"
%token <token> ELSE "else"
%token <token> FROM "from"
%token <token> HOST "host"
%token <token> IF "if"
%token <token> IN "in"
%token <token> MOVEMENT "movement"
%token <token> NOT "not"
%token <token> OF "of"
%token <token> OR "or"
%token <token> OTHERWISE "otherwise"
%token <token> TO "to"

/* Operators. */
%token <token> ADD "+"
%token <token> ASSIGN "="
%token <token> EQUAL "=="
%token <token> GREATER_THAN ">"
%token <token> GREATER_THAN_OR_EQUAL ">="
%token <token> LESS_THAN "<"
%token <token> LESS_THAN_OR_EQUAL "<="
%token <token> NOT_EQUAL "!="
%token <token> SUB "-"

/* Punctuation. */
%token <token> CLOSE_BRACE "}"
%token <token> CLOSE_PARENTHESIS ")"
%token <token> COMMA ","
%token <token> OPEN_BRACE "{"
%token <token> OPEN_PARENTHESIS "("

/* Only for logging purposes (never reach the grammar). */
%token <token> CLOSE_COMMENT
%token <token> IGNORED
%token <token> OPEN_COMMENT
%token <token> UNKNOWN "invalid symbol"

/** Non-terminals. */
%type <arguments> arguments argumentList
%type <calendar> calendar
%type <condition> condition
%type <criteria> criteria
%type <criterion> criterion
%type <cycle> cycle
%type <date> date
%type <declaration> declaration
%type <declarations> declarations
%type <federation> federation
%type <federationItem> federationItem
%type <federationItems> federationItems
%type <format> format
%type <integer> weekdays
%type <legs> legs
%type <movement> movement
%type <movementRule> movementRule
%type <movementRules> movementRules
%type <movementSubject> movementSubject
%type <parameters> parameters parameterList
%type <period> period
%type <points> points
%type <pointsAssignment> pointsAssignment
%type <pointsAssignments> pointsAssignments
%type <position> position
%type <program> program
%type <ranking> ranking
%type <result> result
%type <resultList> resultList
%type <results> results
%type <schedule> schedule
%type <selector> selector
%type <statement> statement ifStatement
%type <statements> statements elseBranch otherwiseBranch
%type <string> alias
%type <teamNames> teamNames
%type <teams> teams
%type <templateDefinition> template
%type <tiebreak> tiebreak
%type <tournament> tournament league
%type <tournamentItem> tournamentItem
%type <tournamentItems> tournamentItems
%type <yearExpression> yearExpression start
%type <zone> zone
%type <zones> zones

/**
 * Precedence and associativity.
 *
 * @see https://en.cppreference.com/w/cpp/language/operator_precedence.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Precedence.html
 */
%left OR
%left AND
%precedence NOT
%left ADD SUB

%%

// IMPORTANT: To use λ in the following grammar, use the %empty symbol.

/* ========================================================================== */
/* Program.                                                                   */
/* ========================================================================== */

program: declarations													{ $$ = ProgramSemanticAction($1); }
	;

declarations: declaration												{ $$ = ListSemanticAction($1); }
	| declarations[list] declaration[element]							{ $$ = AppendSemanticAction($list, $element); }
	;

declaration: tournament													{ $$ = TournamentDeclarationSemanticAction($1); }
	| federation														{ $$ = FederationDeclarationSemanticAction($1); }
	| template															{ $$ = TemplateDeclarationSemanticAction($1); }
	;

/* ========================================================================== */
/* Tournaments and leagues.                                                   */
/* ========================================================================== */

tournament: TOURNAMENT IDENTIFIER[name] OPEN_BRACE tournamentItems[items] CLOSE_BRACE
																		{ $$ = TournamentSemanticAction($name, $items); }
	;

league: LEAGUE IDENTIFIER[name] TIER INTEGER[tier] OPEN_BRACE tournamentItems[items] CLOSE_BRACE
																		{ $$ = LeagueSemanticAction($name, $tier, $items); }
	;

tournamentItems: tournamentItem											{ $$ = ListSemanticAction($1); }
	| tournamentItems[list] tournamentItem[element]						{ $$ = AppendSemanticAction($list, $element); }
	;

tournamentItem: teams													{ $$ = TeamsItemSemanticAction($1); }
	| format															{ $$ = FormatItemSemanticAction($1); }
	| schedule															{ $$ = ScheduleItemSemanticAction($1); }
	| zone																{ $$ = ZoneItemSemanticAction($1); }
	| points															{ $$ = PointsItemSemanticAction($1); }
	| USE POINTS IDENTIFIER[name]										{ $$ = UseItemSemanticAction(TEMPLATE_POINTS, $name); }
	| USE ZONES IDENTIFIER[name]										{ $$ = UseItemSemanticAction(TEMPLATE_ZONES, $name); }
	| tiebreak															{ $$ = TiebreakItemSemanticAction($1); }
	| results															{ $$ = ResultsItemSemanticAction($1); }
	;

/* Participants: an explicit list (at least 2), or selected from another tournament. */

teams: TEAMS OPEN_BRACE teamNames[names] CLOSE_BRACE					{ $$ = TeamListSemanticAction($names); }
	| TEAMS FROM IDENTIFIER[source] selector							{ $$ = TeamsFromTournamentSemanticAction($source, $selector); }
	;

teamNames: STRING[first] COMMA STRING[second]							{ $$ = AppendSemanticAction(ListSemanticAction($first), $second); }
	| teamNames[list] COMMA STRING[element]								{ $$ = AppendSemanticAction($list, $element); }
	;

/* Formats. */

format: FORMAT ROUND_ROBIN legs											{ $$ = RoundRobinFormatSemanticAction($legs); }
	| FORMAT KNOCKOUT legs												{ $$ = KnockoutFormatSemanticAction($legs); }
	| FORMAT GROUPS OF INTEGER[size] ADVANCE INTEGER[advancing] THEN KNOCKOUT legs
																		{ $$ = GroupsFormatSemanticAction($size, $advancing, $legs); }
	;

legs: %empty															{ $$ = LEGS_SINGLE; }
	| SINGLE															{ $$ = LEGS_SINGLE; }
	| DOUBLE															{ $$ = LEGS_DOUBLE; }
	;

/* Schedule: an ISO 8601 date (or a yearless date inside a federation), and a period. */

schedule: SCHEDULE STARTING date EVERY period							{ $$ = ScheduleSemanticAction($date, $period); }
	;

date: DATE																{ $$ = $1; }
	| MONTH[month] INTEGER[day]											{ $$ = YearlessDateSemanticAction($month, $day); }
	;

period: DAY																{ $$ = DaysPeriodSemanticAction(1); }
	| INTEGER[amount] DAY												{ $$ = DaysPeriodSemanticAction($amount); }
	| WEEK																{ $$ = WeeksPeriodSemanticAction(1); }
	| INTEGER[amount] WEEK												{ $$ = WeeksPeriodSemanticAction($amount); }
	| weekdays															{ $$ = WeekdaysPeriodSemanticAction($weekdays); }
	;

weekdays: WEEKDAY														{ $$ = WeekdaySetSemanticAction(0, $1); }
	| weekdays[set] AND WEEKDAY[weekday]								{ $$ = WeekdaySetSemanticAction($set, $weekday); }
	;

/* Zones of the table. */

zone: ZONE IDENTIFIER[name] CHAMPION selector							{ $$ = ZoneSemanticAction($name, ZONE_CHAMPION, NULL, $selector); }
	| ZONE IDENTIFIER[name] PROMOTE selector							{ $$ = ZoneSemanticAction($name, ZONE_PROMOTE, NULL, $selector); }
	| ZONE IDENTIFIER[name] RELEGATE selector							{ $$ = ZoneSemanticAction($name, ZONE_RELEGATE, NULL, $selector); }
	| ZONE IDENTIFIER[name] QUALIFY TO STRING[cup] selector				{ $$ = ZoneSemanticAction($name, ZONE_QUALIFY, $cup, $selector); }
	;

zones: zone																{ $$ = ListSemanticAction($1); }
	| zones[list] zone[element]											{ $$ = AppendSemanticAction($list, $element); }
	;

/* Selection of positions over a ranking (the table, by default). */

selector: FIRST ranking													{ $$ = TopSelectorSemanticAction(1, $ranking); }
	| LAST ranking														{ $$ = BottomSelectorSemanticAction(1, $ranking); }
	| TOP INTEGER[count] ranking										{ $$ = TopSelectorSemanticAction($count, $ranking); }
	| BOTTOM INTEGER[count] ranking										{ $$ = BottomSelectorSemanticAction($count, $ranking); }
	| POSITION position[single] ranking									{ $$ = RangeSelectorSemanticAction($single, NULL, $ranking); }
	| POSITION position[from] TO position[to] ranking					{ $$ = RangeSelectorSemanticAction($from, $to, $ranking); }
	;

position: INTEGER														{ $$ = FromTopPositionSemanticAction($1); }
	| FIRST																{ $$ = FromTopPositionSemanticAction(1); }
	| LAST																{ $$ = FromBottomPositionSemanticAction(0); }
	| LAST SUB INTEGER[offset]											{ $$ = FromBottomPositionSemanticAction($offset); }
	;

ranking: %empty															{ $$ = TableRankingSemanticAction(); }
	| BY AVERAGE OF LAST INTEGER[seasons] SEASON						{ $$ = AverageRankingSemanticAction($seasons); }
	;

/* Points, tiebreakers and results (optional extension). */

points: POINTS OPEN_BRACE pointsAssignments[list] CLOSE_BRACE			{ $$ = PointsSemanticAction($list); }
	;

pointsAssignments: pointsAssignment										{ $$ = ListSemanticAction($1); }
	| pointsAssignments[list] COMMA pointsAssignment[element]			{ $$ = AppendSemanticAction($list, $element); }
	;

pointsAssignment: WIN ASSIGN INTEGER[amount]							{ $$ = PointsAssignmentSemanticAction(OUTCOME_WIN, $amount); }
	| DRAW ASSIGN INTEGER[amount]										{ $$ = PointsAssignmentSemanticAction(OUTCOME_DRAW, $amount); }
	| LOSS ASSIGN INTEGER[amount]										{ $$ = PointsAssignmentSemanticAction(OUTCOME_LOSS, $amount); }
	;

tiebreak: TIEBREAK BY criteria											{ $$ = TiebreakSemanticAction($criteria); }
	;

criteria: criterion														{ $$ = ListSemanticAction($1); }
	| criteria[list] COMMA criterion[element]							{ $$ = AppendSemanticAction($list, $element); }
	;

criterion: GOAL_DIFFERENCE												{ $$ = CriterionSemanticAction(CRITERION_GOAL_DIFFERENCE); }
	| GOALS_FOR															{ $$ = CriterionSemanticAction(CRITERION_GOALS_FOR); }
	| GOALS_AGAINST														{ $$ = CriterionSemanticAction(CRITERION_GOALS_AGAINST); }
	| HEAD_TO_HEAD														{ $$ = CriterionSemanticAction(CRITERION_HEAD_TO_HEAD); }
	| WINS																{ $$ = CriterionSemanticAction(CRITERION_WINS); }
	| AWAY_GOALS														{ $$ = CriterionSemanticAction(CRITERION_AWAY_GOALS); }
	;

results: RESULTS OPEN_BRACE resultList[list] CLOSE_BRACE				{ $$ = ResultsSemanticAction(false, 0, $list); }
	| RESULTS INTEGER[season] OPEN_BRACE resultList[list] CLOSE_BRACE	{ $$ = ResultsSemanticAction(true, $season, $list); }
	;

resultList: result														{ $$ = ListSemanticAction($1); }
	| resultList[list] result[element]									{ $$ = AppendSemanticAction($list, $element); }
	;

result: STRING[home] INTEGER[homeGoals] SUB INTEGER[awayGoals] STRING[away]
																		{ $$ = ResultSemanticAction($home, $homeGoals, $awayGoals, $away); }
	| STRING[home] INTEGER[homeGoals] SUB INTEGER[awayGoals] STRING[away] PENALTIES INTEGER[homePenalties] SUB INTEGER[awayPenalties]
																		{ $$ = PenaltiesResultSemanticAction($home, $homeGoals, $awayGoals, $away, $homePenalties, $awayPenalties); }
	;

/* ========================================================================== */
/* Templates.                                                                 */
/* ========================================================================== */

template: TEMPLATE POINTS IDENTIFIER[name] OPEN_BRACE pointsAssignments[list] CLOSE_BRACE
																		{ $$ = PointsTemplateSemanticAction($name, PointsSemanticAction($list)); }
	| TEMPLATE ZONES IDENTIFIER[name] OPEN_BRACE zones[list] CLOSE_BRACE
																		{ $$ = ZonesTemplateSemanticAction($name, $list); }
	;

/* ========================================================================== */
/* Federations.                                                               */
/* ========================================================================== */

federation: FEDERATION IDENTIFIER[name] OPEN_BRACE federationItems[items] CLOSE_BRACE
																		{ $$ = FederationSemanticAction($name, $items); }
	;

federationItems: federationItem											{ $$ = ListSemanticAction($1); }
	| federationItems[list] federationItem[element]						{ $$ = AppendSemanticAction($list, $element); }
	;

federationItem: template												{ $$ = TemplateFederationItemSemanticAction($1); }
	| tournament														{ $$ = TournamentFederationItemSemanticAction($1); }
	| league															{ $$ = TournamentFederationItemSemanticAction($1); }
	| cycle																{ $$ = CycleFederationItemSemanticAction($1); }
	| movement															{ $$ = MovementFederationItemSemanticAction($1); }
	| calendar															{ $$ = CalendarFederationItemSemanticAction($1); }
	;

/* Procedures (without return value). */

cycle: CYCLE IDENTIFIER[name] OPEN_PARENTHESIS parameters CLOSE_PARENTHESIS OPEN_BRACE statements[body] CLOSE_BRACE
																		{ $$ = CycleSemanticAction($name, $parameters, $body); }
	;

parameters: %empty														{ $$ = EmptyListSemanticAction(); }
	| parameterList														{ $$ = $1; }
	;

parameterList: IDENTIFIER												{ $$ = ListSemanticAction($1); }
	| parameterList[list] COMMA IDENTIFIER[element]						{ $$ = AppendSemanticAction($list, $element); }
	;

/* Promotion and relegation between leagues. */

movement: MOVEMENT OPEN_BRACE movementRules[rules] CLOSE_BRACE			{ $$ = MovementSemanticAction($rules); }
	;

movementRules: movementRule												{ $$ = ListSemanticAction($1); }
	| movementRules[list] movementRule[element]							{ $$ = AppendSemanticAction($list, $element); }
	;

movementRule: RELEGATE movementSubject[subject] FROM IDENTIFIER[source] TO IDENTIFIER[target]
																		{ $$ = MovementRuleSemanticAction(MOVEMENT_RELEGATE, $subject, $source, $target); }
	| PROMOTE movementSubject[subject] FROM IDENTIFIER[source] TO IDENTIFIER[target]
																		{ $$ = MovementRuleSemanticAction(MOVEMENT_PROMOTE, $subject, $source, $target); }
	;

movementSubject: selector												{ $$ = SelectionSubjectSemanticAction($1); }
	| IDENTIFIER[zone]													{ $$ = NamedSubjectSemanticAction(SUBJECT_ZONE, $zone); }
	| STRING[team]														{ $$ = NamedSubjectSemanticAction(SUBJECT_TEAM, $team); }
	| WINNER OF IDENTIFIER[cup]											{ $$ = NamedSubjectSemanticAction(SUBJECT_WINNER, $cup); }
	;

/* Multi-year calendar. */

calendar: CALENDAR FROM INTEGER[from] TO INTEGER[to] OPEN_BRACE statements[body] CLOSE_BRACE
																		{ $$ = CalendarSemanticAction($from, $to, $body); }
	;

/* ========================================================================== */
/* Calendar statements.                                                       */
/* ========================================================================== */

statements: %empty														{ $$ = EmptyListSemanticAction(); }
	| statements[list] statement[element]								{ $$ = AppendSemanticAction($list, $element); }
	;

statement: HOST IDENTIFIER[name]										{ $$ = HostStatementSemanticAction($name, HOST_CURRENT_YEAR, NULL, 0); }
	| HOST IDENTIFIER[name] IN yearExpression[year]						{ $$ = HostStatementSemanticAction($name, HOST_EXPLICIT_YEAR, $year, 0); }
	| HOST IDENTIFIER[name] INTEGER[offset] YEAR BEFORE					{ $$ = HostStatementSemanticAction($name, HOST_YEARS_BEFORE, NULL, $offset); }
	| HOST IDENTIFIER[name] INTEGER[offset] YEAR AFTER					{ $$ = HostStatementSemanticAction($name, HOST_YEARS_AFTER, NULL, $offset); }
	| ifStatement														{ $$ = $1; }
	| EVERY YEAR alias OPEN_BRACE statements[body] CLOSE_BRACE			{ $$ = EveryStatementSemanticAction(1, NULL, $alias, $body, NULL); }
	| EVERY INTEGER[interval] YEAR start alias OPEN_BRACE statements[body] CLOSE_BRACE otherwiseBranch[otherwise]
																		{ $$ = EveryStatementSemanticAction($interval, $start, $alias, $body, $otherwise); }
	| IDENTIFIER[cycle] OPEN_PARENTHESIS arguments CLOSE_PARENTHESIS	{ $$ = CallStatementSemanticAction($cycle, $arguments); }
	;

ifStatement: IF condition OPEN_BRACE statements[body] CLOSE_BRACE elseBranch[alternative]
																		{ $$ = IfStatementSemanticAction($condition, $body, $alternative); }
	;

elseBranch: %empty														{ $$ = NULL; }
	| ELSE OPEN_BRACE statements[body] CLOSE_BRACE						{ $$ = $body; }
	| ELSE ifStatement[chained]											{ $$ = ListSemanticAction($chained); }
	;

start: %empty															{ $$ = NULL; }
	| STARTING yearExpression											{ $$ = $2; }
	;

alias: %empty															{ $$ = NULL; }
	| AS IDENTIFIER														{ $$ = $2; }
	;

otherwiseBranch: %empty													{ $$ = NULL; }
	| OTHERWISE OPEN_BRACE statements[body] CLOSE_BRACE					{ $$ = $body; }
	;

arguments: %empty														{ $$ = EmptyListSemanticAction(); }
	| argumentList														{ $$ = $1; }
	;

argumentList: yearExpression											{ $$ = ListSemanticAction($1); }
	| argumentList[list] COMMA yearExpression[element]					{ $$ = AppendSemanticAction($list, $element); }
	;

condition: condition[left] OR condition[right]							{ $$ = LogicalConditionSemanticAction($left, $right, CONDITION_OR); }
	| condition[left] AND condition[right]								{ $$ = LogicalConditionSemanticAction($left, $right, CONDITION_AND); }
	| NOT condition[operand]											{ $$ = NotConditionSemanticAction($operand); }
	| OPEN_PARENTHESIS condition[inner] CLOSE_PARENTHESIS				{ $$ = $inner; }
	| yearExpression[left] EQUAL yearExpression[right]					{ $$ = ComparisonConditionSemanticAction($left, COMPARISON_EQUAL, $right); }
	| yearExpression[left] NOT_EQUAL yearExpression[right]				{ $$ = ComparisonConditionSemanticAction($left, COMPARISON_NOT_EQUAL, $right); }
	| yearExpression[left] LESS_THAN yearExpression[right]				{ $$ = ComparisonConditionSemanticAction($left, COMPARISON_LESS_THAN, $right); }
	| yearExpression[left] LESS_THAN_OR_EQUAL yearExpression[right]		{ $$ = ComparisonConditionSemanticAction($left, COMPARISON_LESS_THAN_OR_EQUAL, $right); }
	| yearExpression[left] GREATER_THAN yearExpression[right]			{ $$ = ComparisonConditionSemanticAction($left, COMPARISON_GREATER_THAN, $right); }
	| yearExpression[left] GREATER_THAN_OR_EQUAL yearExpression[right]	{ $$ = ComparisonConditionSemanticAction($left, COMPARISON_GREATER_THAN_OR_EQUAL, $right); }
	;

yearExpression: yearExpression[left] ADD yearExpression[right]			{ $$ = ArithmeticYearExpressionSemanticAction($left, $right, YEAR_ADDITION); }
	| yearExpression[left] SUB yearExpression[right]					{ $$ = ArithmeticYearExpressionSemanticAction($left, $right, YEAR_SUBTRACTION); }
	| INTEGER															{ $$ = LiteralYearExpressionSemanticAction($1); }
	| IDENTIFIER														{ $$ = VariableYearExpressionSemanticAction($1); }
	;

%%
