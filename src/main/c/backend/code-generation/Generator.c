#include "Generator.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;
static Program * _program = NULL;

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
 * PRIVATE TYPES
 *
 * The generator emits a single, self-contained HTML document (to standard
 * output), with the fixture of every tournament: the matchdays with their
 * dates, the brackets of the knockouts, and the standings (computed from the
 * declared results, if any). Federations are expanded year by year, according
 * to their calendar.
 */

/** Maximum depth of nested cycle calls (guards against infinite recursion). */
#define MAX_CALL_DEPTH 32

/** Maximum amount of year variables in scope at the same time. */
#define MAX_BINDINGS 64

/** Maximum depth of chained "teams from" references. */
#define MAX_SOURCE_DEPTH 16

/** A pairing of a round-robin. A team index of -1 means a bye. */
typedef struct {
	int home;
	int away;
} Pairing;

/** Iterates over the dates of the matchdays of a schedule. */
typedef struct {
	Date current;
	Period * period;
	bool known;
	bool started;
} Matchdays;

/** Everything needed to generate a tournament, resolved from its items. */
typedef struct {
	char ** teams;						// Heap-allocated names (placeholders included).
	int count;
	Format * format;
	Schedule * schedule;
	Points * points;
	Tiebreak * tiebreak;
	List * zones;						// Zone * (not owned)
	List * results;						// Result * (not owned)
} TournamentAttributes;

/** A row of the standings. */
typedef struct {
	int team;							// Index in the teams array.
	int played;
	int won;
	int drawn;
	int lost;
	int goalsFor;
	int goalsAgainst;
	int awayGoals;
	int points;
} Row;

/** Context needed to rank the rows of the standings. */
typedef struct {
	char ** teams;
	int count;
	List * results;
	Points * points;
	Tiebreak * tiebreak;
} Standings;

/** A year variable in scope, during the evaluation of a calendar. */
typedef struct {
	const char * name;
	int value;
} Binding;

/** A tournament hosted in some year of a calendar. */
typedef struct {
	int year;
	Tournament * tournament;
} Event;

/** The state of the evaluation of a calendar. */
typedef struct {
	Federation * federation;
	Calendar * calendar;
	Binding bindings[MAX_BINDINGS];
	int bindingCount;
	int callDepth;
	Event * events;
	int eventCount;
	int eventCapacity;
} CalendarRun;

/**
 * PRIVATE FUNCTIONS
 */

static void _escape(const char * text);
static void _genDeclaration(Declaration * declaration);
static void _genFederation(Federation * federation);
static void _genTournament(Tournament * tournament, Federation * federation, const int year);
static void _output(const char * const format, ...);
static int _teamCount(Tournament * tournament, Federation * federation, const unsigned int depth);

/* -------------------------------------------------------------------------- */
/* Output.                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * Outputs a string to standard output, escaping the HTML special characters.
 */
static void _escape(const char * text) {
	if (text == NULL) {
		return;
	}
	for (; *text != '\0'; ++text) {
		switch (*text) {
			case '&': fputs("&amp;", stdout); break;
			case '<': fputs("&lt;", stdout); break;
			case '>': fputs("&gt;", stdout); break;
			case '"': fputs("&quot;", stdout); break;
			case '\'': fputs("&#39;", stdout); break;
			default: fputc(*text, stdout); break;
		}
	}
}

/**
 * Outputs a formatted string to standard output. It supports a small subset
 * of the "printf" conversions: "%d" (with optional width and flags), "%s"
 * (an HTML-escaped string), "%r" (a raw string, for markup) and "%%". The
 * "fflush" instruction allows to see the output even close to a failure,
 * because it drops the buffering.
 */
static void _output(const char * const format, ...) {
	va_list arguments;
	va_start(arguments, format);
	for (const char * character = format; *character != '\0'; ++character) {
		if (*character != '%') {
			fputc(*character, stdout);
			continue;
		}
		char specification[16] = "%";
		unsigned int length = 1;
		++character;
		while (*character != '\0' && strchr("0123456789-", *character) != NULL && length < sizeof(specification) - 2) {
			specification[length++] = *character++;
		}
		if (*character == '\0') {
			break;
		}
		switch (*character) {
			case 'd':
				specification[length++] = 'd';
				specification[length] = '\0';
				fprintf(stdout, specification, va_arg(arguments, int));
				break;
			case 's':
				_escape(va_arg(arguments, const char *));
				break;
			case 'r':
				fputs(va_arg(arguments, const char *), stdout);
				break;
			default:
				fputc(*character, stdout);
				break;
		}
	}
	fflush(stdout);
	va_end(arguments);
}

/**
 * Creates a new formatted string, using heap-memory.
 */
static char * _newString(const char * const format, ...) {
	va_list arguments;
	va_start(arguments, format);
	const int length = vsnprintf(NULL, 0, format, arguments);
	va_end(arguments);
	char * string = calloc(length + 1, sizeof(char));
	va_start(arguments, format);
	vsnprintf(string, length + 1, format, arguments);
	va_end(arguments);
	return string;
}

/* -------------------------------------------------------------------------- */
/* Dates.                                                                     */
/* -------------------------------------------------------------------------- */

static const char * _monthNames[] = {"?", "enero", "febrero", "marzo", "abril", "mayo", "junio", "julio",
	"agosto", "septiembre", "octubre", "noviembre", "diciembre"};

static const char * _weekdayNames[] = {"?", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado", "domingo"};

static bool _isLeapYear(const int year) {
	return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int _daysInMonth(const int year, const int month) {
	static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	return (month == 2 && _isLeapYear(year)) ? 29 : days[month];
}

/** Advances a date the specified amount of days. */
static Date _addDays(Date date, int days) {
	for (; 0 < days; --days) {
		if (_daysInMonth(date.year, date.month) < ++date.day) {
			date.day = 1;
			if (12 < ++date.month) {
				date.month = 1;
				++date.year;
			}
		}
	}
	return date;
}

/** The ISO 8601 day of the week (Monday is 1, Sunday is 7), by Sakamoto's method. */
static int _dayOfWeek(const Date date) {
	static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
	const int year = date.year - (date.month < 3);
	const int weekday = (year + year / 4 - year / 100 + year / 400 + offsets[date.month - 1] + date.day) % 7;
	return weekday == 0 ? SUNDAY : weekday;
}

/**
 * Injects the year into a yearless date. A 29th of February on a non-leap
 * year moves to the 1st of March.
 */
static Date _resolveDate(Date date, const int year) {
	if (date.year == 0) {
		date.year = year;
	}
	if (date.year != 0 && _daysInMonth(date.year, date.month) < date.day) {
		date.day = 1;
		++date.month;
	}
	return date;
}

static bool _isEnabledWeekday(const unsigned int weekdays, const Date date) {
	return (weekdays & (1u << _dayOfWeek(date))) != 0;
}

static Matchdays _startMatchdays(Schedule * schedule, const int year) {
	Matchdays matchdays = {
		.current = {0, 0, 0},
		.known = false,
		.period = NULL,
		.started = false
	};
	if (schedule != NULL) {
		matchdays.current = _resolveDate(schedule->start, year);
		matchdays.period = schedule->period;
		matchdays.known = matchdays.current.year != 0;
	}
	return matchdays;
}

/** Returns the date of the next matchday. Only valid if the dates are known. */
static Date _nextMatchday(Matchdays * matchdays) {
	Period * period = matchdays->period;
	const bool hasWeekdays = period->type == PERIOD_WEEKDAYS && (period->weekdays & 0xFEu) != 0;
	if (!matchdays->started) {
		matchdays->started = true;
		while (hasWeekdays && !_isEnabledWeekday(period->weekdays, matchdays->current)) {
			matchdays->current = _addDays(matchdays->current, 1);
		}
		return matchdays->current;
	}
	switch (period->type) {
		case PERIOD_DAYS:
			matchdays->current = _addDays(matchdays->current, period->amount);
			break;
		case PERIOD_WEEKS:
			matchdays->current = _addDays(matchdays->current, 7 * period->amount);
			break;
		case PERIOD_WEEKDAYS:
			if (!hasWeekdays) {
				matchdays->current = _addDays(matchdays->current, 7);
				break;
			}
			do {
				matchdays->current = _addDays(matchdays->current, 1);
			} while (!_isEnabledWeekday(period->weekdays, matchdays->current));
			break;
	}
	return matchdays->current;
}

static void _genDate(const Date date) {
	_output("%r %d de %r de %d", _weekdayNames[_dayOfWeek(date)], date.day, _monthNames[date.month], date.year);
}

/** Outputs the date of the next matchday, if the schedule allows to know it. */
static void _genNextMatchday(Matchdays * matchdays) {
	if (matchdays->known) {
		_output("<span class=\"date\">");
		_genDate(_nextMatchday(matchdays));
		_output("</span>");
	}
}

/* -------------------------------------------------------------------------- */
/* Lookups.                                                                   */
/* -------------------------------------------------------------------------- */

/** Finds a tournament or league, first in the federation and then globally. */
static Tournament * _findTournament(const char * name, Federation * federation) {
	if (federation != NULL) {
		for (ListNode * node = federation->items->first; node != NULL; node = node->next) {
			FederationItem * item = node->element;
			if (item->type == FEDERATION_ITEM_TOURNAMENT && strcmp(item->tournament->name, name) == 0) {
				return item->tournament;
			}
		}
	}
	for (ListNode * node = _program->declarations->first; node != NULL; node = node->next) {
		Declaration * declaration = node->element;
		if (declaration->type == DECLARATION_TOURNAMENT && strcmp(declaration->tournament->name, name) == 0) {
			return declaration->tournament;
		}
	}
	return NULL;
}

/** Finds a template, first in the federation and then globally. */
static Template * _findTemplate(const char * name, const TemplateType type, Federation * federation) {
	if (federation != NULL) {
		for (ListNode * node = federation->items->first; node != NULL; node = node->next) {
			FederationItem * item = node->element;
			if (item->type == FEDERATION_ITEM_TEMPLATE && item->templateDefinition->type == type
					&& strcmp(item->templateDefinition->name, name) == 0) {
				return item->templateDefinition;
			}
		}
	}
	for (ListNode * node = _program->declarations->first; node != NULL; node = node->next) {
		Declaration * declaration = node->element;
		if (declaration->type == DECLARATION_TEMPLATE && declaration->templateDefinition->type == type
				&& strcmp(declaration->templateDefinition->name, name) == 0) {
			return declaration->templateDefinition;
		}
	}
	return NULL;
}

static Cycle * _findCycle(const char * name, Federation * federation) {
	for (ListNode * node = federation->items->first; node != NULL; node = node->next) {
		FederationItem * item = node->element;
		if (item->type == FEDERATION_ITEM_CYCLE && strcmp(item->cycle->name, name) == 0) {
			return item->cycle;
		}
	}
	return NULL;
}

static int _teamIndex(char ** teams, const int count, const char * name) {
	for (int k = 0; k < count; ++k) {
		if (strcmp(teams[k], name) == 0) {
			return k;
		}
	}
	return -1;
}

/* -------------------------------------------------------------------------- */
/* Selectors and zones.                                                       */
/* -------------------------------------------------------------------------- */

static int _clamp(const int value, const int minimum, const int maximum) {
	return value < minimum ? minimum : (maximum < value ? maximum : value);
}

static int _positionValue(Position * position, const int count) {
	return position->type == POSITION_FROM_TOP ? position->value : count - position->value;
}

/** Converts a selector into a 1-based range of positions, over N teams. */
static void _selectorRange(Selector * selector, const int count, int * from, int * to) {
	switch (selector->type) {
		case SELECTOR_BOTTOM:
			*from = count - selector->count + 1;
			*to = count;
			break;
		case SELECTOR_RANGE:
			*from = _positionValue(selector->from, count);
			*to = _positionValue(selector->to, count);
			break;
		case SELECTOR_TOP:
			*from = 1;
			*to = selector->count;
			break;
	}
	*from = _clamp(*from, 1, count);
	*to = _clamp(*to, 1, count);
}

static const char * _zoneClass(const ZoneKind kind) {
	switch (kind) {
		case ZONE_CHAMPION: return "zone-champion";
		case ZONE_PROMOTE: return "zone-promote";
		case ZONE_QUALIFY: return "zone-qualify";
		case ZONE_RELEGATE: return "zone-relegate";
		default: return "";
	}
}

/** The first zone that covers the position, or NULL. */
static Zone * _zoneAt(List * zones, const int position, const int count) {
	if (zones == NULL) {
		return NULL;
	}
	for (ListNode * node = zones->first; node != NULL; node = node->next) {
		Zone * zone = node->element;
		int from, to;
		_selectorRange(zone->selector, count, &from, &to);
		if (from <= position && position <= to) {
			return zone;
		}
	}
	return NULL;
}

static void _genPosition(Position * position) {
	if (position->type == POSITION_FROM_TOP) {
		_output("%d°", position->value);
	}
	else if (position->value == 0) {
		_output("último");
	}
	else {
		_output("último − %d", position->value);
	}
}

static void _genSelector(Selector * selector) {
	switch (selector->type) {
		case SELECTOR_BOTTOM:
			if (selector->count == 1) _output("último");
			else _output("últimos %d", selector->count);
			break;
		case SELECTOR_RANGE:
			_genPosition(selector->from);
			if (selector->from->type != selector->to->type || selector->from->value != selector->to->value) {
				_output(" a ");
				_genPosition(selector->to);
			}
			break;
		case SELECTOR_TOP:
			if (selector->count == 1) _output("primero");
			else _output("primeros %d", selector->count);
			break;
	}
	if (selector->ranking->type == RANKING_AVERAGE) {
		_output(" (por promedio de las últimas %d temporadas)", selector->ranking->seasons);
	}
}

static void _genZoneLegend(List * zones) {
	if (zones == NULL || zones->size == 0) {
		return;
	}
	_output("<ul class=\"legend\">");
	for (ListNode * node = zones->first; node != NULL; node = node->next) {
		Zone * zone = node->element;
		_output("<li><span class=\"swatch %r\"></span><strong>%s</strong>: ", _zoneClass(zone->kind), zone->name);
		switch (zone->kind) {
			case ZONE_CHAMPION: _output("campeón"); break;
			case ZONE_PROMOTE: _output("ascenso"); break;
			case ZONE_QUALIFY: _output("clasifica a %s", zone->cup); break;
			case ZONE_RELEGATE: _output("descenso"); break;
		}
		_output(" — ");
		_genSelector(zone->selector);
		_output("</li>");
	}
	_output("</ul>");
}

/* -------------------------------------------------------------------------- */
/* Tournament attributes.                                                     */
/* -------------------------------------------------------------------------- */

/** The amount of teams of a tournament (following "teams from" references). */
static int _teamCount(Tournament * tournament, Federation * federation, const unsigned int depth) {
	for (ListNode * node = tournament->items->first; node != NULL; node = node->next) {
		TournamentItem * item = node->element;
		if (item->type != ITEM_TEAMS) {
			continue;
		}
		if (item->teams->type == TEAMS_LIST) {
			return item->teams->names->size;
		}
		Tournament * source = _findTournament(item->teams->sourceTournament, federation);
		if (source == NULL || MAX_SOURCE_DEPTH <= depth) {
			return 0;
		}
		int from, to;
		_selectorRange(item->teams->selector, _teamCount(source, federation, 1 + depth), &from, &to);
		return to - from + 1;
	}
	return 0;
}

/**
 * Resolves the teams of a tournament. Teams taken from another tournament are
 * unknown until it ends, so they are named after their position on it.
 */
static void _resolveTeams(Teams * teams, Federation * federation, TournamentAttributes * attributes) {
	if (teams->type == TEAMS_LIST) {
		attributes->count = teams->names->size;
		attributes->teams = calloc(attributes->count, sizeof(char *));
		int k = 0;
		for (ListNode * node = teams->names->first; node != NULL; node = node->next) {
			attributes->teams[k++] = concatenate(1, (char *) node->element);
		}
		return;
	}
	Tournament * source = _findTournament(teams->sourceTournament, federation);
	const int sourceCount = source == NULL ? 0 : _teamCount(source, federation, 1);
	int from, to;
	_selectorRange(teams->selector, sourceCount, &from, &to);
	attributes->count = sourceCount == 0 ? 0 : to - from + 1;
	attributes->teams = calloc(attributes->count == 0 ? 1 : attributes->count, sizeof(char *));
	const bool byAverage = teams->selector->ranking->type == RANKING_AVERAGE;
	for (int k = 0; k < attributes->count; ++k) {
		attributes->teams[k] = _newString("%d° %s%s", from + k, teams->sourceTournament, byAverage ? " (promedio)" : "");
	}
}

/**
 * Collects the items of a tournament. The results are filtered by season,
 * when the year is known (i.e., inside a federation with calendar).
 */
static void _resolveAttributes(Tournament * tournament, Federation * federation, const int year, TournamentAttributes * attributes) {
	*attributes = (TournamentAttributes) {
		.teams = NULL,
		.count = 0,
		.format = NULL,
		.schedule = NULL,
		.points = NULL,
		.tiebreak = NULL,
		.zones = createList(),
		.results = createList()
	};
	for (ListNode * node = tournament->items->first; node != NULL; node = node->next) {
		TournamentItem * item = node->element;
		switch (item->type) {
			case ITEM_FORMAT:
				attributes->format = item->format;
				break;
			case ITEM_POINTS:
				attributes->points = item->points;
				break;
			case ITEM_RESULTS:
				if (year == 0 || !item->results->hasSeason || item->results->season == year) {
					for (ListNode * result = item->results->results->first; result != NULL; result = result->next) {
						appendToList(attributes->results, result->element);
					}
				}
				break;
			case ITEM_SCHEDULE:
				attributes->schedule = item->schedule;
				break;
			case ITEM_TEAMS:
				if (attributes->teams == NULL) {
					_resolveTeams(item->teams, federation, attributes);
				}
				break;
			case ITEM_TIEBREAK:
				attributes->tiebreak = item->tiebreak;
				break;
			case ITEM_USE: {
				Template * templateDefinition = _findTemplate(item->use->name, item->use->type, federation);
				if (templateDefinition == NULL) {
					break;
				}
				if (templateDefinition->type == TEMPLATE_POINTS) {
					attributes->points = templateDefinition->points;
				}
				else {
					for (ListNode * zone = templateDefinition->zones->first; zone != NULL; zone = zone->next) {
						appendToList(attributes->zones, zone->element);
					}
				}
				break;
			}
			case ITEM_ZONE:
				appendToList(attributes->zones, item->zone);
				break;
		}
	}
}

static void _freeAttributes(TournamentAttributes * attributes) {
	for (int k = 0; k < attributes->count; ++k) {
		free(attributes->teams[k]);
	}
	free(attributes->teams);
	destroyList(attributes->zones, NULL);
	destroyList(attributes->results, NULL);
}

/* -------------------------------------------------------------------------- */
/* Results and standings.                                                     */
/* -------------------------------------------------------------------------- */

static Result * _findResult(List * results, const char * home, const char * away) {
	for (ListNode * node = results->first; node != NULL; node = node->next) {
		Result * result = node->element;
		if (strcmp(result->home, home) == 0 && strcmp(result->away, away) == 0) {
			return result;
		}
	}
	return NULL;
}

static int _pointsFor(Points * points, const PointsOutcome outcome) {
	if (points != NULL) {
		for (ListNode * node = points->assignments->first; node != NULL; node = node->next) {
			PointsAssignment * assignment = node->element;
			if (assignment->outcome == outcome) {
				return assignment->points;
			}
		}
	}
	return outcome == OUTCOME_WIN ? 3 : (outcome == OUTCOME_DRAW ? 1 : 0);
}

static PointsOutcome _outcome(const int goals, const int against) {
	return goals > against ? OUTCOME_WIN : (goals == against ? OUTCOME_DRAW : OUTCOME_LOSS);
}

/** Points of team A minus points of team B, in the matches between them. */
static int _headToHead(Standings * standings, const int a, const int b) {
	int difference = 0;
	for (ListNode * node = standings->results->first; node != NULL; node = node->next) {
		Result * result = node->element;
		const int home = _teamIndex(standings->teams, standings->count, result->home);
		const int away = _teamIndex(standings->teams, standings->count, result->away);
		if (home == a && away == b) {
			difference += _pointsFor(standings->points, _outcome(result->homeGoals, result->awayGoals))
				- _pointsFor(standings->points, _outcome(result->awayGoals, result->homeGoals));
		}
		else if (home == b && away == a) {
			difference += _pointsFor(standings->points, _outcome(result->awayGoals, result->homeGoals))
				- _pointsFor(standings->points, _outcome(result->homeGoals, result->awayGoals));
		}
	}
	return difference;
}

static int _criterionDifference(Standings * standings, const TiebreakCriterion criterion, const Row * x, const Row * y) {
	switch (criterion) {
		case CRITERION_AWAY_GOALS: return x->awayGoals - y->awayGoals;
		case CRITERION_GOAL_DIFFERENCE: return (x->goalsFor - x->goalsAgainst) - (y->goalsFor - y->goalsAgainst);
		case CRITERION_GOALS_AGAINST: return y->goalsAgainst - x->goalsAgainst;
		case CRITERION_GOALS_FOR: return x->goalsFor - y->goalsFor;
		case CRITERION_HEAD_TO_HEAD: return _headToHead(standings, x->team, y->team);
		case CRITERION_WINS: return x->won - y->won;
		default: return 0;
	}
}

/** Positive if row X ranks above row Y. Ties keep the declared order. */
static int _compareRows(Standings * standings, const Row * x, const Row * y) {
	if (x->points != y->points) {
		return x->points - y->points;
	}
	if (standings->tiebreak != NULL) {
		for (ListNode * node = standings->tiebreak->criteria->first; node != NULL; node = node->next) {
			Criterion * criterion = node->element;
			const int difference = _criterionDifference(standings, criterion->value, x, y);
			if (difference != 0) {
				return difference;
			}
		}
	}
	else {
		int difference = _criterionDifference(standings, CRITERION_GOAL_DIFFERENCE, x, y);
		if (difference == 0) {
			difference = _criterionDifference(standings, CRITERION_GOALS_FOR, x, y);
		}
		if (difference != 0) {
			return difference;
		}
	}
	return y->team - x->team;
}

/** Computes and sorts the rows of the standings (the caller frees them). */
static Row * _computeStandings(Standings * standings) {
	Row * rows = calloc(standings->count == 0 ? 1 : standings->count, sizeof(Row));
	for (int k = 0; k < standings->count; ++k) {
		rows[k].team = k;
	}
	for (ListNode * node = standings->results->first; node != NULL; node = node->next) {
		Result * result = node->element;
		const int home = _teamIndex(standings->teams, standings->count, result->home);
		const int away = _teamIndex(standings->teams, standings->count, result->away);
		if (home < 0 || away < 0) {
			continue;
		}
		const int indexes[] = {home, away};
		const int goals[] = {result->homeGoals, result->awayGoals};
		for (int side = 0; side < 2; ++side) {
			Row * row = &rows[indexes[side]];
			const PointsOutcome outcome = _outcome(goals[side], goals[1 - side]);
			row->played++;
			row->won += outcome == OUTCOME_WIN;
			row->drawn += outcome == OUTCOME_DRAW;
			row->lost += outcome == OUTCOME_LOSS;
			row->goalsFor += goals[side];
			row->goalsAgainst += goals[1 - side];
			row->awayGoals += side == 1 ? goals[side] : 0;
			row->points += _pointsFor(standings->points, outcome);
		}
	}
	for (int i = 1; i < standings->count; ++i) {
		const Row key = rows[i];
		int j = i - 1;
		for (; 0 <= j && _compareRows(standings, &key, &rows[j]) > 0; --j) {
			rows[j + 1] = rows[j];
		}
		rows[j + 1] = key;
	}
	return rows;
}

/**
 * Outputs the standings of a group (the index of its first team in the teams
 * of the tournament is the offset). The zones go by position, so they stay
 * fixed when the script reorders the rows.
 */
static void _genStandings(Standings * standings, const int group, const int offset, List * zones, const char * title) {
	Row * rows = _computeStandings(standings);
	const bool hasZones = zones != NULL && 0 < zones->size;
	_output("<h4>%s</h4><table class=\"standings\" data-group=\"%d\"><tr><th>#</th><th class=\"team\">Equipo</th>"
		"<th>PJ</th><th>G</th><th>E</th><th>P</th><th>GF</th><th>GC</th><th>DG</th><th>Pts</th>%r</tr>",
		title, group, hasZones ? "<th>Zona</th>" : "");
	for (int k = 0; k < standings->count; ++k) {
		const Row * row = &rows[k];
		Zone * zone = hasZones ? _zoneAt(zones, k + 1, standings->count) : NULL;
		_output("<tr class=\"%r\" data-team=\"%d\"><td>%d</td><td class=\"team\">%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td>"
			"<td>%d</td><td>%d</td><td>%d</td><td><strong>%d</strong></td>",
			zone == NULL ? "" : _zoneClass(zone->kind), offset + row->team, k + 1, standings->teams[row->team], row->played, row->won,
			row->drawn, row->lost, row->goalsFor, row->goalsAgainst, row->goalsFor - row->goalsAgainst, row->points);
		if (hasZones) {
			_output("<td>%s</td>", zone == NULL ? "" : zone->name);
		}
		_output("</tr>");
	}
	_output("</table>");
	if (hasZones) {
		_genZoneLegend(zones);
	}
	free(rows);
}

static void _genScore(Result * result) {
	if (result == NULL) {
		_output("<td class=\"score pending\">vs</td>");
		return;
	}
	_output("<td class=\"score\">%d - %d", result->homeGoals, result->awayGoals);
	if (result->hasPenalties) {
		_output(" <small>(%d-%d pen.)</small>", result->homePenalties, result->awayPenalties);
	}
	_output("</td>");
}

/** Outputs an input for an amount of goals, with its declared value (if any). */
static void _genGoalsInput(const char * classes, const bool known, const int goals) {
	_output("<input class=\"goals %r\" type=\"number\" min=\"0\" inputmode=\"numeric\"", classes);
	if (known) {
		_output(" value=\"%d\"", goals);
	}
	_output(">");
}

/** The score of a match of the fixture, as inputs prefilled with its result. */
static void _genScoreInputs(Result * result) {
	_output("<td class=\"score\">");
	_genGoalsInput("home-goals", result != NULL, result == NULL ? 0 : result->homeGoals);
	_output(" - ");
	_genGoalsInput("away-goals", result != NULL, result == NULL ? 0 : result->awayGoals);
	_output("</td>");
}

static void _genResults(List * results) {
	if (results->size == 0) {
		return;
	}
	_output("<h4>Resultados</h4><table class=\"results\"><tr><th>Local</th><th>Resultado</th><th>Visitante</th></tr>");
	for (ListNode * node = results->first; node != NULL; node = node->next) {
		Result * result = node->element;
		_output("<tr><td>%s</td>", result->home);
		_genScore(result);
		_output("<td>%s</td></tr>", result->away);
	}
	_output("</table>");
}

/* -------------------------------------------------------------------------- */
/* Round-robin.                                                               */
/* -------------------------------------------------------------------------- */

/**
 * Builds the pairings of a single round-robin with Berger's algorithm, as a
 * matrix of (rounds × matches per round). With an odd amount of teams, an
 * extra slot acts as a bye. The caller frees the matrix.
 */
static Pairing * _berger(const int count, int * rounds, int * perRound) {
	const int slotCount = count + count % 2;
	*rounds = slotCount - 1;
	*perRound = slotCount / 2;
	int * slots = calloc(slotCount, sizeof(int));
	Pairing * pairings = calloc((*rounds) * (*perRound) == 0 ? 1 : (*rounds) * (*perRound), sizeof(Pairing));
	for (int k = 0; k < slotCount; ++k) {
		slots[k] = k;
	}
	for (int round = 0; round < *rounds; ++round) {
		for (int match = 0; match < *perRound; ++match) {
			int home = slots[match];
			int away = slots[slotCount - 1 - match];
			if (match == 0 && round % 2 == 1) {
				const int swap = home;
				home = away;
				away = swap;
			}
			const bool bye = count <= home || count <= away;
			pairings[round * (*perRound) + match] = (Pairing) {
				.home = bye ? (count <= home ? -1 : home) : home,
				.away = bye ? (count <= away ? -1 : away) : away
			};
		}
		// The last slot stays fixed, and the rest rotates one place.
		const int last = slots[slotCount - 2];
		for (int k = slotCount - 2; 0 < k; --k) {
			slots[k] = slots[k - 1];
		}
		slots[0] = last;
	}
	free(slots);
	return pairings;
}

static char _groupLetter(const int group) {
	return (char) ('A' + group % 26);
}

/**
 * Outputs the matchdays of one or several round-robins played in parallel
 * (i.e., the groups of a group stage), where group G owns the teams in the
 * range [G × size, (G + 1) × size).
 */
static void _genRoundRobin(char ** teams, const int size, const int groups, const Legs legs, Matchdays * matchdays, List * results) {
	int rounds, perRound;
	Pairing * pairings = _berger(size, &rounds, &perRound);
	const int legCount = legs == LEGS_DOUBLE ? 2 : 1;
	int number = 0;
	_output("<h4>Fixture</h4><div class=\"rounds\">");
	for (int leg = 0; leg < legCount; ++leg) {
		for (int round = 0; round < rounds; ++round) {
			_output("<div class=\"round\"><h5>Fecha %d</h5>", ++number);
			_genNextMatchday(matchdays);
			_output("<table class=\"matches\">");
			for (int group = 0; group < groups; ++group) {
				for (int match = 0; match < perRound; ++match) {
					Pairing pairing = pairings[round * perRound + match];
					if (pairing.home < 0 || pairing.away < 0) {
						continue;
					}
					int homeIndex = group * size + (leg == 0 ? pairing.home : pairing.away);
					int awayIndex = group * size + (leg == 0 ? pairing.away : pairing.home);
					const char * home = teams[homeIndex];
					const char * away = teams[awayIndex];
					Result * result = _findResult(results, home, away);
					if (result == NULL && legs == LEGS_SINGLE && (result = _findResult(results, away, home)) != NULL) {
						// With a single leg, the declared result may swap who plays at home.
						const int swap = homeIndex;
						homeIndex = awayIndex;
						awayIndex = swap;
						home = result->home;
						away = result->away;
					}
					_output("<tr class=\"match\" data-group=\"%d\" data-home=\"%d\" data-away=\"%d\">", group, homeIndex, awayIndex);
					if (1 < groups) {
						_output("<td class=\"group\">%r</td>", (char[]) {_groupLetter(group), '\0'});
					}
					_output("<td class=\"home\">%s</td>", home);
					_genScoreInputs(result);
					_output("<td class=\"away\">%s</td></tr>", away);
				}
			}
			_output("</table>");
			for (int group = 0; group < groups; ++group) {
				for (int match = 0; match < perRound; ++match) {
					Pairing pairing = pairings[round * perRound + match];
					if (pairing.home < 0 && 0 <= pairing.away) {
						_output("<p class=\"bye\">Libre: %s</p>", teams[group * size + pairing.away]);
					}
					else if (pairing.away < 0 && 0 <= pairing.home) {
						_output("<p class=\"bye\">Libre: %s</p>", teams[group * size + pairing.home]);
					}
				}
			}
			_output("</div>");
		}
	}
	_output("</div>");
	free(pairings);
}

/* -------------------------------------------------------------------------- */
/* Knockout.                                                                  */
/* -------------------------------------------------------------------------- */

static const char * _roundName(const int matches) {
	switch (matches) {
		case 1: return "Final";
		case 2: return "Semifinales";
		case 4: return "Cuartos de final";
		case 8: return "Octavos de final";
		case 16: return "Dieciseisavos de final";
		default: return NULL;
	}
}

/**
 * Finds the declared results of a tie between teams A and B: the first leg
 * (or the only one) and, for home and away ties, the second leg. Returns
 * false if the tie is incomplete.
 */
static bool _tieResults(List * results, const char * a, const char * b, const Legs legs, Result ** first, Result ** second) {
	*first = _findResult(results, a, b);
	*second = _findResult(results, b, a);
	if (legs == LEGS_SINGLE) {
		if (*first == NULL) {
			*first = *second;
		}
		*second = NULL;
		return *first != NULL;
	}
	return *first != NULL && *second != NULL;
}

/**
 * Determines the winner of a tie from the declared results (the aggregate,
 * for home and away ties, and then the penalties). Returns 0 if unknown, 1
 * if team A wins, or 2 if team B wins.
 */
static int _tieWinner(List * results, const char * a, const char * b, const Legs legs) {
	Result * first;
	Result * second;
	if (!_tieResults(results, a, b, legs, &first, &second)) {
		return 0;
	}
	int goalsA = 0, goalsB = 0, penaltiesA = 0, penaltiesB = 0;
	Result * legsOfTie[] = {first, second};
	for (int k = 0; k < 2; ++k) {
		Result * result = legsOfTie[k];
		if (result == NULL) {
			continue;
		}
		const bool aIsHome = strcmp(result->home, a) == 0;
		goalsA += aIsHome ? result->homeGoals : result->awayGoals;
		goalsB += aIsHome ? result->awayGoals : result->homeGoals;
		if (result->hasPenalties) {
			penaltiesA = aIsHome ? result->homePenalties : result->awayPenalties;
			penaltiesB = aIsHome ? result->awayPenalties : result->homePenalties;
		}
	}
	if (goalsA != goalsB) {
		return goalsA > goalsB ? 1 : 2;
	}
	if (penaltiesA != penaltiesB) {
		return penaltiesA > penaltiesB ? 1 : 2;
	}
	return 0;
}

static void _genKnockoutDates(Matchdays * matchdays, const Legs legs) {
	if (!matchdays->known) {
		return;
	}
	if (legs == LEGS_DOUBLE) {
		_output("<span class=\"date\">Ida: ");
		_genDate(_nextMatchday(matchdays));
		_output("</span><span class=\"date\">Vuelta: ");
		_genDate(_nextMatchday(matchdays));
		_output("</span>");
	}
	else {
		_genNextMatchday(matchdays);
	}
}

/** Outputs the goals of team A (from its point of view) in a leg, as an input. */
static void _genLegInput(const char * classes, Result * result, const char * a) {
	const bool aIsHome = result != NULL && strcmp(result->home, a) == 0;
	_genGoalsInput(classes, result != NULL, result == NULL ? 0 : (aIsHome ? result->homeGoals : result->awayGoals));
}

/**
 * Outputs the inputs of a tie between teams A and B, prefilled with the
 * declared results: the goals of each leg (the first leg is played at the
 * home of A) and the penalties.
 */
static void _genTieInputs(List * results, const char * a, const char * b, const Legs legs, const int winner) {
	Result * first = NULL;
	Result * second = NULL;
	_tieResults(results, a, b, legs, &first, &second);
	Result * penalties = second != NULL && second->hasPenalties ? second : (first != NULL && first->hasPenalties ? first : NULL);
	const char * names[] = {a, b};
	const char * sides[] = {"a", "b"};
	if (legs == LEGS_DOUBLE) {
		_output("<div class=\"legs\"><span>Ida</span><span>Vuelta</span></div>");
	}
	for (int side = 0; side < 2; ++side) {
		char * firstClass = _newString("first-%s", sides[side]);
		char * secondClass = _newString("second-%s", sides[side]);
		_output("<div class=\"side\"><span class=\"slot slot-%r%r\">%s</span>", sides[side], winner == side + 1 ? " winner" : "", names[side]);
		_genLegInput(firstClass, first, names[side]);
		if (legs == LEGS_DOUBLE) {
			_genLegInput(secondClass, second, names[side]);
		}
		_output("</div>");
		free(firstClass);
		free(secondClass);
	}
	const bool aIsHome = penalties != NULL && strcmp(penalties->home, a) == 0;
	_output("<div class=\"penalties\">Penales ");
	_genGoalsInput("pen-a", penalties != NULL, penalties == NULL ? 0 : (aIsHome ? penalties->homePenalties : penalties->awayPenalties));
	_output(" - ");
	_genGoalsInput("pen-b", penalties != NULL, penalties == NULL ? 0 : (aIsHome ? penalties->awayPenalties : penalties->homePenalties));
	_output("</div>");
}

/**
 * Outputs the bracket of a knockout. If the amount of entrants is not a power
 * of 2, the first ones get a bye. The winners advance when the declared
 * results allow to know them; otherwise, a placeholder takes their slot. Each
 * slot also carries its source, so that the script can resolve it again: an
 * entrant ("t:K" or "g:G:P", as given) or the winner of a tie ("w:N").
 */
static void _genKnockout(char ** entrants, char ** sources, const int count, const Legs legs, Matchdays * matchdays, List * results) {
	_output("<h4>Llaves</h4>");
	if (count < 2) {
		if (count == 1) {
			_output("<p class=\"champion\">Clasificado directo: <strong>%s</strong></p>", entrants[0]);
		}
		return;
	}
	int size = 1;
	while (size < count) {
		size <<= 1;
	}
	const int byes = size - count;
	List * owned = createList();
	const char ** slots = calloc(size, sizeof(char *));
	const char ** slotSources = calloc(size, sizeof(char *));
	bool * decided = calloc(size, sizeof(bool));
	for (int k = 0; k < byes; ++k) {
		slots[2 * k] = entrants[k];
		slotSources[2 * k] = sources[k];
	}
	for (int k = byes; k < count; ++k) {
		slots[2 * byes + (k - byes)] = entrants[k];
		slotSources[2 * byes + (k - byes)] = sources[k];
	}
	for (int k = 0; k < size; ++k) {
		decided[k] = slots[k] != NULL;
	}
	int tie = 0;
	_output("<div class=\"bracket\" data-legs=\"%r\">", legs == LEGS_DOUBLE ? "double" : "single");
	for (int width = size; 1 < width; width /= 2) {
		const int matches = width / 2;
		const char * name = _roundName(matches);
		_output("<div class=\"bracket-round\"><h5>");
		if (name != NULL) _output("%s", name);
		else _output("Ronda de %d", width);
		_output("</h5>");
		_genKnockoutDates(matchdays, legs);
		for (int match = 0; match < matches; ++match) {
			const char * a = slots[2 * match];
			const char * b = slots[2 * match + 1];
			const char * winner = NULL;
			const char * winnerSource = NULL;
			bool winnerDecided = false;
			++tie;
			if (a == NULL || b == NULL) {
				winner = a == NULL ? b : a;
				winnerSource = a == NULL ? slotSources[2 * match + 1] : slotSources[2 * match];
				winnerDecided = a == NULL ? decided[2 * match + 1] : decided[2 * match];
				_output("<div class=\"matchup\" data-tie=\"%d\" data-a=\"%r\" data-bye><small>Llave %d</small>",
					tie, winnerSource == NULL ? "" : winnerSource, tie);
				_output("<span class=\"slot slot-a\">%s</span><span class=\"bye\">pasa directo</span>", winner == NULL ? "—" : winner);
			}
			else {
				const int outcome = decided[2 * match] && decided[2 * match + 1] ? _tieWinner(results, a, b, legs) : 0;
				_output("<div class=\"matchup\" data-tie=\"%d\" data-a=\"%r\" data-b=\"%r\"><small>Llave %d</small>",
					tie, slotSources[2 * match], slotSources[2 * match + 1], tie);
				_genTieInputs(results, a, b, legs, outcome);
				if (outcome == 0) {
					char * placeholder = _newString("Ganador llave %d", tie);
					appendToList(owned, placeholder);
					winner = placeholder;
				}
				else {
					winner = outcome == 1 ? a : b;
					winnerDecided = true;
				}
				char * source = _newString("w:%d", tie);
				appendToList(owned, source);
				winnerSource = source;
			}
			_output("</div>");
			slots[match] = winner;
			slotSources[match] = winnerSource;
			decided[match] = winnerDecided;
		}
		_output("</div>");
	}
	_output("</div>");
	const bool hasChampion = decided[0] && slots[0] != NULL;
	_output("<p class=\"champion\"%r>Campeón: <strong>%s</strong></p>", hasChampion ? "" : " hidden", hasChampion ? slots[0] : "");
	free(decided);
	free(slotSources);
	free(slots);
	destroyList(owned, free);
}

/* -------------------------------------------------------------------------- */
/* Groups.                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * Outputs a group stage followed by a knockout. In the knockout, the
 * qualified teams of consecutive groups cross each other (e.g., "1° Grupo A"
 * against "2° Grupo B", and "2° Grupo A" against "1° Grupo B").
 */
static void _genGroups(TournamentAttributes * attributes, Matchdays * matchdays) {
	Format * format = attributes->format;
	const int size = format->groupSize <= 0 ? attributes->count : format->groupSize;
	const int groups = size == 0 ? 0 : attributes->count / size;
	const int advancing = _clamp(format->advancing, 0, size);
	if (groups == 0) {
		return;
	}
	_output("<h4>Grupos</h4><div class=\"groups\">");
	for (int group = 0; group < groups; ++group) {
		_output("<div class=\"group-box\"><h5>Grupo %r</h5><ul>", (char[]) {_groupLetter(group), '\0'});
		for (int k = 0; k < size; ++k) {
			_output("<li>%s</li>", attributes->teams[group * size + k]);
		}
		_output("</ul></div>");
	}
	_output("</div>");
	_genRoundRobin(attributes->teams, size, groups, LEGS_SINGLE, matchdays, attributes->results);
	for (int group = 0; group < groups; ++group) {
		Standings standings = {
			.teams = attributes->teams + group * size,
			.count = size,
			.results = attributes->results,
			.points = attributes->points,
			.tiebreak = attributes->tiebreak
		};
		char * title = _newString("Posiciones — Grupo %c", _groupLetter(group));
		_genStandings(&standings, group, group * size, NULL, title);
		free(title);
	}
	const int count = groups * advancing;
	char ** entrants = calloc(count == 0 ? 1 : count, sizeof(char *));
	char ** sources = calloc(count == 0 ? 1 : count, sizeof(char *));
	int k = 0;
	for (int group = 0; group < groups; group += 2) {
		const bool paired = group + 1 < groups;
		for (int position = 1; position <= advancing; ++position) {
			sources[k] = _newString("g:%d:%d", group, position);
			entrants[k++] = _newString("%d° Grupo %c", position, _groupLetter(group));
			if (paired) {
				sources[k] = _newString("g:%d:%d", group + 1, advancing + 1 - position);
				entrants[k++] = _newString("%d° Grupo %c", advancing + 1 - position, _groupLetter(group + 1));
			}
		}
	}
	_genKnockout(entrants, sources, k, format->legs, matchdays, attributes->results);
	for (int j = 0; j < k; ++j) {
		free(entrants[j]);
		free(sources[j]);
	}
	free(entrants);
	free(sources);
}

/* -------------------------------------------------------------------------- */
/* Tournaments.                                                               */
/* -------------------------------------------------------------------------- */

static void _genFormat(Format * format) {
	const char * legs = format->legs == LEGS_DOUBLE ? "ida y vuelta" : "partido único";
	switch (format->type) {
		case FORMAT_GROUPS:
			_output("grupos de %d (clasifican %d) y eliminación directa a %r", format->groupSize, format->advancing, legs);
			break;
		case FORMAT_KNOCKOUT:
			_output("eliminación directa a %r", legs);
			break;
		case FORMAT_ROUND_ROBIN:
			_output("todos contra todos a %r", format->legs == LEGS_DOUBLE ? "dos ruedas" : "una rueda");
			break;
	}
}

static const char * _criterionToken(const TiebreakCriterion criterion) {
	switch (criterion) {
		case CRITERION_AWAY_GOALS: return "away";
		case CRITERION_GOAL_DIFFERENCE: return "gd";
		case CRITERION_GOALS_AGAINST: return "ga";
		case CRITERION_GOALS_FOR: return "gf";
		case CRITERION_HEAD_TO_HEAD: return "h2h";
		case CRITERION_WINS: return "wins";
		default: return "";
	}
}

/**
 * Outputs the attributes that the script needs to recompute the standings:
 * the points of each outcome and the tiebreak criteria (by default, as in
 * "_compareRows").
 */
static void _genRankingAttributes(TournamentAttributes * attributes) {
	_output(" data-points=\"%d,%d,%d\" data-tiebreak=\"", _pointsFor(attributes->points, OUTCOME_WIN),
		_pointsFor(attributes->points, OUTCOME_DRAW), _pointsFor(attributes->points, OUTCOME_LOSS));
	if (attributes->tiebreak == NULL) {
		_output("gd,gf");
	}
	else {
		for (ListNode * node = attributes->tiebreak->criteria->first; node != NULL; node = node->next) {
			Criterion * criterion = node->element;
			_output("%r%r", node == attributes->tiebreak->criteria->first ? "" : ",", _criterionToken(criterion->value));
		}
	}
	_output("\"");
}

static void _genTournament(Tournament * tournament, Federation * federation, const int year) {
	TournamentAttributes attributes;
	_resolveAttributes(tournament, federation, year, &attributes);
	Matchdays matchdays = _startMatchdays(attributes.schedule, year);
	_output("<section class=\"tournament\"");
	_genRankingAttributes(&attributes);
	_output("><h3>%s", tournament->name);
	if (tournament->type == KIND_LEAGUE) {
		_output(" <small class=\"tier\">división %d</small>", tournament->tier);
	}
	_output("</h3><p class=\"meta\">%d equipos", attributes.count);
	if (attributes.format != NULL) {
		_output(" · ");
		_genFormat(attributes.format);
	}
	if (attributes.schedule != NULL && matchdays.known) {
		_output(" · desde el ");
		_genDate(matchdays.current);
	}
	_output("</p>");
	if (attributes.format != NULL) {
		Standings standings = {
			.teams = attributes.teams,
			.count = attributes.count,
			.results = attributes.results,
			.points = attributes.points,
			.tiebreak = attributes.tiebreak
		};
		_output("<button class=\"reset\" type=\"button\" hidden>Restaurar resultados declarados</button>");
		switch (attributes.format->type) {
			case FORMAT_GROUPS:
				_genGroups(&attributes, &matchdays);
				break;
			case FORMAT_KNOCKOUT: {
				char ** sources = calloc(attributes.count == 0 ? 1 : attributes.count, sizeof(char *));
				for (int k = 0; k < attributes.count; ++k) {
					sources[k] = _newString("t:%d", k);
				}
				_genKnockout(attributes.teams, sources, attributes.count, attributes.format->legs, &matchdays, attributes.results);
				for (int k = 0; k < attributes.count; ++k) {
					free(sources[k]);
				}
				free(sources);
				break;
			}
			case FORMAT_ROUND_ROBIN:
				_genRoundRobin(attributes.teams, attributes.count, 1, attributes.format->legs, &matchdays, attributes.results);
				_genStandings(&standings, 0, 0, attributes.zones, "Tabla de posiciones");
				break;
		}
	}
	_genResults(attributes.results);
	_output("</section>");
	_freeAttributes(&attributes);
}

/* -------------------------------------------------------------------------- */
/* Federations and calendars.                                                 */
/* -------------------------------------------------------------------------- */

static int _evaluateYear(CalendarRun * run, YearExpression * expression) {
	switch (expression->type) {
		case YEAR_ADDITION:
			return _evaluateYear(run, expression->leftExpression) + _evaluateYear(run, expression->rightExpression);
		case YEAR_LITERAL:
			return expression->value;
		case YEAR_SUBTRACTION:
			return _evaluateYear(run, expression->leftExpression) - _evaluateYear(run, expression->rightExpression);
		case YEAR_VARIABLE:
			for (int k = run->bindingCount - 1; 0 <= k; --k) {
				if (strcmp(run->bindings[k].name, expression->variable) == 0) {
					return run->bindings[k].value;
				}
			}
			logError(_logger, "Unbound year variable: %s", expression->variable);
			return 0;
		default:
			return 0;
	}
}

static bool _evaluateCondition(CalendarRun * run, Condition * condition) {
	switch (condition->type) {
		case CONDITION_AND:
			return _evaluateCondition(run, condition->leftCondition) && _evaluateCondition(run, condition->rightCondition);
		case CONDITION_NOT:
			return !_evaluateCondition(run, condition->operand);
		case CONDITION_OR:
			return _evaluateCondition(run, condition->leftCondition) || _evaluateCondition(run, condition->rightCondition);
		case CONDITION_COMPARISON: {
			const int left = _evaluateYear(run, condition->leftYear);
			const int right = _evaluateYear(run, condition->rightYear);
			switch (condition->comparator) {
				case COMPARISON_EQUAL: return left == right;
				case COMPARISON_GREATER_THAN: return left > right;
				case COMPARISON_GREATER_THAN_OR_EQUAL: return left >= right;
				case COMPARISON_LESS_THAN: return left < right;
				case COMPARISON_LESS_THAN_OR_EQUAL: return left <= right;
				case COMPARISON_NOT_EQUAL: return left != right;
				default: return false;
			}
		}
		default:
			return false;
	}
}

static bool _pushBinding(CalendarRun * run, const char * name, const int value) {
	if (MAX_BINDINGS <= run->bindingCount) {
		logError(_logger, "Too many year variables in scope.");
		return false;
	}
	run->bindings[run->bindingCount++] = (Binding) {
		.name = name,
		.value = value
	};
	return true;
}

/** Hosts a tournament in a year of the calendar (once, and only within its range). */
static void _addEvent(CalendarRun * run, Tournament * tournament, const int year) {
	if (tournament == NULL || year < run->calendar->fromYear || run->calendar->toYear < year) {
		return;
	}
	for (int k = 0; k < run->eventCount; ++k) {
		if (run->events[k].year == year && run->events[k].tournament == tournament) {
			return;
		}
	}
	if (run->eventCount == run->eventCapacity) {
		run->eventCapacity = run->eventCapacity == 0 ? 16 : 2 * run->eventCapacity;
		run->events = realloc(run->events, run->eventCapacity * sizeof(Event));
	}
	run->events[run->eventCount++] = (Event) {
		.year = year,
		.tournament = tournament
	};
}

static void _executeStatements(CalendarRun * run, List * statements, const int year);

static void _executeStatement(CalendarRun * run, Statement * statement, const int year) {
	switch (statement->type) {
		case STATEMENT_CALL: {
			Cycle * cycle = _findCycle(statement->call->cycle, run->federation);
			if (cycle == NULL || MAX_CALL_DEPTH <= run->callDepth) {
				break;
			}
			// The arguments are evaluated in the scope of the caller.
			const int previousCount = run->bindingCount;
			const unsigned int arity = cycle->parameters->size;
			int * values = calloc(arity == 0 ? 1 : arity, sizeof(int));
			ListNode * argument = statement->call->arguments->first;
			for (unsigned int k = 0; k < arity && argument != NULL; ++k, argument = argument->next) {
				values[k] = _evaluateYear(run, argument->element);
			}
			unsigned int k = 0;
			for (ListNode * parameter = cycle->parameters->first; parameter != NULL; parameter = parameter->next) {
				_pushBinding(run, parameter->element, values[k++]);
			}
			free(values);
			run->callDepth++;
			_executeStatements(run, cycle->body, year);
			run->callDepth--;
			run->bindingCount = previousCount;
			break;
		}
		case STATEMENT_EVERY: {
			Every * every = statement->every;
			const int start = every->start == NULL ? run->calendar->fromYear : _evaluateYear(run, every->start);
			const int interval = every->interval <= 0 ? 1 : every->interval;
			if (start <= year && (year - start) % interval == 0) {
				const int previousCount = run->bindingCount;
				if (every->variable != NULL) {
					_pushBinding(run, every->variable, year);
				}
				_executeStatements(run, every->body, year);
				run->bindingCount = previousCount;
			}
			else if (every->otherwise != NULL) {
				_executeStatements(run, every->otherwise, year);
			}
			break;
		}
		case STATEMENT_HOST: {
			Host * host = statement->host;
			Tournament * tournament = _findTournament(host->tournament, run->federation);
			switch (host->timing) {
				case HOST_CURRENT_YEAR:
					_addEvent(run, tournament, year);
					break;
				case HOST_EXPLICIT_YEAR:
					_addEvent(run, tournament, _evaluateYear(run, host->year));
					break;
				case HOST_YEARS_AFTER:
					_addEvent(run, tournament, year + host->offset);
					break;
				case HOST_YEARS_BEFORE:
					_addEvent(run, tournament, year - host->offset);
					break;
			}
			break;
		}
		case STATEMENT_IF:
			if (_evaluateCondition(run, statement->conditional->condition)) {
				_executeStatements(run, statement->conditional->thenBody, year);
			}
			else if (statement->conditional->elseBody != NULL) {
				_executeStatements(run, statement->conditional->elseBody, year);
			}
			break;
	}
}

static void _executeStatements(CalendarRun * run, List * statements, const int year) {
	if (statements == NULL) {
		return;
	}
	for (ListNode * node = statements->first; node != NULL; node = node->next) {
		_executeStatement(run, node->element, year);
	}
}

static void _genMovement(Movement * movement) {
	for (ListNode * node = movement->rules->first; node != NULL; node = node->next) {
		MovementRule * rule = node->element;
		const bool promote = rule->direction == MOVEMENT_PROMOTE;
		_output("<li class=\"%r\">%r ", promote ? "zone-promote" : "zone-relegate", promote ? "Asciende" : "Desciende");
		switch (rule->subject->type) {
			case SUBJECT_SELECTION:
				_output("el ");
				_genSelector(rule->subject->selector);
				break;
			case SUBJECT_TEAM:
				_output("<strong>%s</strong>", rule->subject->name);
				break;
			case SUBJECT_WINNER:
				_output("el ganador de <strong>%s</strong>", rule->subject->name);
				break;
			case SUBJECT_ZONE:
				_output("la zona <strong>%s</strong>", rule->subject->name);
				break;
		}
		_output(" de %s a %s</li>", rule->fromLeague, rule->toLeague);
	}
}

/**
 * Outputs a federation. With a calendar, it is evaluated year by year (the
 * "current year" of every statement), collecting the hosted tournaments,
 * which are then generated grouped by year.
 */
static void _genFederation(Federation * federation) {
	Calendar * calendar = NULL;
	bool hasMovement = false;
	for (ListNode * node = federation->items->first; node != NULL; node = node->next) {
		FederationItem * item = node->element;
		if (item->type == FEDERATION_ITEM_CALENDAR && calendar == NULL) {
			calendar = item->calendar;
		}
		hasMovement |= item->type == FEDERATION_ITEM_MOVEMENT;
	}
	_output("<section class=\"federation\"><h2>Federación %s</h2>", federation->name);
	if (hasMovement) {
		_output("<h4>Ascensos y descensos</h4><ul class=\"movement\">");
		for (ListNode * node = federation->items->first; node != NULL; node = node->next) {
			FederationItem * item = node->element;
			if (item->type == FEDERATION_ITEM_MOVEMENT) {
				_genMovement(item->movement);
			}
		}
		_output("</ul>");
	}
	if (calendar == NULL) {
		_output("<p class=\"meta\">Sin calendario: las fechas sin año no pueden resolverse.</p>");
		for (ListNode * node = federation->items->first; node != NULL; node = node->next) {
			FederationItem * item = node->element;
			if (item->type == FEDERATION_ITEM_TOURNAMENT) {
				_genTournament(item->tournament, federation, 0);
			}
		}
		_output("</section>");
		return;
	}
	CalendarRun run = {
		.federation = federation,
		.calendar = calendar,
		.bindingCount = 0,
		.callDepth = 0,
		.events = NULL,
		.eventCount = 0,
		.eventCapacity = 0
	};
	for (int year = calendar->fromYear; year <= calendar->toYear; ++year) {
		_executeStatements(&run, calendar->body, year);
	}
	for (int year = calendar->fromYear; year <= calendar->toYear; ++year) {
		_output("<div class=\"season\"><h2 class=\"year\">Temporada %d</h2>", year);
		bool empty = true;
		for (int k = 0; k < run.eventCount; ++k) {
			if (run.events[k].year == year) {
				_genTournament(run.events[k].tournament, federation, year);
				empty = false;
			}
		}
		if (empty) {
			_output("<p class=\"meta\">Sin torneos programados.</p>");
		}
		_output("</div>");
	}
	free(run.events);
	_output("</section>");
}

static void _genDeclaration(Declaration * declaration) {
	switch (declaration->type) {
		case DECLARATION_FEDERATION:
			_genFederation(declaration->federation);
			break;
		case DECLARATION_TEMPLATE:
			// Templates only generate output through the tournaments that use them.
			break;
		case DECLARATION_TOURNAMENT:
			_genTournament(declaration->tournament, NULL, 0);
			break;
	}
}

/* -------------------------------------------------------------------------- */
/* Document.                                                                  */
/* -------------------------------------------------------------------------- */

static void _genHeader() {
	_output("<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n<meta charset=\"UTF-8\">\n"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>Fixtura</title>\n<style>\n"
		"body{font-family:system-ui,-apple-system,\"Segoe UI\",sans-serif;margin:2rem auto;max-width:1100px;padding:0 1rem;color:#222;background:#fafafa}\n"
		"h1{border-bottom:3px solid #222;padding-bottom:.3rem}\n"
		"h2.year{background:#222;color:#fff;padding:.3rem .8rem;border-radius:4px;font-size:1.2rem}\n"
		"section.federation{margin:2rem 0}\n"
		"section.tournament{background:#fff;border:1px solid #ddd;border-radius:6px;padding:1rem 1.2rem;margin:1.2rem 0}\n"
		"section.tournament h3{margin:.2rem 0}\n"
		".tier{color:#666;font-weight:normal}\n"
		".meta{color:#666;margin:.2rem 0 .8rem}\n"
		"table{border-collapse:collapse;width:100%%}\n"
		"th,td{border:1px solid #ccc;padding:.4rem .6rem;text-align:center}\n"
		"th{background:#e8e8e8}\n"
		"td.team,th.team{text-align:left}\n"
		"td.home{text-align:right;width:42%%}\n"
		"td.away{text-align:left;width:42%%}\n"
		"td.score{font-weight:bold;white-space:nowrap}\n"
		"td.pending{color:#999;font-weight:normal}\n"
		".rounds{display:grid;grid-template-columns:repeat(auto-fill,minmax(320px,1fr));gap:1rem}\n"
		".round h5,.bracket-round h5{margin:.2rem 0}\n"
		".date{display:block;color:#555;font-size:.85rem;margin-bottom:.3rem}\n"
		".bye{color:#777;font-style:italic;margin:.3rem 0}\n"
		".zone-champion{background:#ffe066 !important}\n"
		".zone-promote{background:#b8f0b0 !important}\n"
		".zone-relegate{background:#f7b8b8 !important}\n"
		".zone-qualify{background:#b8d8f8 !important}\n"
		".legend{list-style:none;padding:0}\n"
		".swatch{display:inline-block;width:.9rem;height:.9rem;border:1px solid #999;margin-right:.4rem;vertical-align:middle}\n"
		".movement li{padding:.2rem .5rem;margin:.2rem 0;border-radius:3px;list-style:none}\n"
		".groups{display:flex;flex-wrap:wrap;gap:1rem}\n"
		".group-box{border:1px solid #ccc;border-radius:4px;padding:.4rem .8rem;min-width:160px}\n"
		".bracket{display:flex;gap:2rem;overflow-x:auto;padding-bottom:.5rem}\n"
		".bracket-round{display:flex;flex-direction:column;justify-content:space-around;gap:1rem;min-width:180px}\n"
		".matchup{border:1px solid #aaa;border-radius:4px;padding:.4rem;background:#fff}\n"
		".matchup span{display:block}\n"
		".matchup small{color:#888}\n"
		".matchup .winner{font-weight:bold}\n"
		".champion{font-size:1.1rem}\n"
		"[hidden]{display:none !important}\n"
		"input.goals{width:2.6rem;padding:.15rem;text-align:center;font:inherit;border:1px solid #bbb;border-radius:3px;-moz-appearance:textfield;appearance:textfield}\n"
		"input.goals::-webkit-inner-spin-button,input.goals::-webkit-outer-spin-button{-webkit-appearance:none;margin:0}\n"
		".reset{margin:.2rem 0 .6rem;font:inherit;font-size:.85rem;padding:.25rem .6rem;cursor:pointer}\n"
		".matchup .side{display:flex;align-items:center;gap:.3rem;margin:.15rem 0}\n"
		".matchup .side .slot{flex:1}\n"
		".matchup .legs{display:flex;justify-content:flex-end;gap:.3rem;font-size:.7rem;color:#888}\n"
		".matchup .legs span{display:inline-block;width:2.6rem;text-align:center}\n"
		".matchup .penalties{font-size:.8rem;color:#555;text-align:right;margin-top:.2rem}\n"
		".matchup .penalties input.goals{width:2.2rem}\n"
		"</style>\n</head>\n<body>\n<h1>Fixtura</h1>\n");
}

/**
 * The script of the document: it lets the user write the results in the
 * inputs, recomputes the standings and advances the winners of the brackets
 * (as the compiler does with the declared results), and keeps the results
 * in the local storage of the browser.
 */
static const char * _script =
	"(function () {\n"
	"\t'use strict';\n"
	"\tconst prefix = 'fixtura:' + location.pathname + ':';\n"
	"\tconst number = input => {\n"
	"\t\tif (input === null) return null;\n"
	"\t\tconst value = input.value.trim();\n"
	"\t\tconst goals = parseInt(value, 10);\n"
	"\t\treturn value === '' || isNaN(goals) || goals < 0 ? null : goals;\n"
	"\t};\n"
	"\tconst outcome = (goals, against) => goals > against ? 0 : (goals === against ? 1 : 2);\n"
	"\tconst letter = group => String.fromCharCode(65 + group % 26);\n"
	"\n"
	"\tfunction setup(section, index) {\n"
	"\t\tconst inputs = Array.from(section.querySelectorAll('input.goals'));\n"
	"\t\tconst reset = section.querySelector('button.reset');\n"
	"\t\tif (inputs.length === 0) return;\n"
	"\t\tif (reset !== null) reset.hidden = false;\n"
	"\t\tconst key = prefix + index;\n"
	"\t\tconst points = section.dataset.points.split(',').map(Number);\n"
	"\t\tconst criteria = section.dataset.tiebreak.split(',').filter(Boolean);\n"
	"\t\tconst names = {};\n"
	"\t\tsection.querySelectorAll('table.standings tr[data-team]').forEach(row => {\n"
	"\t\t\tnames[row.dataset.team] = row.querySelector('td.team').textContent;\n"
	"\t\t});\n"
	"\t\tsection.querySelectorAll('.matchup').forEach(matchup => {\n"
	"\t\t\t['a', 'b'].forEach(side => {\n"
	"\t\t\t\tconst source = matchup.getAttribute('data-' + side) || '';\n"
	"\t\t\t\tconst slot = matchup.querySelector('.slot-' + side);\n"
	"\t\t\t\tif (source.startsWith('t:') && slot !== null) names[source.slice(2)] = slot.textContent;\n"
	"\t\t\t});\n"
	"\t\t});\n"
	"\t\ttry {\n"
	"\t\t\tconst saved = JSON.parse(localStorage.getItem(key));\n"
	"\t\t\tif (Array.isArray(saved) && saved.length === inputs.length) {\n"
	"\t\t\t\tinputs.forEach((input, k) => input.value = saved[k]);\n"
	"\t\t\t}\n"
	"\t\t}\n"
	"\t\tcatch (error) {}\n"
	"\n"
	"\t\tfunction rank(teams, matches) {\n"
	"\t\t\tconst rows = new Map(teams.map(team => [team, {team: team, played: 0, won: 0, drawn: 0, lost: 0, goalsFor: 0, goalsAgainst: 0, awayGoals: 0, points: 0}]));\n"
	"\t\t\tmatches.forEach(match => {\n"
	"\t\t\t\t[[match.home, match.homeGoals, match.awayGoals, false], [match.away, match.awayGoals, match.homeGoals, true]].forEach(([team, goals, against, away]) => {\n"
	"\t\t\t\t\tconst row = rows.get(team);\n"
	"\t\t\t\t\tif (row === undefined) return;\n"
	"\t\t\t\t\tconst result = outcome(goals, against);\n"
	"\t\t\t\t\trow.played++;\n"
	"\t\t\t\t\trow.won += result === 0;\n"
	"\t\t\t\t\trow.drawn += result === 1;\n"
	"\t\t\t\t\trow.lost += result === 2;\n"
	"\t\t\t\t\trow.goalsFor += goals;\n"
	"\t\t\t\t\trow.goalsAgainst += against;\n"
	"\t\t\t\t\trow.awayGoals += away ? goals : 0;\n"
	"\t\t\t\t\trow.points += points[result];\n"
	"\t\t\t\t});\n"
	"\t\t\t});\n"
	"\t\t\tconst headToHead = (a, b) => matches.reduce((difference, match) => {\n"
	"\t\t\t\tif (match.home === a && match.away === b) return difference + points[outcome(match.homeGoals, match.awayGoals)] - points[outcome(match.awayGoals, match.homeGoals)];\n"
	"\t\t\t\tif (match.home === b && match.away === a) return difference + points[outcome(match.awayGoals, match.homeGoals)] - points[outcome(match.homeGoals, match.awayGoals)];\n"
	"\t\t\t\treturn difference;\n"
	"\t\t\t}, 0);\n"
	"\t\t\tconst differences = {\n"
	"\t\t\t\taway: (x, y) => x.awayGoals - y.awayGoals,\n"
	"\t\t\t\tgd: (x, y) => (x.goalsFor - x.goalsAgainst) - (y.goalsFor - y.goalsAgainst),\n"
	"\t\t\t\tga: (x, y) => y.goalsAgainst - x.goalsAgainst,\n"
	"\t\t\t\tgf: (x, y) => x.goalsFor - y.goalsFor,\n"
	"\t\t\t\th2h: (x, y) => headToHead(x.team, y.team),\n"
	"\t\t\t\twins: (x, y) => x.won - y.won\n"
	"\t\t\t};\n"
	"\t\t\t// Positive if X ranks above Y. Ties keep the declared order (as the compiler does).\n"
	"\t\t\tconst compare = (x, y) => {\n"
	"\t\t\t\tif (x.points !== y.points) return x.points - y.points;\n"
	"\t\t\t\tfor (const criterion of criteria) {\n"
	"\t\t\t\t\tconst difference = differences[criterion] === undefined ? 0 : differences[criterion](x, y);\n"
	"\t\t\t\t\tif (difference !== 0) return difference;\n"
	"\t\t\t\t}\n"
	"\t\t\t\treturn y.team - x.team;\n"
	"\t\t\t};\n"
	"\t\t\tconst list = Array.from(rows.values());\n"
	"\t\t\tfor (let i = 1; i < list.length; ++i) {\n"
	"\t\t\t\tconst row = list[i];\n"
	"\t\t\t\tlet j = i - 1;\n"
	"\t\t\t\tfor (; 0 <= j && compare(row, list[j]) > 0; --j) list[j + 1] = list[j];\n"
	"\t\t\t\tlist[j + 1] = row;\n"
	"\t\t\t}\n"
	"\t\t\treturn list;\n"
	"\t\t}\n"
	"\n"
	"\t\tfunction tieWinner(matchup, double) {\n"
	"\t\t\tconst value = name => number(matchup.querySelector('.' + name));\n"
	"\t\t\tconst legs = double ? ['first', 'second'] : ['first'];\n"
	"\t\t\tconst goals = ['a', 'b'].map(side => legs.reduce((total, leg) => {\n"
	"\t\t\t\tconst goals = value(leg + '-' + side);\n"
	"\t\t\t\treturn total === null || goals === null ? null : total + goals;\n"
	"\t\t\t}, 0));\n"
	"\t\t\tconst penalties = matchup.querySelector('.penalties');\n"
	"\t\t\tconst tied = goals[0] !== null && goals[0] === goals[1];\n"
	"\t\t\tif (penalties !== null) penalties.hidden = !tied && value('pen-a') === null && value('pen-b') === null;\n"
	"\t\t\tif (goals[0] === null || goals[1] === null) return 0;\n"
	"\t\t\tif (goals[0] !== goals[1]) return goals[0] > goals[1] ? 1 : 2;\n"
	"\t\t\tconst penaltiesA = value('pen-a');\n"
	"\t\t\tconst penaltiesB = value('pen-b');\n"
	"\t\t\tif (penaltiesA === null || penaltiesB === null || penaltiesA === penaltiesB) return 0;\n"
	"\t\t\treturn penaltiesA > penaltiesB ? 1 : 2;\n"
	"\t\t}\n"
	"\n"
	"\t\tfunction update() {\n"
	"\t\t\tconst groups = {};\n"
	"\t\t\tsection.querySelectorAll('tr.match').forEach(row => {\n"
	"\t\t\t\tconst group = groups[row.dataset.group] = groups[row.dataset.group] || {matches: [], total: 0};\n"
	"\t\t\t\tconst homeGoals = number(row.querySelector('.home-goals'));\n"
	"\t\t\t\tconst awayGoals = number(row.querySelector('.away-goals'));\n"
	"\t\t\t\tgroup.total++;\n"
	"\t\t\t\tif (homeGoals !== null && awayGoals !== null) {\n"
	"\t\t\t\t\tgroup.matches.push({home: +row.dataset.home, away: +row.dataset.away, homeGoals: homeGoals, awayGoals: awayGoals});\n"
	"\t\t\t\t}\n"
	"\t\t\t});\n"
	"\t\t\tconst rankings = {};\n"
	"\t\t\tsection.querySelectorAll('table.standings').forEach(table => {\n"
	"\t\t\t\tconst group = groups[table.dataset.group] || {matches: [], total: 0};\n"
	"\t\t\t\tconst rows = Array.from(table.querySelectorAll('tr[data-team]'));\n"
	"\t\t\t\tconst ranked = rank(rows.map(row => +row.dataset.team).sort((x, y) => x - y), group.matches);\n"
	"\t\t\t\trankings[table.dataset.group] = {order: ranked.map(row => row.team), complete: group.matches.length === group.total};\n"
	"\t\t\t\trows.forEach((row, k) => {\n"
	"\t\t\t\t\tconst stats = ranked[k];\n"
	"\t\t\t\t\tconst cells = row.cells;\n"
	"\t\t\t\t\trow.dataset.team = stats.team;\n"
	"\t\t\t\t\tcells[1].textContent = names[stats.team];\n"
	"\t\t\t\t\t[stats.played, stats.won, stats.drawn, stats.lost, stats.goalsFor, stats.goalsAgainst, stats.goalsFor - stats.goalsAgainst]\n"
	"\t\t\t\t\t\t.forEach((value, column) => cells[2 + column].textContent = value);\n"
	"\t\t\t\t\tcells[9].querySelector('strong').textContent = stats.points;\n"
	"\t\t\t\t});\n"
	"\t\t\t});\n"
	"\t\t\tsection.querySelectorAll('.bracket').forEach(bracket => {\n"
	"\t\t\t\tconst double = bracket.dataset.legs === 'double';\n"
	"\t\t\t\tconst winners = {};\n"
	"\t\t\t\tlet last = null;\n"
	"\t\t\t\tconst resolve = source => {\n"
	"\t\t\t\t\tconst parts = (source || '').split(':');\n"
	"\t\t\t\t\tswitch (parts[0]) {\n"
	"\t\t\t\t\t\tcase 't': return {name: names[parts[1]], known: true};\n"
	"\t\t\t\t\t\tcase 'g': {\n"
	"\t\t\t\t\t\t\tconst ranking = rankings[parts[1]];\n"
	"\t\t\t\t\t\t\treturn ranking !== undefined && ranking.complete\n"
	"\t\t\t\t\t\t\t\t? {name: names[ranking.order[parts[2] - 1]], known: true}\n"
	"\t\t\t\t\t\t\t\t: {name: parts[2] + '° Grupo ' + letter(+parts[1]), known: false};\n"
	"\t\t\t\t\t\t}\n"
	"\t\t\t\t\t\tcase 'w': return winners[parts[1]] || {name: 'Ganador llave ' + parts[1], known: false};\n"
	"\t\t\t\t\t\tdefault: return {name: '—', known: false};\n"
	"\t\t\t\t\t}\n"
	"\t\t\t\t};\n"
	"\t\t\t\tbracket.querySelectorAll('.matchup').forEach(matchup => {\n"
	"\t\t\t\t\tconst tie = matchup.dataset.tie;\n"
	"\t\t\t\t\tconst a = resolve(matchup.dataset.a);\n"
	"\t\t\t\t\tconst slotA = matchup.querySelector('.slot-a');\n"
	"\t\t\t\t\tslotA.textContent = a.name;\n"
	"\t\t\t\t\tif (matchup.hasAttribute('data-bye')) {\n"
	"\t\t\t\t\t\tlast = winners[tie] = a;\n"
	"\t\t\t\t\t\treturn;\n"
	"\t\t\t\t\t}\n"
	"\t\t\t\t\tconst b = resolve(matchup.dataset.b);\n"
	"\t\t\t\t\tconst slotB = matchup.querySelector('.slot-b');\n"
	"\t\t\t\t\tslotB.textContent = b.name;\n"
	"\t\t\t\t\tconst winner = tieWinner(matchup, double);\n"
	"\t\t\t\t\tconst decided = winner !== 0 && a.known && b.known;\n"
	"\t\t\t\t\tslotA.classList.toggle('winner', decided && winner === 1);\n"
	"\t\t\t\t\tslotB.classList.toggle('winner', decided && winner === 2);\n"
	"\t\t\t\t\tlast = winners[tie] = decided ? (winner === 1 ? a : b) : {name: 'Ganador llave ' + tie, known: false};\n"
	"\t\t\t\t});\n"
	"\t\t\t\tconst champion = bracket.nextElementSibling;\n"
	"\t\t\t\tif (champion !== null && champion.classList.contains('champion') && last !== null) {\n"
	"\t\t\t\t\tchampion.hidden = !last.known;\n"
	"\t\t\t\t\tchampion.querySelector('strong').textContent = last.known ? last.name : '';\n"
	"\t\t\t\t}\n"
	"\t\t\t});\n"
	"\t\t}\n"
	"\n"
	"\t\tsection.addEventListener('input', () => {\n"
	"\t\t\tupdate();\n"
	"\t\t\ttry {\n"
	"\t\t\t\tlocalStorage.setItem(key, JSON.stringify(inputs.map(input => input.value)));\n"
	"\t\t\t}\n"
	"\t\t\tcatch (error) {}\n"
	"\t\t});\n"
	"\t\tif (reset !== null) {\n"
	"\t\t\treset.addEventListener('click', () => {\n"
	"\t\t\t\ttry {\n"
	"\t\t\t\t\tlocalStorage.removeItem(key);\n"
	"\t\t\t\t}\n"
	"\t\t\t\tcatch (error) {}\n"
	"\t\t\t\tinputs.forEach(input => input.value = input.defaultValue);\n"
	"\t\t\t\tupdate();\n"
	"\t\t\t});\n"
	"\t\t}\n"
	"\t\tupdate();\n"
	"\t}\n"
	"\n"
	"\tdocument.querySelectorAll('section.tournament').forEach(setup);\n"
	"})();\n";

static void _genFooter() {
	_output("\n<script>\n%r</script>\n</body>\n</html>\n", _script);
}

/** PUBLIC FUNCTIONS */

void executeGenerator(CompilerState * compilerState) {
	logDebugging(_logger, "Generating the fixtures...");
	_program = compilerState->abstractSyntaxtTree;
	_genHeader();
	for (ListNode * node = _program->declarations->first; node != NULL; node = node->next) {
		_genDeclaration(node->element);
	}
	_genFooter();
	_program = NULL;
	logDebugging(_logger, "Generation is done.");
}
