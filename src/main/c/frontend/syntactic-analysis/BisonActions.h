#ifndef BISON_ACTIONS_HEADER
#define BISON_ACTIONS_HEADER

#include "../../support/language/List.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonParser.h"
#include <stdbool.h>
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeBisonActionsModule();

/**
 * Reports a syntax error (called by "yyerror").
 */
void SyntaxErrorAction(const YYLTYPE * location, const char * message);

/**
 * Bison semantic actions.
 */

/* Generic lists. */
List * AppendSemanticAction(List * list, void * element);
List * EmptyListSemanticAction();
List * ListSemanticAction(void * element);

/* Program and declarations. */
Program * ProgramSemanticAction(List * declarations);
Declaration * FederationDeclarationSemanticAction(Federation * federation);
Declaration * TemplateDeclarationSemanticAction(Template * templateDefinition);
Declaration * TournamentDeclarationSemanticAction(Tournament * tournament);

/* Tournaments and leagues. */
Tournament * LeagueSemanticAction(char * name, const int tier, List * items);
Tournament * TournamentSemanticAction(char * name, List * items);
TournamentItem * FormatItemSemanticAction(Format * format);
TournamentItem * PointsItemSemanticAction(Points * points);
TournamentItem * ResultsItemSemanticAction(Results * results);
TournamentItem * ScheduleItemSemanticAction(Schedule * schedule);
TournamentItem * TeamsItemSemanticAction(Teams * teams);
TournamentItem * TiebreakItemSemanticAction(Tiebreak * tiebreak);
TournamentItem * UseItemSemanticAction(const TemplateType type, char * name);
TournamentItem * ZoneItemSemanticAction(Zone * zone);
Teams * TeamListSemanticAction(List * names);
Teams * TeamsFromTournamentSemanticAction(char * sourceTournament, Selector * selector);
Format * GroupsFormatSemanticAction(const int groupSize, const int advancing, const Legs legs);
Format * KnockoutFormatSemanticAction(const Legs legs);
Format * RoundRobinFormatSemanticAction(const Legs legs);
Schedule * ScheduleSemanticAction(const Date start, Period * period);
Date YearlessDateSemanticAction(const int month, const int day);
Period * DaysPeriodSemanticAction(const int amount);
Period * WeekdaysPeriodSemanticAction(const unsigned int weekdays);
Period * WeeksPeriodSemanticAction(const int amount);
int WeekdaySetSemanticAction(const int weekdays, const int weekday);
Zone * ZoneSemanticAction(char * name, const ZoneKind kind, char * cup, Selector * selector);
Selector * BottomSelectorSemanticAction(const int count, Ranking * ranking);
Selector * RangeSelectorSemanticAction(Position * from, Position * to, Ranking * ranking);
Selector * TopSelectorSemanticAction(const int count, Ranking * ranking);
Position * FromBottomPositionSemanticAction(const int offset);
Position * FromTopPositionSemanticAction(const int position);
Ranking * AverageRankingSemanticAction(const int seasons);
Ranking * TableRankingSemanticAction();
Points * PointsSemanticAction(List * assignments);
PointsAssignment * PointsAssignmentSemanticAction(const PointsOutcome outcome, const int points);
Tiebreak * TiebreakSemanticAction(List * criteria);
Criterion * CriterionSemanticAction(const TiebreakCriterion value);
Results * ResultsSemanticAction(const bool hasSeason, const int season, List * results);
Result * PenaltiesResultSemanticAction(char * home, const int homeGoals, const int awayGoals, char * away, const int homePenalties, const int awayPenalties);
Result * ResultSemanticAction(char * home, const int homeGoals, const int awayGoals, char * away);

/* Templates. */
Template * PointsTemplateSemanticAction(char * name, Points * points);
Template * ZonesTemplateSemanticAction(char * name, List * zones);

/* Federations. */
Federation * FederationSemanticAction(char * name, List * items);
FederationItem * CalendarFederationItemSemanticAction(Calendar * calendar);
FederationItem * CycleFederationItemSemanticAction(Cycle * cycle);
FederationItem * MovementFederationItemSemanticAction(Movement * movement);
FederationItem * TemplateFederationItemSemanticAction(Template * templateDefinition);
FederationItem * TournamentFederationItemSemanticAction(Tournament * tournament);
Cycle * CycleSemanticAction(char * name, List * parameters, List * body);
Movement * MovementSemanticAction(List * rules);
MovementRule * MovementRuleSemanticAction(const MovementDirection direction, MovementSubject * subject, char * fromLeague, char * toLeague);
MovementSubject * NamedSubjectSemanticAction(const MovementSubjectType type, char * name);
MovementSubject * SelectionSubjectSemanticAction(Selector * selector);
Calendar * CalendarSemanticAction(const int fromYear, const int toYear, List * body);

/* Calendar statements. */
Statement * CallStatementSemanticAction(char * cycle, List * arguments);
Statement * EveryStatementSemanticAction(const int interval, YearExpression * start, char * variable, List * body, List * otherwise);
Statement * HostStatementSemanticAction(char * tournament, const HostTiming timing, YearExpression * year, const int offset);
Statement * IfStatementSemanticAction(Condition * condition, List * thenBody, List * elseBody);
Condition * ComparisonConditionSemanticAction(YearExpression * left, const ComparisonOperator comparator, YearExpression * right);
Condition * LogicalConditionSemanticAction(Condition * left, Condition * right, const ConditionType type);
Condition * NotConditionSemanticAction(Condition * operand);
YearExpression * ArithmeticYearExpressionSemanticAction(YearExpression * left, YearExpression * right, const YearExpressionType type);
YearExpression * LiteralYearExpressionSemanticAction(const int value);
YearExpression * VariableYearExpressionSemanticAction(char * variable);

#endif
