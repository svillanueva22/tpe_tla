#include "AbstractSyntaxTree.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownAbstractSyntaxTreeModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: AbstractSyntaxTree...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeAbstractSyntaxTreeModule() {
	_logger = createLogger("AbstractSyntaxTree");
	return _shutdownAbstractSyntaxTreeModule;
}

/* PRIVATE FUNCTIONS */

/**
 * Adapters from generic list elements (void *) to typed destructors.
 */

static void _destroyArgumentElement(void * element) { destroyYearExpression((YearExpression *) element); }
static void _destroyCriterionElement(void * element) { destroyCriterion((Criterion *) element); }
static void _destroyDeclarationElement(void * element) { destroyDeclaration((Declaration *) element); }
static void _destroyFederationItemElement(void * element) { destroyFederationItem((FederationItem *) element); }
static void _destroyMovementRuleElement(void * element) { destroyMovementRule((MovementRule *) element); }
static void _destroyPointsAssignmentElement(void * element) { destroyPointsAssignment((PointsAssignment *) element); }
static void _destroyResultElement(void * element) { destroyResult((Result *) element); }
static void _destroyStatementElement(void * element) { destroyStatement((Statement *) element); }
static void _destroyStringElement(void * element) { free(element); }
static void _destroyTournamentItemElement(void * element) { destroyTournamentItem((TournamentItem *) element); }
static void _destroyZoneElement(void * element) { destroyZone((Zone *) element); }

static void _logDestructor(const char * functionName) {
	logDebugging(_logger, "Executing destructor: %s", functionName);
}

/* PUBLIC FUNCTIONS: LISTS */

void destroyArgumentList(List * arguments) { destroyList(arguments, _destroyArgumentElement); }
void destroyCriterionList(List * criteria) { destroyList(criteria, _destroyCriterionElement); }
void destroyDeclarationList(List * declarations) { destroyList(declarations, _destroyDeclarationElement); }
void destroyFederationItemList(List * federationItems) { destroyList(federationItems, _destroyFederationItemElement); }
void destroyMovementRuleList(List * movementRules) { destroyList(movementRules, _destroyMovementRuleElement); }
void destroyPointsAssignmentList(List * pointsAssignments) { destroyList(pointsAssignments, _destroyPointsAssignmentElement); }
void destroyResultList(List * results) { destroyList(results, _destroyResultElement); }
void destroyStatementList(List * statements) { destroyList(statements, _destroyStatementElement); }
void destroyStringList(List * strings) { destroyList(strings, _destroyStringElement); }
void destroyTournamentItemList(List * tournamentItems) { destroyList(tournamentItems, _destroyTournamentItemElement); }
void destroyZoneList(List * zones) { destroyList(zones, _destroyZoneElement); }

/* PUBLIC FUNCTIONS: NODES */

void destroyCalendar(Calendar * calendar) {
	_logDestructor(__FUNCTION__);
	if (calendar != NULL) {
		destroyStatementList(calendar->body);
		free(calendar);
	}
}

void destroyCall(Call * call) {
	_logDestructor(__FUNCTION__);
	if (call != NULL) {
		free(call->cycle);
		destroyArgumentList(call->arguments);
		free(call);
	}
}

void destroyCondition(Condition * condition) {
	_logDestructor(__FUNCTION__);
	if (condition != NULL) {
		switch (condition->type) {
			case CONDITION_AND:
			case CONDITION_OR:
				destroyCondition(condition->leftCondition);
				destroyCondition(condition->rightCondition);
				break;
			case CONDITION_NOT:
				destroyCondition(condition->operand);
				break;
			case CONDITION_COMPARISON:
				destroyYearExpression(condition->leftYear);
				destroyYearExpression(condition->rightYear);
				break;
		}
		free(condition);
	}
}

void destroyCriterion(Criterion * criterion) {
	_logDestructor(__FUNCTION__);
	if (criterion != NULL) {
		free(criterion);
	}
}

void destroyCycle(Cycle * cycle) {
	_logDestructor(__FUNCTION__);
	if (cycle != NULL) {
		free(cycle->name);
		destroyStringList(cycle->parameters);
		destroyStatementList(cycle->body);
		free(cycle);
	}
}

void destroyDeclaration(Declaration * declaration) {
	_logDestructor(__FUNCTION__);
	if (declaration != NULL) {
		switch (declaration->type) {
			case DECLARATION_FEDERATION:
				destroyFederation(declaration->federation);
				break;
			case DECLARATION_TEMPLATE:
				destroyTemplate(declaration->templateDefinition);
				break;
			case DECLARATION_TOURNAMENT:
				destroyTournament(declaration->tournament);
				break;
		}
		free(declaration);
	}
}

void destroyEvery(Every * every) {
	_logDestructor(__FUNCTION__);
	if (every != NULL) {
		destroyYearExpression(every->start);
		free(every->variable);
		destroyStatementList(every->body);
		destroyStatementList(every->otherwise);
		free(every);
	}
}

void destroyFederation(Federation * federation) {
	_logDestructor(__FUNCTION__);
	if (federation != NULL) {
		free(federation->name);
		destroyFederationItemList(federation->items);
		free(federation);
	}
}

void destroyFederationItem(FederationItem * federationItem) {
	_logDestructor(__FUNCTION__);
	if (federationItem != NULL) {
		switch (federationItem->type) {
			case FEDERATION_ITEM_CALENDAR:
				destroyCalendar(federationItem->calendar);
				break;
			case FEDERATION_ITEM_CYCLE:
				destroyCycle(federationItem->cycle);
				break;
			case FEDERATION_ITEM_MOVEMENT:
				destroyMovement(federationItem->movement);
				break;
			case FEDERATION_ITEM_TEMPLATE:
				destroyTemplate(federationItem->templateDefinition);
				break;
			case FEDERATION_ITEM_TOURNAMENT:
				destroyTournament(federationItem->tournament);
				break;
		}
		free(federationItem);
	}
}

void destroyFormat(Format * format) {
	_logDestructor(__FUNCTION__);
	if (format != NULL) {
		free(format);
	}
}

void destroyHost(Host * host) {
	_logDestructor(__FUNCTION__);
	if (host != NULL) {
		free(host->tournament);
		destroyYearExpression(host->year);
		free(host);
	}
}

void destroyIf(If * conditional) {
	_logDestructor(__FUNCTION__);
	if (conditional != NULL) {
		destroyCondition(conditional->condition);
		destroyStatementList(conditional->thenBody);
		destroyStatementList(conditional->elseBody);
		free(conditional);
	}
}

void destroyMovement(Movement * movement) {
	_logDestructor(__FUNCTION__);
	if (movement != NULL) {
		destroyMovementRuleList(movement->rules);
		free(movement);
	}
}

void destroyMovementRule(MovementRule * movementRule) {
	_logDestructor(__FUNCTION__);
	if (movementRule != NULL) {
		destroyMovementSubject(movementRule->subject);
		free(movementRule->fromLeague);
		free(movementRule->toLeague);
		free(movementRule);
	}
}

void destroyMovementSubject(MovementSubject * movementSubject) {
	_logDestructor(__FUNCTION__);
	if (movementSubject != NULL) {
		switch (movementSubject->type) {
			case SUBJECT_SELECTION:
				destroySelector(movementSubject->selector);
				break;
			case SUBJECT_TEAM:
			case SUBJECT_WINNER:
			case SUBJECT_ZONE:
				free(movementSubject->name);
				break;
		}
		free(movementSubject);
	}
}

void destroyPeriod(Period * period) {
	_logDestructor(__FUNCTION__);
	if (period != NULL) {
		free(period);
	}
}

void destroyPoints(Points * points) {
	_logDestructor(__FUNCTION__);
	if (points != NULL) {
		destroyPointsAssignmentList(points->assignments);
		free(points);
	}
}

void destroyPointsAssignment(PointsAssignment * pointsAssignment) {
	_logDestructor(__FUNCTION__);
	if (pointsAssignment != NULL) {
		free(pointsAssignment);
	}
}

void destroyPosition(Position * position) {
	_logDestructor(__FUNCTION__);
	if (position != NULL) {
		free(position);
	}
}

void destroyProgram(Program * program) {
	_logDestructor(__FUNCTION__);
	if (program != NULL) {
		destroyDeclarationList(program->declarations);
		free(program);
	}
}

void destroyRanking(Ranking * ranking) {
	_logDestructor(__FUNCTION__);
	if (ranking != NULL) {
		free(ranking);
	}
}

void destroyResult(Result * result) {
	_logDestructor(__FUNCTION__);
	if (result != NULL) {
		free(result->home);
		free(result->away);
		free(result);
	}
}

void destroyResults(Results * results) {
	_logDestructor(__FUNCTION__);
	if (results != NULL) {
		destroyResultList(results->results);
		free(results);
	}
}

void destroySchedule(Schedule * schedule) {
	_logDestructor(__FUNCTION__);
	if (schedule != NULL) {
		destroyPeriod(schedule->period);
		free(schedule);
	}
}

void destroySelector(Selector * selector) {
	_logDestructor(__FUNCTION__);
	if (selector != NULL) {
		destroyPosition(selector->from);
		destroyPosition(selector->to);
		destroyRanking(selector->ranking);
		free(selector);
	}
}

void destroyStatement(Statement * statement) {
	_logDestructor(__FUNCTION__);
	if (statement != NULL) {
		switch (statement->type) {
			case STATEMENT_CALL:
				destroyCall(statement->call);
				break;
			case STATEMENT_EVERY:
				destroyEvery(statement->every);
				break;
			case STATEMENT_HOST:
				destroyHost(statement->host);
				break;
			case STATEMENT_IF:
				destroyIf(statement->conditional);
				break;
		}
		free(statement);
	}
}

void destroyTeams(Teams * teams) {
	_logDestructor(__FUNCTION__);
	if (teams != NULL) {
		switch (teams->type) {
			case TEAMS_FROM_TOURNAMENT:
				free(teams->sourceTournament);
				destroySelector(teams->selector);
				break;
			case TEAMS_LIST:
				destroyStringList(teams->names);
				break;
		}
		free(teams);
	}
}

void destroyTemplate(Template * templateDefinition) {
	_logDestructor(__FUNCTION__);
	if (templateDefinition != NULL) {
		free(templateDefinition->name);
		switch (templateDefinition->type) {
			case TEMPLATE_POINTS:
				destroyPoints(templateDefinition->points);
				break;
			case TEMPLATE_ZONES:
				destroyZoneList(templateDefinition->zones);
				break;
		}
		free(templateDefinition);
	}
}

void destroyTiebreak(Tiebreak * tiebreak) {
	_logDestructor(__FUNCTION__);
	if (tiebreak != NULL) {
		destroyCriterionList(tiebreak->criteria);
		free(tiebreak);
	}
}

void destroyTournament(Tournament * tournament) {
	_logDestructor(__FUNCTION__);
	if (tournament != NULL) {
		free(tournament->name);
		destroyTournamentItemList(tournament->items);
		free(tournament);
	}
}

void destroyTournamentItem(TournamentItem * tournamentItem) {
	_logDestructor(__FUNCTION__);
	if (tournamentItem != NULL) {
		switch (tournamentItem->type) {
			case ITEM_FORMAT:
				destroyFormat(tournamentItem->format);
				break;
			case ITEM_POINTS:
				destroyPoints(tournamentItem->points);
				break;
			case ITEM_RESULTS:
				destroyResults(tournamentItem->results);
				break;
			case ITEM_SCHEDULE:
				destroySchedule(tournamentItem->schedule);
				break;
			case ITEM_TEAMS:
				destroyTeams(tournamentItem->teams);
				break;
			case ITEM_TIEBREAK:
				destroyTiebreak(tournamentItem->tiebreak);
				break;
			case ITEM_USE:
				destroyUse(tournamentItem->use);
				break;
			case ITEM_ZONE:
				destroyZone(tournamentItem->zone);
				break;
		}
		free(tournamentItem);
	}
}

void destroyUse(Use * use) {
	_logDestructor(__FUNCTION__);
	if (use != NULL) {
		free(use->name);
		free(use);
	}
}

void destroyYearExpression(YearExpression * yearExpression) {
	_logDestructor(__FUNCTION__);
	if (yearExpression != NULL) {
		switch (yearExpression->type) {
			case YEAR_ADDITION:
			case YEAR_SUBTRACTION:
				destroyYearExpression(yearExpression->leftExpression);
				destroyYearExpression(yearExpression->rightExpression);
				break;
			case YEAR_LITERAL:
				break;
			case YEAR_VARIABLE:
				free(yearExpression->variable);
				break;
		}
		free(yearExpression);
	}
}

void destroyZone(Zone * zone) {
	_logDestructor(__FUNCTION__);
	if (zone != NULL) {
		free(zone->name);
		free(zone->cup);
		destroySelector(zone->selector);
		free(zone);
	}
}
