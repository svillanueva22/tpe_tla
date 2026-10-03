#ifndef ABSTRACT_SYNTAX_TREE_HEADER
#define ABSTRACT_SYNTAX_TREE_HEADER

#include "../../support/language/List.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdbool.h>
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeAbstractSyntaxTreeModule();

/**
 * This type definitions allows self-referencing types (e.g., a condition that
 * is made of another conditions, or a statement whose body is a list of
 * statements).
 *
 * IMPORTANT: every enumerated constant uses a prefix, because Bison exports
 * the terminal symbols (e.g., FIRST, LAST, WIN) as plain C enumerations too.
 */

typedef struct Calendar Calendar;
typedef struct Call Call;
typedef struct Condition Condition;
typedef struct Criterion Criterion;
typedef struct Cycle Cycle;
typedef struct Date Date;
typedef struct Declaration Declaration;
typedef struct Every Every;
typedef struct Federation Federation;
typedef struct FederationItem FederationItem;
typedef struct Format Format;
typedef struct Host Host;
typedef struct If If;
typedef struct Movement Movement;
typedef struct MovementRule MovementRule;
typedef struct MovementSubject MovementSubject;
typedef struct Period Period;
typedef struct Points Points;
typedef struct PointsAssignment PointsAssignment;
typedef struct Position Position;
typedef struct Program Program;
typedef struct Ranking Ranking;
typedef struct Result Result;
typedef struct Results Results;
typedef struct Schedule Schedule;
typedef struct Selector Selector;
typedef struct Statement Statement;
typedef struct Teams Teams;
typedef struct Template Template;
typedef struct Tiebreak Tiebreak;
typedef struct Tournament Tournament;
typedef struct TournamentItem TournamentItem;
typedef struct Use Use;
typedef struct YearExpression YearExpression;
typedef struct Zone Zone;

/**
 * Enumerations.
 */

typedef enum {
	COMPARISON_EQUAL,
	COMPARISON_GREATER_THAN,
	COMPARISON_GREATER_THAN_OR_EQUAL,
	COMPARISON_LESS_THAN,
	COMPARISON_LESS_THAN_OR_EQUAL,
	COMPARISON_NOT_EQUAL
} ComparisonOperator;

typedef enum {
	CONDITION_AND,
	CONDITION_COMPARISON,
	CONDITION_NOT,
	CONDITION_OR
} ConditionType;

typedef enum {
	DECLARATION_FEDERATION,
	DECLARATION_TEMPLATE,
	DECLARATION_TOURNAMENT
} DeclarationType;

typedef enum {
	FEDERATION_ITEM_CALENDAR,
	FEDERATION_ITEM_CYCLE,
	FEDERATION_ITEM_MOVEMENT,
	FEDERATION_ITEM_TEMPLATE,
	FEDERATION_ITEM_TOURNAMENT
} FederationItemType;

typedef enum {
	FORMAT_GROUPS,
	FORMAT_KNOCKOUT,
	FORMAT_ROUND_ROBIN
} FormatType;

typedef enum {
	HOST_CURRENT_YEAR,
	HOST_EXPLICIT_YEAR,
	HOST_YEARS_AFTER,
	HOST_YEARS_BEFORE
} HostTiming;

/** Matches per pairing: a single match, or home and away. */
typedef enum {
	LEGS_DOUBLE,
	LEGS_SINGLE
} Legs;

typedef enum {
	MOVEMENT_PROMOTE,
	MOVEMENT_RELEGATE
} MovementDirection;

typedef enum {
	SUBJECT_SELECTION,
	SUBJECT_TEAM,
	SUBJECT_WINNER,
	SUBJECT_ZONE
} MovementSubjectType;

typedef enum {
	PERIOD_DAYS,
	PERIOD_WEEKDAYS,
	PERIOD_WEEKS
} PeriodType;

typedef enum {
	OUTCOME_DRAW,
	OUTCOME_LOSS,
	OUTCOME_WIN
} PointsOutcome;

/** A position counted from the top (1 is the first), or from the bottom (0 is the last). */
typedef enum {
	POSITION_FROM_BOTTOM,
	POSITION_FROM_TOP
} PositionType;

typedef enum {
	RANKING_AVERAGE,
	RANKING_TABLE
} RankingType;

typedef enum {
	SELECTOR_BOTTOM,
	SELECTOR_RANGE,
	SELECTOR_TOP
} SelectorType;

typedef enum {
	STATEMENT_CALL,
	STATEMENT_EVERY,
	STATEMENT_HOST,
	STATEMENT_IF
} StatementType;

typedef enum {
	TEAMS_FROM_TOURNAMENT,
	TEAMS_LIST
} TeamsType;

typedef enum {
	TEMPLATE_POINTS,
	TEMPLATE_ZONES
} TemplateType;

typedef enum {
	CRITERION_AWAY_GOALS,
	CRITERION_GOAL_DIFFERENCE,
	CRITERION_GOALS_AGAINST,
	CRITERION_GOALS_FOR,
	CRITERION_HEAD_TO_HEAD,
	CRITERION_WINS
} TiebreakCriterion;

typedef enum {
	KIND_LEAGUE,
	KIND_TOURNAMENT
} TournamentType;

typedef enum {
	ITEM_FORMAT,
	ITEM_POINTS,
	ITEM_RESULTS,
	ITEM_SCHEDULE,
	ITEM_TEAMS,
	ITEM_TIEBREAK,
	ITEM_USE,
	ITEM_ZONE
} TournamentItemType;

/** ISO 8601 day of the week (Monday is 1, Sunday is 7). */
typedef enum {
	MONDAY = 1,
	TUESDAY = 2,
	WEDNESDAY = 3,
	THURSDAY = 4,
	FRIDAY = 5,
	SATURDAY = 6,
	SUNDAY = 7
} Weekday;

typedef enum {
	YEAR_ADDITION,
	YEAR_LITERAL,
	YEAR_SUBTRACTION,
	YEAR_VARIABLE
} YearExpressionType;

typedef enum {
	ZONE_CHAMPION,
	ZONE_PROMOTE,
	ZONE_QUALIFY,
	ZONE_RELEGATE
} ZoneKind;

/**
 * Node types for the Abstract Syntax Tree (AST). Every List field documents
 * the type of its elements.
 */

/** A calendar date. A yearless date (inside a federation) has year = 0. */
struct Date {
	int year;
	int month;
	int day;
};

struct Program {
	List * declarations;				// Declaration *
};

struct Declaration {
	union {
		Federation * federation;
		Template * templateDefinition;
		Tournament * tournament;
	};
	DeclarationType type;
};

/* -------------------------------------------------------------------------- */
/* Tournaments and leagues.                                                   */
/* -------------------------------------------------------------------------- */

struct Tournament {
	char * name;
	TournamentType type;
	int tier;							// Only for leagues.
	List * items;						// TournamentItem *
};

struct TournamentItem {
	union {
		Format * format;
		Points * points;
		Results * results;
		Schedule * schedule;
		Teams * teams;
		Tiebreak * tiebreak;
		Use * use;
		Zone * zone;
	};
	TournamentItemType type;
};

struct Teams {
	union {
		List * names;					// char * (team names, in declared order)
		struct {
			char * sourceTournament;
			Selector * selector;
		};
	};
	TeamsType type;
};

struct Format {
	FormatType type;
	Legs legs;							// Legs of the round-robin or of the knockout.
	int groupSize;						// Only for groups.
	int advancing;						// Only for groups.
};

struct Schedule {
	Date start;
	Period * period;
};

struct Period {
	PeriodType type;
	int amount;							// Days or weeks.
	unsigned int weekdays;				// Bitmask: bit k is set if Weekday k is included.
};

struct Zone {
	char * name;
	ZoneKind kind;
	char * cup;							// Only for qualification zones.
	Selector * selector;
};

/**
 * A group of positions over a ranking: "first" is "top 1", "last" is
 * "bottom 1" and "position P" is the range P to P.
 */
struct Selector {
	SelectorType type;
	int count;							// For top/bottom.
	Position * from;					// For ranges.
	Position * to;						// For ranges.
	Ranking * ranking;
};

struct Position {
	PositionType type;
	int value;							// Absolute from top, or offset from bottom.
};

struct Ranking {
	RankingType type;
	int seasons;						// Only for averages.
};

struct Points {
	List * assignments;					// PointsAssignment *
};

struct PointsAssignment {
	PointsOutcome outcome;
	int points;
};

struct Use {
	TemplateType type;
	char * name;
};

struct Tiebreak {
	List * criteria;					// Criterion *
};

struct Criterion {
	TiebreakCriterion value;
};

struct Results {
	bool hasSeason;
	int season;							// Only inside a federation.
	List * results;						// Result *
};

struct Result {
	char * home;
	int homeGoals;
	int awayGoals;
	char * away;
	bool hasPenalties;
	int homePenalties;
	int awayPenalties;
};

/* -------------------------------------------------------------------------- */
/* Templates.                                                                 */
/* -------------------------------------------------------------------------- */

struct Template {
	char * name;
	union {
		Points * points;
		List * zones;					// Zone *
	};
	TemplateType type;
};

/* -------------------------------------------------------------------------- */
/* Federations.                                                               */
/* -------------------------------------------------------------------------- */

struct Federation {
	char * name;
	List * items;						// FederationItem *
};

struct FederationItem {
	union {
		Calendar * calendar;
		Cycle * cycle;
		Movement * movement;
		Template * templateDefinition;
		Tournament * tournament;
	};
	FederationItemType type;
};

struct Cycle {
	char * name;
	List * parameters;					// char *
	List * body;						// Statement *
};

struct Movement {
	List * rules;						// MovementRule *
};

struct MovementRule {
	MovementDirection direction;
	MovementSubject * subject;
	char * fromLeague;
	char * toLeague;
};

struct MovementSubject {
	union {
		Selector * selector;
		char * name;					// Zone, team, or cup (winner) name.
	};
	MovementSubjectType type;
};

struct Calendar {
	int fromYear;
	int toYear;
	List * body;						// Statement *
};

/* -------------------------------------------------------------------------- */
/* Calendar statements.                                                       */
/* -------------------------------------------------------------------------- */

struct Statement {
	union {
		Call * call;
		Every * every;
		Host * host;
		If * conditional;
	};
	StatementType type;
};

struct Call {
	char * cycle;
	List * arguments;					// YearExpression *
};

/**
 * A periodic loop over the years of the calendar: every year, or every N
 * years (optionally starting at some year). The "otherwise" branch, if
 * present, runs on the years that do not match the period.
 */
struct Every {
	int interval;
	YearExpression * start;				// NULL if absent.
	char * variable;					// NULL if absent.
	List * body;						// Statement *
	List * otherwise;					// Statement *, or NULL if absent.
};

struct Host {
	char * tournament;
	HostTiming timing;
	YearExpression * year;				// Only for explicit years.
	int offset;							// Only for years before/after.
};

struct If {
	Condition * condition;
	List * thenBody;					// Statement *
	List * elseBody;					// Statement *, or NULL if absent.
};

struct Condition {
	union {
		struct {
			Condition * leftCondition;
			Condition * rightCondition;
		};
		Condition * operand;
		struct {
			YearExpression * leftYear;
			YearExpression * rightYear;
			ComparisonOperator comparator;
		};
	};
	ConditionType type;
};

struct YearExpression {
	union {
		struct {
			YearExpression * leftExpression;
			YearExpression * rightExpression;
		};
		int value;
		char * variable;
	};
	YearExpressionType type;
};

/**
 * Node recursive super-duper-trambolik-destructors.
 */

void destroyCalendar(Calendar * calendar);
void destroyCall(Call * call);
void destroyCondition(Condition * condition);
void destroyCriterion(Criterion * criterion);
void destroyCycle(Cycle * cycle);
void destroyDeclaration(Declaration * declaration);
void destroyEvery(Every * every);
void destroyFederation(Federation * federation);
void destroyFederationItem(FederationItem * federationItem);
void destroyFormat(Format * format);
void destroyHost(Host * host);
void destroyIf(If * conditional);
void destroyMovement(Movement * movement);
void destroyMovementRule(MovementRule * movementRule);
void destroyMovementSubject(MovementSubject * movementSubject);
void destroyPeriod(Period * period);
void destroyPoints(Points * points);
void destroyPointsAssignment(PointsAssignment * pointsAssignment);
void destroyPosition(Position * position);
void destroyProgram(Program * program);
void destroyRanking(Ranking * ranking);
void destroyResult(Result * result);
void destroyResults(Results * results);
void destroySchedule(Schedule * schedule);
void destroySelector(Selector * selector);
void destroyStatement(Statement * statement);
void destroyTeams(Teams * teams);
void destroyTemplate(Template * templateDefinition);
void destroyTiebreak(Tiebreak * tiebreak);
void destroyTournament(Tournament * tournament);
void destroyTournamentItem(TournamentItem * tournamentItem);
void destroyUse(Use * use);
void destroyYearExpression(YearExpression * yearExpression);
void destroyZone(Zone * zone);

/**
 * Typed list destructors (they destroy the list and its elements).
 */

void destroyArgumentList(List * arguments);
void destroyCriterionList(List * criteria);
void destroyDeclarationList(List * declarations);
void destroyFederationItemList(List * federationItems);
void destroyMovementRuleList(List * movementRules);
void destroyPointsAssignmentList(List * pointsAssignments);
void destroyResultList(List * results);
void destroyStatementList(List * statements);
void destroyStringList(List * strings);
void destroyTournamentItemList(List * tournamentItems);
void destroyZoneList(List * zones);

#endif
