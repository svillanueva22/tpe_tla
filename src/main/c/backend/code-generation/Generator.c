#include "Generator.h"

/* MODULE INTERNAL STATE */

static const char _indentationCharacter = ' ';
static const char _indentationSize = 2;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownGeneratorModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: Generator...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeGeneratorModule() {
	_logger = createLogger("Generator");
	return _shutdownGeneratorModule;
}

/**
 * PRIVATE FUNCTIONS
 *
 * During the Stage II (frontend), the generator only prints the AST in a
 * human-readable way, to verify that it's built as expected. The Stage III
 * (backend), will replace this with the generation of the fixtures.
 */

static char * _indentation(const unsigned int level);
static void _output(const unsigned int indentationLevel, const char * const format, ...);
static void _printCondition(Condition * condition);
static void _printDate(const Date date);
static void _printDeclaration(const unsigned int level, Declaration * declaration);
static void _printFederation(const unsigned int level, Federation * federation);
static void _printFormat(Format * format);
static void _printMovementRule(const unsigned int level, MovementRule * rule);
static void _printPeriod(Period * period);
static void _printPoints(Points * points);
static void _printPosition(Position * position);
static void _printSelector(Selector * selector);
static void _printStatement(const unsigned int level, Statement * statement);
static void _printStatements(const unsigned int level, List * statements);
static void _printTemplate(const unsigned int level, Template * templateDefinition);
static void _printTournament(const unsigned int level, Tournament * tournament);
static void _printTournamentItem(const unsigned int level, TournamentItem * item);
static void _printYearExpression(YearExpression * expression);
static void _printZone(const unsigned int level, Zone * zone);
static void _raw(const char * const format, ...);

static const char * _comparatorToString(const ComparisonOperator comparator) {
	switch (comparator) {
		case COMPARISON_EQUAL: return "==";
		case COMPARISON_GREATER_THAN: return ">";
		case COMPARISON_GREATER_THAN_OR_EQUAL: return ">=";
		case COMPARISON_LESS_THAN: return "<";
		case COMPARISON_LESS_THAN_OR_EQUAL: return "<=";
		case COMPARISON_NOT_EQUAL: return "!=";
		default: return "?";
	}
}

static const char * _criterionToString(const TiebreakCriterion criterion) {
	switch (criterion) {
		case CRITERION_AWAY_GOALS: return "away goals";
		case CRITERION_GOAL_DIFFERENCE: return "goal difference";
		case CRITERION_GOALS_AGAINST: return "goals against";
		case CRITERION_GOALS_FOR: return "goals for";
		case CRITERION_HEAD_TO_HEAD: return "head to head";
		case CRITERION_WINS: return "wins";
		default: return "?";
	}
}

static const char * _legsToString(const Legs legs) {
	return legs == LEGS_DOUBLE ? "double (home and away)" : "single";
}

static const char * _outcomeToString(const PointsOutcome outcome) {
	switch (outcome) {
		case OUTCOME_DRAW: return "draw";
		case OUTCOME_LOSS: return "loss";
		case OUTCOME_WIN: return "win";
		default: return "?";
	}
}

static const char * _weekdayToString(const int weekday) {
	static const char * names[] = {"?", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
	return (MONDAY <= weekday && weekday <= SUNDAY) ? names[weekday] : names[0];
}

static const char * _zoneKindToString(const ZoneKind kind) {
	switch (kind) {
		case ZONE_CHAMPION: return "champion";
		case ZONE_PROMOTE: return "promote";
		case ZONE_QUALIFY: return "qualify";
		case ZONE_RELEGATE: return "relegate";
		default: return "?";
	}
}

static void _printCondition(Condition * condition) {
	switch (condition->type) {
		case CONDITION_AND:
		case CONDITION_OR:
			_raw("(");
			_printCondition(condition->leftCondition);
			_raw(condition->type == CONDITION_AND ? " and " : " or ");
			_printCondition(condition->rightCondition);
			_raw(")");
			break;
		case CONDITION_NOT:
			_raw("not ");
			_printCondition(condition->operand);
			break;
		case CONDITION_COMPARISON:
			_raw("(");
			_printYearExpression(condition->leftYear);
			_raw(" %s ", _comparatorToString(condition->comparator));
			_printYearExpression(condition->rightYear);
			_raw(")");
			break;
	}
}

static void _printDate(const Date date) {
	if (date.year == 0) {
		_raw("--%02d-%02d (yearless)", date.month, date.day);
	}
	else {
		_raw("%04d-%02d-%02d", date.year, date.month, date.day);
	}
}

static void _printDeclaration(const unsigned int level, Declaration * declaration) {
	switch (declaration->type) {
		case DECLARATION_FEDERATION:
			_printFederation(level, declaration->federation);
			break;
		case DECLARATION_TEMPLATE:
			_printTemplate(level, declaration->templateDefinition);
			break;
		case DECLARATION_TOURNAMENT:
			_printTournament(level, declaration->tournament);
			break;
	}
}

static void _printFederation(const unsigned int level, Federation * federation) {
	_output(level, "Federation \"%s\"\n", federation->name);
	for (ListNode * node = federation->items->first; node != NULL; node = node->next) {
		FederationItem * item = node->element;
		switch (item->type) {
			case FEDERATION_ITEM_CALENDAR:
				_output(1 + level, "Calendar from %d to %d\n", item->calendar->fromYear, item->calendar->toYear);
				_printStatements(2 + level, item->calendar->body);
				break;
			case FEDERATION_ITEM_CYCLE:
				_output(1 + level, "Cycle \"%s\" (", item->cycle->name);
				for (ListNode * parameter = item->cycle->parameters->first; parameter != NULL; parameter = parameter->next) {
					_raw("%s%s", (char *) parameter->element, parameter->next == NULL ? "" : ", ");
				}
				_raw(")\n");
				_printStatements(2 + level, item->cycle->body);
				break;
			case FEDERATION_ITEM_MOVEMENT:
				_output(1 + level, "Movement\n");
				for (ListNode * rule = item->movement->rules->first; rule != NULL; rule = rule->next) {
					_printMovementRule(2 + level, rule->element);
				}
				break;
			case FEDERATION_ITEM_TEMPLATE:
				_printTemplate(1 + level, item->templateDefinition);
				break;
			case FEDERATION_ITEM_TOURNAMENT:
				_printTournament(1 + level, item->tournament);
				break;
		}
	}
}

static void _printFormat(Format * format) {
	switch (format->type) {
		case FORMAT_GROUPS:
			_raw("groups of %d, top %d advance, then knockout %s", format->groupSize, format->advancing, _legsToString(format->legs));
			break;
		case FORMAT_KNOCKOUT:
			_raw("knockout %s", _legsToString(format->legs));
			break;
		case FORMAT_ROUND_ROBIN:
			_raw("round robin %s", _legsToString(format->legs));
			break;
	}
}

static void _printMovementRule(const unsigned int level, MovementRule * rule) {
	_output(level, "%s ", rule->direction == MOVEMENT_PROMOTE ? "Promote" : "Relegate");
	switch (rule->subject->type) {
		case SUBJECT_SELECTION:
			_printSelector(rule->subject->selector);
			break;
		case SUBJECT_TEAM:
			_raw("team \"%s\"", rule->subject->name);
			break;
		case SUBJECT_WINNER:
			_raw("winner of \"%s\"", rule->subject->name);
			break;
		case SUBJECT_ZONE:
			_raw("zone \"%s\"", rule->subject->name);
			break;
	}
	_raw(" from \"%s\" to \"%s\"\n", rule->fromLeague, rule->toLeague);
}

static void _printPeriod(Period * period) {
	switch (period->type) {
		case PERIOD_DAYS:
			_raw("every %d day(s)", period->amount);
			break;
		case PERIOD_WEEKDAYS:
			_raw("every");
			for (int weekday = MONDAY; weekday <= SUNDAY; ++weekday) {
				if (period->weekdays & (1u << weekday)) {
					_raw(" %s", _weekdayToString(weekday));
				}
			}
			break;
		case PERIOD_WEEKS:
			_raw("every %d week(s)", period->amount);
			break;
	}
}

static void _printPoints(Points * points) {
	for (ListNode * node = points->assignments->first; node != NULL; node = node->next) {
		PointsAssignment * assignment = node->element;
		_raw("%s = %d%s", _outcomeToString(assignment->outcome), assignment->points, node->next == NULL ? "" : ", ");
	}
}

static void _printPosition(Position * position) {
	if (position->type == POSITION_FROM_TOP) {
		_raw("%d", position->value);
	}
	else if (position->value == 0) {
		_raw("last");
	}
	else {
		_raw("last - %d", position->value);
	}
}

static void _printSelector(Selector * selector) {
	switch (selector->type) {
		case SELECTOR_BOTTOM:
			_raw("bottom %d", selector->count);
			break;
		case SELECTOR_RANGE:
			_raw("positions ");
			_printPosition(selector->from);
			_raw(" to ");
			_printPosition(selector->to);
			break;
		case SELECTOR_TOP:
			_raw("top %d", selector->count);
			break;
	}
	if (selector->ranking->type == RANKING_AVERAGE) {
		_raw(" by average of last %d season(s)", selector->ranking->seasons);
	}
	else {
		_raw(" by table");
	}
}

static void _printStatement(const unsigned int level, Statement * statement) {
	switch (statement->type) {
		case STATEMENT_CALL:
			_output(level, "Call \"%s\" (", statement->call->cycle);
			for (ListNode * node = statement->call->arguments->first; node != NULL; node = node->next) {
				_printYearExpression(node->element);
				_raw("%s", node->next == NULL ? "" : ", ");
			}
			_raw(")\n");
			break;
		case STATEMENT_EVERY:
			_output(level, "Every %d year(s)", statement->every->interval);
			if (statement->every->start != NULL) {
				_raw(" starting ");
				_printYearExpression(statement->every->start);
			}
			if (statement->every->variable != NULL) {
				_raw(" as %s", statement->every->variable);
			}
			_raw("\n");
			_printStatements(1 + level, statement->every->body);
			if (statement->every->otherwise != NULL) {
				_output(level, "Otherwise\n");
				_printStatements(1 + level, statement->every->otherwise);
			}
			break;
		case STATEMENT_HOST:
			_output(level, "Host \"%s\" ", statement->host->tournament);
			switch (statement->host->timing) {
				case HOST_CURRENT_YEAR:
					_raw("in the current year");
					break;
				case HOST_EXPLICIT_YEAR:
					_raw("in ");
					_printYearExpression(statement->host->year);
					break;
				case HOST_YEARS_AFTER:
					_raw("%d year(s) after", statement->host->offset);
					break;
				case HOST_YEARS_BEFORE:
					_raw("%d year(s) before", statement->host->offset);
					break;
			}
			_raw("\n");
			break;
		case STATEMENT_IF:
			_output(level, "If ");
			_printCondition(statement->conditional->condition);
			_raw("\n");
			_printStatements(1 + level, statement->conditional->thenBody);
			if (statement->conditional->elseBody != NULL) {
				_output(level, "Else\n");
				_printStatements(1 + level, statement->conditional->elseBody);
			}
			break;
	}
}

static void _printStatements(const unsigned int level, List * statements) {
	for (ListNode * node = statements->first; node != NULL; node = node->next) {
		_printStatement(level, node->element);
	}
}

static void _printTemplate(const unsigned int level, Template * templateDefinition) {
	switch (templateDefinition->type) {
		case TEMPLATE_POINTS:
			_output(level, "Template points \"%s\": ", templateDefinition->name);
			_printPoints(templateDefinition->points);
			_raw("\n");
			break;
		case TEMPLATE_ZONES:
			_output(level, "Template zones \"%s\"\n", templateDefinition->name);
			for (ListNode * node = templateDefinition->zones->first; node != NULL; node = node->next) {
				_printZone(1 + level, node->element);
			}
			break;
	}
}

static void _printTournament(const unsigned int level, Tournament * tournament) {
	if (tournament->type == KIND_LEAGUE) {
		_output(level, "League \"%s\" (tier %d)\n", tournament->name, tournament->tier);
	}
	else {
		_output(level, "Tournament \"%s\"\n", tournament->name);
	}
	for (ListNode * node = tournament->items->first; node != NULL; node = node->next) {
		_printTournamentItem(1 + level, node->element);
	}
}

static void _printTournamentItem(const unsigned int level, TournamentItem * item) {
	switch (item->type) {
		case ITEM_FORMAT:
			_output(level, "Format: ");
			_printFormat(item->format);
			_raw("\n");
			break;
		case ITEM_POINTS:
			_output(level, "Points: ");
			_printPoints(item->points);
			_raw("\n");
			break;
		case ITEM_RESULTS:
			if (item->results->hasSeason) {
				_output(level, "Results of season %d\n", item->results->season);
			}
			else {
				_output(level, "Results\n");
			}
			for (ListNode * node = item->results->results->first; node != NULL; node = node->next) {
				Result * result = node->element;
				_output(1 + level, "\"%s\" %d-%d \"%s\"", result->home, result->homeGoals, result->awayGoals, result->away);
				if (result->hasPenalties) {
					_raw(" (penalties %d-%d)", result->homePenalties, result->awayPenalties);
				}
				_raw("\n");
			}
			break;
		case ITEM_SCHEDULE:
			_output(level, "Schedule: starting ");
			_printDate(item->schedule->start);
			_raw(", ");
			_printPeriod(item->schedule->period);
			_raw("\n");
			break;
		case ITEM_TEAMS:
			if (item->teams->type == TEAMS_LIST) {
				_output(level, "Teams (%u): ", item->teams->names->size);
				for (ListNode * node = item->teams->names->first; node != NULL; node = node->next) {
					_raw("\"%s\"%s", (char *) node->element, node->next == NULL ? "" : ", ");
				}
			}
			else {
				_output(level, "Teams: from \"%s\", ", item->teams->sourceTournament);
				_printSelector(item->teams->selector);
			}
			_raw("\n");
			break;
		case ITEM_TIEBREAK:
			_output(level, "Tiebreak: ");
			for (ListNode * node = item->tiebreak->criteria->first; node != NULL; node = node->next) {
				Criterion * criterion = node->element;
				_raw("%s%s", _criterionToString(criterion->value), node->next == NULL ? "" : ", ");
			}
			_raw("\n");
			break;
		case ITEM_USE:
			_output(level, "Use %s \"%s\"\n", item->use->type == TEMPLATE_POINTS ? "points" : "zones", item->use->name);
			break;
		case ITEM_ZONE:
			_printZone(level, item->zone);
			break;
	}
}

static void _printYearExpression(YearExpression * expression) {
	switch (expression->type) {
		case YEAR_ADDITION:
		case YEAR_SUBTRACTION:
			_raw("(");
			_printYearExpression(expression->leftExpression);
			_raw(expression->type == YEAR_ADDITION ? " + " : " - ");
			_printYearExpression(expression->rightExpression);
			_raw(")");
			break;
		case YEAR_LITERAL:
			_raw("%d", expression->value);
			break;
		case YEAR_VARIABLE:
			_raw("%s", expression->variable);
			break;
	}
}

static void _printZone(const unsigned int level, Zone * zone) {
	_output(level, "Zone \"%s\": %s", zone->name, _zoneKindToString(zone->kind));
	if (zone->kind == ZONE_QUALIFY) {
		_raw(" to \"%s\"", zone->cup);
	}
	_raw(", ");
	_printSelector(zone->selector);
	_raw("\n");
}

/**
 * Generates an indentation string for the specified level.
 */
static char * _indentation(const unsigned int level) {
	return indentation(_indentationCharacter, level, _indentationSize);
}

/**
 * Outputs a formatted string to standard output, with indentation. The
 * "fflush" instruction allows to see the output even close to a failure,
 * because it drops the buffering.
 */
static void _output(const unsigned int indentationLevel, const char * const format, ...) {
	va_list arguments;
	va_start(arguments, format);
	char * indentation = _indentation(indentationLevel);
	char * effectiveFormat = concatenate(2, indentation, format);
	vfprintf(stdout, effectiveFormat, arguments);
	fflush(stdout);
	free(effectiveFormat);
	free(indentation);
	va_end(arguments);
}

/**
 * Outputs a formatted string to standard output, without indentation.
 */
static void _raw(const char * const format, ...) {
	va_list arguments;
	va_start(arguments, format);
	vfprintf(stdout, format, arguments);
	fflush(stdout);
	va_end(arguments);
}

/** PUBLIC FUNCTIONS */

void executeGenerator(CompilerState * compilerState) {
	logDebugging(_logger, "Printing the abstract syntax tree...");
	Program * program = compilerState->abstractSyntaxtTree;
	_output(0, "Program\n");
	for (ListNode * node = program->declarations->first; node != NULL; node = node->next) {
		_printDeclaration(1, node->element);
	}
	logDebugging(_logger, "Generation is done.");
}
