#include "BisonActions.h"

/* MODULE INTERNAL STATE */

static CompilerState * _compilerState = NULL;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownBisonActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: BisonActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_compilerState = NULL;
}

ModuleDestructor initializeBisonActionsModule(CompilerState * compilerState) {
	_compilerState = compilerState;
	_logger = createLogger("BisonActions");
	return _shutdownBisonActionsModule;
}

/* PRIVATE FUNCTIONS */

static void _logSyntacticAnalyzerAction(const char * functionName);
static Statement * _statement(const StatementType type);
static TournamentItem * _tournamentItem(const TournamentItemType type);

/**
 * Logs a syntactic-analyzer action in DEBUGGING level.
 */
static void _logSyntacticAnalyzerAction(const char * functionName) {
	logDebugging(_logger, "%s", functionName);
}

static Statement * _statement(const StatementType type) {
	Statement * statement = calloc(1, sizeof(Statement));
	statement->type = type;
	return statement;
}

static TournamentItem * _tournamentItem(const TournamentItemType type) {
	TournamentItem * item = calloc(1, sizeof(TournamentItem));
	item->type = type;
	return item;
}

/* PUBLIC FUNCTIONS */

void SyntaxErrorAction(const YYLTYPE * location, const char * message) {
	if (location != NULL && 0 < location->first_line) {
		logError(_logger, "Syntax error at line %d: %s.", location->first_line, message);
	}
	else {
		logError(_logger, "Syntax error: %s.", message);
	}
}

/* Generic lists. */

List * AppendSemanticAction(List * list, void * element) {
	return appendToList(list, element);
}

List * EmptyListSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return createList();
}

List * ListSemanticAction(void * element) {
	return appendToList(createList(), element);
}

/* Program and declarations. */

Program * ProgramSemanticAction(List * declarations) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Program * program = calloc(1, sizeof(Program));
	program->declarations = declarations;
	_compilerState->abstractSyntaxtTree = program;
	return program;
}

Declaration * FederationDeclarationSemanticAction(Federation * federation) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->federation = federation;
	declaration->type = DECLARATION_FEDERATION;
	return declaration;
}

Declaration * TemplateDeclarationSemanticAction(Template * templateDefinition) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->templateDefinition = templateDefinition;
	declaration->type = DECLARATION_TEMPLATE;
	return declaration;
}

Declaration * TournamentDeclarationSemanticAction(Tournament * tournament) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->tournament = tournament;
	declaration->type = DECLARATION_TOURNAMENT;
	return declaration;
}

/* Tournaments and leagues. */

Tournament * LeagueSemanticAction(char * name, const int tier, List * items) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Tournament * tournament = calloc(1, sizeof(Tournament));
	tournament->name = name;
	tournament->type = KIND_LEAGUE;
	tournament->tier = tier;
	tournament->items = items;
	return tournament;
}

Tournament * TournamentSemanticAction(char * name, List * items) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Tournament * tournament = calloc(1, sizeof(Tournament));
	tournament->name = name;
	tournament->type = KIND_TOURNAMENT;
	tournament->tier = 0;
	tournament->items = items;
	return tournament;
}

TournamentItem * FormatItemSemanticAction(Format * format) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TournamentItem * item = _tournamentItem(ITEM_FORMAT);
	item->format = format;
	return item;
}

TournamentItem * PointsItemSemanticAction(Points * points) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TournamentItem * item = _tournamentItem(ITEM_POINTS);
	item->points = points;
	return item;
}

TournamentItem * ResultsItemSemanticAction(Results * results) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TournamentItem * item = _tournamentItem(ITEM_RESULTS);
	item->results = results;
	return item;
}

TournamentItem * ScheduleItemSemanticAction(Schedule * schedule) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TournamentItem * item = _tournamentItem(ITEM_SCHEDULE);
	item->schedule = schedule;
	return item;
}

TournamentItem * TeamsItemSemanticAction(Teams * teams) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TournamentItem * item = _tournamentItem(ITEM_TEAMS);
	item->teams = teams;
	return item;
}

TournamentItem * TiebreakItemSemanticAction(Tiebreak * tiebreak) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TournamentItem * item = _tournamentItem(ITEM_TIEBREAK);
	item->tiebreak = tiebreak;
	return item;
}

TournamentItem * UseItemSemanticAction(const TemplateType type, char * name) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Use * use = calloc(1, sizeof(Use));
	use->type = type;
	use->name = name;
	TournamentItem * item = _tournamentItem(ITEM_USE);
	item->use = use;
	return item;
}

TournamentItem * ZoneItemSemanticAction(Zone * zone) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TournamentItem * item = _tournamentItem(ITEM_ZONE);
	item->zone = zone;
	return item;
}

Teams * TeamListSemanticAction(List * names) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Teams * teams = calloc(1, sizeof(Teams));
	teams->names = names;
	teams->type = TEAMS_LIST;
	return teams;
}

Teams * TeamsFromTournamentSemanticAction(char * sourceTournament, Selector * selector) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Teams * teams = calloc(1, sizeof(Teams));
	teams->sourceTournament = sourceTournament;
	teams->selector = selector;
	teams->type = TEAMS_FROM_TOURNAMENT;
	return teams;
}

Format * GroupsFormatSemanticAction(const int groupSize, const int advancing, const Legs legs) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Format * format = calloc(1, sizeof(Format));
	format->type = FORMAT_GROUPS;
	format->groupSize = groupSize;
	format->advancing = advancing;
	format->legs = legs;
	return format;
}

Format * KnockoutFormatSemanticAction(const Legs legs) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Format * format = calloc(1, sizeof(Format));
	format->type = FORMAT_KNOCKOUT;
	format->legs = legs;
	return format;
}

Format * RoundRobinFormatSemanticAction(const Legs legs) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Format * format = calloc(1, sizeof(Format));
	format->type = FORMAT_ROUND_ROBIN;
	format->legs = legs;
	return format;
}

Schedule * ScheduleSemanticAction(const Date start, Period * period) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Schedule * schedule = calloc(1, sizeof(Schedule));
	schedule->start = start;
	schedule->period = period;
	return schedule;
}

Date YearlessDateSemanticAction(const int month, const int day) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Date date = {
		.year = 0,
		.month = month,
		.day = day
	};
	return date;
}

Period * DaysPeriodSemanticAction(const int amount) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Period * period = calloc(1, sizeof(Period));
	period->type = PERIOD_DAYS;
	period->amount = amount;
	return period;
}

Period * WeekdaysPeriodSemanticAction(const unsigned int weekdays) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Period * period = calloc(1, sizeof(Period));
	period->type = PERIOD_WEEKDAYS;
	period->weekdays = weekdays;
	return period;
}

Period * WeeksPeriodSemanticAction(const int amount) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Period * period = calloc(1, sizeof(Period));
	period->type = PERIOD_WEEKS;
	period->amount = amount;
	return period;
}

int WeekdaySetSemanticAction(const int weekdays, const int weekday) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return weekdays | (1 << weekday);
}

Zone * ZoneSemanticAction(char * name, const ZoneKind kind, char * cup, Selector * selector) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Zone * zone = calloc(1, sizeof(Zone));
	zone->name = name;
	zone->kind = kind;
	zone->cup = cup;
	zone->selector = selector;
	return zone;
}

Selector * BottomSelectorSemanticAction(const int count, Ranking * ranking) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Selector * selector = calloc(1, sizeof(Selector));
	selector->type = SELECTOR_BOTTOM;
	selector->count = count;
	selector->ranking = ranking;
	return selector;
}

Selector * RangeSelectorSemanticAction(Position * from, Position * to, Ranking * ranking) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Selector * selector = calloc(1, sizeof(Selector));
	selector->type = SELECTOR_RANGE;
	selector->from = from;
	if (to == NULL) {
		// A single position P is the range [P, P].
		to = calloc(1, sizeof(Position));
		to->type = from->type;
		to->value = from->value;
	}
	selector->to = to;
	selector->ranking = ranking;
	return selector;
}

Selector * TopSelectorSemanticAction(const int count, Ranking * ranking) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Selector * selector = calloc(1, sizeof(Selector));
	selector->type = SELECTOR_TOP;
	selector->count = count;
	selector->ranking = ranking;
	return selector;
}

Position * FromBottomPositionSemanticAction(const int offset) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Position * position = calloc(1, sizeof(Position));
	position->type = POSITION_FROM_BOTTOM;
	position->value = offset;
	return position;
}

Position * FromTopPositionSemanticAction(const int value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Position * position = calloc(1, sizeof(Position));
	position->type = POSITION_FROM_TOP;
	position->value = value;
	return position;
}

Ranking * AverageRankingSemanticAction(const int seasons) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Ranking * ranking = calloc(1, sizeof(Ranking));
	ranking->type = RANKING_AVERAGE;
	ranking->seasons = seasons;
	return ranking;
}

Ranking * TableRankingSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Ranking * ranking = calloc(1, sizeof(Ranking));
	ranking->type = RANKING_TABLE;
	ranking->seasons = 1;
	return ranking;
}

Points * PointsSemanticAction(List * assignments) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Points * points = calloc(1, sizeof(Points));
	points->assignments = assignments;
	return points;
}

PointsAssignment * PointsAssignmentSemanticAction(const PointsOutcome outcome, const int amount) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	PointsAssignment * assignment = calloc(1, sizeof(PointsAssignment));
	assignment->outcome = outcome;
	assignment->points = amount;
	return assignment;
}

Tiebreak * TiebreakSemanticAction(List * criteria) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Tiebreak * tiebreak = calloc(1, sizeof(Tiebreak));
	tiebreak->criteria = criteria;
	return tiebreak;
}

Criterion * CriterionSemanticAction(const TiebreakCriterion value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Criterion * criterion = calloc(1, sizeof(Criterion));
	criterion->value = value;
	return criterion;
}

Results * ResultsSemanticAction(const bool hasSeason, const int season, List * resultList) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Results * results = calloc(1, sizeof(Results));
	results->hasSeason = hasSeason;
	results->season = season;
	results->results = resultList;
	return results;
}

Result * PenaltiesResultSemanticAction(char * home, const int homeGoals, const int awayGoals, char * away, const int homePenalties, const int awayPenalties) {
	Result * result = ResultSemanticAction(home, homeGoals, awayGoals, away);
	result->hasPenalties = true;
	result->homePenalties = homePenalties;
	result->awayPenalties = awayPenalties;
	return result;
}

Result * ResultSemanticAction(char * home, const int homeGoals, const int awayGoals, char * away) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Result * result = calloc(1, sizeof(Result));
	result->home = home;
	result->homeGoals = homeGoals;
	result->awayGoals = awayGoals;
	result->away = away;
	result->hasPenalties = false;
	return result;
}

/* Templates. */

Template * PointsTemplateSemanticAction(char * name, Points * points) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Template * templateDefinition = calloc(1, sizeof(Template));
	templateDefinition->name = name;
	templateDefinition->points = points;
	templateDefinition->type = TEMPLATE_POINTS;
	return templateDefinition;
}

Template * ZonesTemplateSemanticAction(char * name, List * zones) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Template * templateDefinition = calloc(1, sizeof(Template));
	templateDefinition->name = name;
	templateDefinition->zones = zones;
	templateDefinition->type = TEMPLATE_ZONES;
	return templateDefinition;
}

/* Federations. */

Federation * FederationSemanticAction(char * name, List * items) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Federation * federation = calloc(1, sizeof(Federation));
	federation->name = name;
	federation->items = items;
	return federation;
}

FederationItem * CalendarFederationItemSemanticAction(Calendar * calendar) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	FederationItem * item = calloc(1, sizeof(FederationItem));
	item->calendar = calendar;
	item->type = FEDERATION_ITEM_CALENDAR;
	return item;
}

FederationItem * CycleFederationItemSemanticAction(Cycle * cycle) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	FederationItem * item = calloc(1, sizeof(FederationItem));
	item->cycle = cycle;
	item->type = FEDERATION_ITEM_CYCLE;
	return item;
}

FederationItem * MovementFederationItemSemanticAction(Movement * movement) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	FederationItem * item = calloc(1, sizeof(FederationItem));
	item->movement = movement;
	item->type = FEDERATION_ITEM_MOVEMENT;
	return item;
}

FederationItem * TemplateFederationItemSemanticAction(Template * templateDefinition) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	FederationItem * item = calloc(1, sizeof(FederationItem));
	item->templateDefinition = templateDefinition;
	item->type = FEDERATION_ITEM_TEMPLATE;
	return item;
}

FederationItem * TournamentFederationItemSemanticAction(Tournament * tournament) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	FederationItem * item = calloc(1, sizeof(FederationItem));
	item->tournament = tournament;
	item->type = FEDERATION_ITEM_TOURNAMENT;
	return item;
}

Cycle * CycleSemanticAction(char * name, List * parameters, List * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Cycle * cycle = calloc(1, sizeof(Cycle));
	cycle->name = name;
	cycle->parameters = parameters;
	cycle->body = body;
	return cycle;
}

Movement * MovementSemanticAction(List * rules) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Movement * movement = calloc(1, sizeof(Movement));
	movement->rules = rules;
	return movement;
}

MovementRule * MovementRuleSemanticAction(const MovementDirection direction, MovementSubject * subject, char * fromLeague, char * toLeague) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	MovementRule * rule = calloc(1, sizeof(MovementRule));
	rule->direction = direction;
	rule->subject = subject;
	rule->fromLeague = fromLeague;
	rule->toLeague = toLeague;
	return rule;
}

MovementSubject * NamedSubjectSemanticAction(const MovementSubjectType type, char * name) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	MovementSubject * subject = calloc(1, sizeof(MovementSubject));
	subject->name = name;
	subject->type = type;
	return subject;
}

MovementSubject * SelectionSubjectSemanticAction(Selector * selector) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	MovementSubject * subject = calloc(1, sizeof(MovementSubject));
	subject->selector = selector;
	subject->type = SUBJECT_SELECTION;
	return subject;
}

Calendar * CalendarSemanticAction(const int fromYear, const int toYear, List * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Calendar * calendar = calloc(1, sizeof(Calendar));
	calendar->fromYear = fromYear;
	calendar->toYear = toYear;
	calendar->body = body;
	return calendar;
}

/* Calendar statements. */

Statement * CallStatementSemanticAction(char * cycle, List * arguments) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Call * call = calloc(1, sizeof(Call));
	call->cycle = cycle;
	call->arguments = arguments;
	Statement * statement = _statement(STATEMENT_CALL);
	statement->call = call;
	return statement;
}

Statement * EveryStatementSemanticAction(const int interval, YearExpression * start, char * variable, List * body, List * otherwise) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Every * every = calloc(1, sizeof(Every));
	every->interval = interval;
	every->start = start;
	every->variable = variable;
	every->body = body;
	every->otherwise = otherwise;
	Statement * statement = _statement(STATEMENT_EVERY);
	statement->every = every;
	return statement;
}

Statement * HostStatementSemanticAction(char * tournament, const HostTiming timing, YearExpression * year, const int offset) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Host * host = calloc(1, sizeof(Host));
	host->tournament = tournament;
	host->timing = timing;
	host->year = year;
	host->offset = offset;
	Statement * statement = _statement(STATEMENT_HOST);
	statement->host = host;
	return statement;
}

Statement * IfStatementSemanticAction(Condition * condition, List * thenBody, List * elseBody) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	If * conditional = calloc(1, sizeof(If));
	conditional->condition = condition;
	conditional->thenBody = thenBody;
	conditional->elseBody = elseBody;
	Statement * statement = _statement(STATEMENT_IF);
	statement->conditional = conditional;
	return statement;
}

Condition * ComparisonConditionSemanticAction(YearExpression * left, const ComparisonOperator comparator, YearExpression * right) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Condition * condition = calloc(1, sizeof(Condition));
	condition->leftYear = left;
	condition->rightYear = right;
	condition->comparator = comparator;
	condition->type = CONDITION_COMPARISON;
	return condition;
}

Condition * LogicalConditionSemanticAction(Condition * left, Condition * right, const ConditionType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Condition * condition = calloc(1, sizeof(Condition));
	condition->leftCondition = left;
	condition->rightCondition = right;
	condition->type = type;
	return condition;
}

Condition * NotConditionSemanticAction(Condition * operand) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Condition * condition = calloc(1, sizeof(Condition));
	condition->operand = operand;
	condition->type = CONDITION_NOT;
	return condition;
}

YearExpression * ArithmeticYearExpressionSemanticAction(YearExpression * left, YearExpression * right, const YearExpressionType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	YearExpression * expression = calloc(1, sizeof(YearExpression));
	expression->leftExpression = left;
	expression->rightExpression = right;
	expression->type = type;
	return expression;
}

YearExpression * LiteralYearExpressionSemanticAction(const int value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	YearExpression * expression = calloc(1, sizeof(YearExpression));
	expression->value = value;
	expression->type = YEAR_LITERAL;
	return expression;
}

YearExpression * VariableYearExpressionSemanticAction(char * variable) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	YearExpression * expression = calloc(1, sizeof(YearExpression));
	expression->variable = variable;
	expression->type = YEAR_VARIABLE;
	return expression;
}
