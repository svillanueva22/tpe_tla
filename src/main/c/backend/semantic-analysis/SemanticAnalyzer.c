#include "SemanticAnalyzer.h"
#include <string.h>
#include <stdio.h>

/* ========================================================================== */
/* MODULE INTERNAL STATE                                                       */
/* ========================================================================== */

static Logger * _logger = NULL;

static void _shutdownSemanticAnalyzerModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: SemanticAnalyzer...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeSemanticAnalyzerModule() {
	_logger = createLogger("SemanticAnalyzer");
	return _shutdownSemanticAnalyzerModule;
}

/* ========================================================================== */
/* SYMBOL TABLE                                                                */
/* ========================================================================== */

typedef enum {
	SYM_TOURNAMENT,
	SYM_LEAGUE,
	SYM_TEMPLATE_POINTS,
	SYM_TEMPLATE_ZONES,
	SYM_CYCLE,
	SYM_YEAR_VARIABLE
} SymbolKind;

typedef struct {
	char *     name;
	SymbolKind kind;
	int        tier;    /* SYM_LEAGUE only */
	int        arity;   /* SYM_CYCLE only */
} Symbol;

/* ========================================================================== */
/* SCOPE STACK                                                                 */
/* ========================================================================== */

#define MAX_SCOPE_DEPTH 16

typedef struct {
	List * scopes[MAX_SCOPE_DEPTH];
	int    top; /* index of current scope; -1 = empty */
} ScopeStack;

static void _pushScope(ScopeStack * stack) {
	if (stack->top + 1 >= MAX_SCOPE_DEPTH) {
		logError(_logger, "Scope stack overflow (depth > %d)", MAX_SCOPE_DEPTH);
		return;
	}
	stack->top++;
	stack->scopes[stack->top] = createList();
}

static void _popScope(ScopeStack * stack) {
	if (stack->top < 0) return;
	destroyList(stack->scopes[stack->top], free);
	stack->scopes[stack->top] = NULL;
	stack->top--;
}

static Symbol * _lookupInStack(ScopeStack * stack, const char * name) {
	for (int s = stack->top; s >= 0; --s) {
		for (ListNode * n = stack->scopes[s]->first; n != NULL; n = n->next) {
			Symbol * sym = n->element;
			if (strcmp(sym->name, name) == 0) return sym;
		}
	}
	return NULL;
}

/* Returns false if name already exists in the CURRENT scope. */
static bool _insertInCurrentScope(ScopeStack * stack, Symbol * sym) {
	if (stack->top < 0) return false;
	List * current = stack->scopes[stack->top];
	for (ListNode * n = current->first; n != NULL; n = n->next) {
		Symbol * existing = n->element;
		if (strcmp(existing->name, sym->name) == 0) return false;
	}
	appendToList(current, sym);
	return true;
}

static Symbol * _makeSymbol(const char * name, SymbolKind kind, int tier, int arity) {
	Symbol * sym = malloc(sizeof(Symbol));
	sym->name  = (char *) name; /* borrows AST string — AST owns the memory */
	sym->kind  = kind;
	sym->tier  = tier;
	sym->arity = arity;
	return sym;
}

/* ========================================================================== */
/* FORWARD DECLARATIONS                                                        */
/* ========================================================================== */

static void _analyzeProgram(Program * program, ScopeStack * stack, CompilerState * state);
static void _analyzeDeclaration(Declaration * decl, ScopeStack * stack, CompilerState * state, bool insideFederation);
static void _analyzeTournament(Tournament * t, ScopeStack * stack, CompilerState * state, bool insideFederation, bool registerSymbol);
static void _analyzeTemplate(Template * tmpl, ScopeStack * stack, CompilerState * state);
static void _analyzeFederation(Federation * fed, ScopeStack * stack, CompilerState * state);
static void _analyzeMovement(Movement * mov, ScopeStack * stack, CompilerState * state, int maxTier);
static void _analyzeCalendar(Calendar * cal, ScopeStack * stack, CompilerState * state);
static void _analyzeStatements(List * stmts, ScopeStack * stack, CompilerState * state, bool insideLoop);
static void _analyzeStatement(Statement * stmt, ScopeStack * stack, CompilerState * state, bool insideLoop);
static void _analyzeYearExpression(YearExpression * expr, ScopeStack * stack, CompilerState * state);
static void _analyzeCondition(Condition * cond, ScopeStack * stack, CompilerState * state);

/* ========================================================================== */
/* HELPER: resolve a Selector to a 1-based [from,to] range.                  */
/* Returns false if resolution is impossible (e.g. teamCount == 0).          */
/* ========================================================================== */

static bool _selectorToRange(Selector * sel, int teamCount, int * outFrom, int * outTo) {
	if (teamCount <= 0) return false;
	switch (sel->type) {
		case SELECTOR_TOP:
			*outFrom = 1;
			*outTo   = sel->count;
			break;
		case SELECTOR_BOTTOM:
			*outFrom = teamCount - sel->count + 1;
			*outTo   = teamCount;
			break;
		case SELECTOR_RANGE: {
			Position * f = sel->from;
			Position * t = sel->to;
			*outFrom = (f->type == POSITION_FROM_TOP) ? f->value : (teamCount - f->value);
			*outTo   = (t->type == POSITION_FROM_TOP) ? t->value : (teamCount - t->value);
			break;
		}
	}
	return true;
}

/* ========================================================================== */
/* HELPER: zones ranked by the table, or by the same average, share positions.*/
/* ========================================================================== */

static bool _sameRanking(Ranking * a, Ranking * b) {
	if (a->type != b->type) return false;
	return a->type == RANKING_TABLE || a->seasons == b->seasons;
}

/* ========================================================================== */
/* HELPER: days in month for a yearless date (Feb is 29: it may be leap).     */
/* ========================================================================== */

static int _daysInMonth(int month) {
	static const int days[] = {0,31,29,31,30,31,30,31,31,30,31,30,31};
	if (month < 1 || month > 12) return 0;
	return days[month];
}

/* ========================================================================== */
/* ANALYSIS FUNCTIONS                                                          */
/* ========================================================================== */

static void _analyzeProgram(Program * program, ScopeStack * stack, CompilerState * state) {
	_pushScope(stack); /* global scope */
	for (ListNode * n = program->declarations->first; n != NULL; n = n->next) {
		_analyzeDeclaration(n->element, stack, state, false);
	}
	_popScope(stack);
}

static void _analyzeDeclaration(Declaration * decl, ScopeStack * stack, CompilerState * state, bool insideFederation) {
	switch (decl->type) {
		case DECLARATION_TOURNAMENT:
			_analyzeTournament(decl->tournament, stack, state, insideFederation, true);
			break;
		case DECLARATION_TEMPLATE:
			_analyzeTemplate(decl->templateDefinition, stack, state);
			break;
		case DECLARATION_FEDERATION:
			_analyzeFederation(decl->federation, stack, state);
			break;
	}
}

/* -------------------------------------------------------------------------- */
/* Tournament validation                                                       */
/* -------------------------------------------------------------------------- */

static void _analyzeTournament(Tournament * t, ScopeStack * stack, CompilerState * state, bool insideFederation, bool registerSymbol) {
	/* [02] duplicate name — skipped in federation 2nd pass (already registered in 1st pass) */
	if (registerSymbol) {
		SymbolKind kind = (t->type == KIND_LEAGUE) ? SYM_LEAGUE : SYM_TOURNAMENT;
		Symbol * sym = _makeSymbol(t->name, kind, t->tier, 0);
		if (!_insertInCurrentScope(stack, sym)) {
			logError(_logger, "Semantic error [02]: duplicate tournament/league name \"%s\".", t->name);
			state->semanticError = true;
			free(sym);
		}
	}

	/* Collect tournament items */
	int teamCount  = 0;
	Format * fmt   = NULL;
	Schedule * sch = NULL;
	List * teams   = NULL; /* char * list when TEAMS_LIST */
	List * zones   = NULL; /* Zone * list (direct zones, not via use) */
	bool hasFormat = false;
	int formatCount = 0;

	for (ListNode * n = t->items->first; n != NULL; n = n->next) {
		TournamentItem * item = n->element;
		switch (item->type) {
			case ITEM_FORMAT:
				formatCount++;
				fmt = item->format;
				hasFormat = true;
				break;
			case ITEM_SCHEDULE:
				sch = item->schedule;
				break;
			case ITEM_TEAMS:
				if (item->teams->type == TEAMS_LIST) {
					teams = item->teams->names;
					teamCount = (int) teams->size;
				}
				else {
					/* [25] teams from tournament */
					Symbol * src = _lookupInStack(stack, item->teams->sourceTournament);
					if (src == NULL) {
						logError(_logger, "Semantic error [25]: teams from unknown tournament \"%s\".", item->teams->sourceTournament);
						state->semanticError = true;
					}
					/* teamCount unknown at static time for selector-based teams */
					teamCount = 0;
				}
				break;
			case ITEM_ZONE:
				if (zones == NULL) zones = createList();
				appendToList(zones, item->zone);
				break;
			case ITEM_USE:
				/* [16] use points/zones template must exist */
				if (_lookupInStack(stack, item->use->name) == NULL) {
					logError(_logger, "Semantic error [16]: use of undeclared template \"%s\".", item->use->name);
					state->semanticError = true;
				}
				break;
			case ITEM_POINTS:
			case ITEM_RESULTS:
			case ITEM_TIEBREAK:
				break;
		}
	}

	/* [26] duplicate format */
	if (formatCount > 1) {
		logError(_logger, "Semantic error [26]: tournament \"%s\" has %d format declarations (only one allowed).", t->name, formatCount);
		state->semanticError = true;
	}

	/* [27] missing format */
	if (!hasFormat) {
		logError(_logger, "Semantic error [27]: tournament \"%s\" has no format declaration.", t->name);
		state->semanticError = true;
	}

	/* [11] schedule period amount = 0 */
	if (sch != NULL && sch->period->type != PERIOD_WEEKDAYS && sch->period->amount == 0) {
		logError(_logger, "Semantic error [11]: schedule period amount is zero in tournament \"%s\".", t->name);
		state->semanticError = true;
	}

	/* [22] full date (year != 0) outside federation */
	if (sch != NULL && !insideFederation && sch->start.year == 0) {
		logError(_logger, "Semantic error [22]: schedule date without year used outside a federation in tournament \"%s\".", t->name);
		state->semanticError = true;
	}

	/* [23] invalid yearless date (e.g. february 30) */
	if (sch != NULL && sch->start.year == 0 && (sch->start.day < 1 || sch->start.day > _daysInMonth(sch->start.month))) {
		logError(_logger, "Semantic error [23]: invalid yearless date --%02d-%02d in tournament \"%s\".", sch->start.month, sch->start.day, t->name);
		state->semanticError = true;
	}

	/* [21] yearless date (year == 0) inside federation */
	if (sch != NULL && insideFederation && sch->start.year != 0) {
		logError(_logger, "Semantic error [21]: schedule date with explicit year used inside a federation in tournament \"%s\".", t->name);
		state->semanticError = true;
	}

	/* Zone validations — only if we have direct zones and a known team count */
	if (zones != NULL && fmt != NULL && teamCount > 0) {
		/* [08] zones forbidden in knockout */
		if (fmt->type == FORMAT_KNOCKOUT) {
			logError(_logger, "Semantic error [08]: zones defined in knockout tournament \"%s\".", t->name);
			state->semanticError = true;
		}
		else {
			/* Build a local name set for zone duplicate check */
			List * zoneNames = createList();
			bool champSeen = false;

			for (ListNode * zn = zones->first; zn != NULL; zn = zn->next) {
				Zone * z = zn->element;

				/* [03] duplicate zone name */
				bool duplicate = false;
				for (ListNode * zn2 = zoneNames->first; zn2 != NULL; zn2 = zn2->next) {
					if (strcmp((char *) zn2->element, z->name) == 0) {
						duplicate = true;
						break;
					}
				}
				if (duplicate) {
					logError(_logger, "Semantic error [03]: duplicate zone name \"%s\" in tournament \"%s\".", z->name, t->name);
					state->semanticError = true;
				}
				else {
					appendToList(zoneNames, z->name);
				}

				/* [06] two champion zones */
				if (z->kind == ZONE_CHAMPION) {
					if (champSeen) {
						logError(_logger, "Semantic error [06]: more than one champion zone in tournament \"%s\".", t->name);
						state->semanticError = true;
					}
					champSeen = true;
				}

				int from, to;
				if (_selectorToRange(z->selector, teamCount, &from, &to)) {
					/* [07] champion zone covers more than one position */
					if (z->kind == ZONE_CHAMPION && (to - from) > 0) {
						logError(_logger, "Semantic error [07]: champion zone in tournament \"%s\" covers more than one position.", t->name);
						state->semanticError = true;
					}

					/* [04] zone out of range */
					if (from < 1 || to > teamCount || from > to) {
						logError(_logger, "Semantic error [04]: zone \"%s\" out of range (positions %d-%d, teams %d) in tournament \"%s\".", z->name, from, to, teamCount, t->name);
						state->semanticError = true;
					}
				}
			}

			/* [05] overlapping zones over the same ranking (pairwise). The champion
			 * is a title, so it may overlap other zones (e.g. it also promotes). */
			if (zones->size > 1) {
				int idx = 0;
				int from[zones->size], to[zones->size];
				Ranking * rankings[zones->size];
				for (ListNode * zn = zones->first; zn != NULL; zn = zn->next) {
					Zone * z = zn->element;
					if (z->kind != ZONE_CHAMPION && _selectorToRange(z->selector, teamCount, &from[idx], &to[idx])) {
						rankings[idx] = z->selector->ranking;
						idx++;
					}
				}
				for (int a = 0; a < idx; ++a) {
					for (int b = a + 1; b < idx; ++b) {
						if (_sameRanking(rankings[a], rankings[b]) && from[a] <= to[b] && from[b] <= to[a]) {
							logError(_logger, "Semantic error [05]: overlapping zones in tournament \"%s\".", t->name);
							state->semanticError = true;
						}
					}
				}
			}

			destroyList(zoneNames, NULL);
		}
	}

	/* Format-groups specific */
	if (fmt != NULL && fmt->type == FORMAT_GROUPS && teamCount > 0) {
		/* [09] teamCount % groupSize != 0 */
		if (fmt->groupSize > 0 && teamCount % fmt->groupSize != 0) {
			logError(_logger, "Semantic error [09]: team count %d is not divisible by group size %d in tournament \"%s\".", teamCount, fmt->groupSize, t->name);
			state->semanticError = true;
		}
		/* [10] advancing >= groupSize */
		if (fmt->advancing >= fmt->groupSize) {
			logError(_logger, "Semantic error [10]: advancing (%d) must be less than group size (%d) in tournament \"%s\".", fmt->advancing, fmt->groupSize, t->name);
			state->semanticError = true;
		}
	}

	/* [01] duplicate team names */
	if (teams != NULL) {
		for (ListNode * a = teams->first; a != NULL; a = a->next) {
			for (ListNode * b = a->next; b != NULL; b = b->next) {
				if (strcmp((char *) a->element, (char *) b->element) == 0) {
					logError(_logger, "Semantic error [01]: duplicate team \"%s\" in tournament \"%s\".", (char *) a->element, t->name);
					state->semanticError = true;
				}
			}
		}
	}

	/* Results: validate referenced teams exist in this tournament's list */
	if (teams != NULL) {
		for (ListNode * n = t->items->first; n != NULL; n = n->next) {
			TournamentItem * item = n->element;
			if (item->type != ITEM_RESULTS) continue;
			for (ListNode * rn = item->results->results->first; rn != NULL; rn = rn->next) {
				Result * r = rn->element;
				bool homeFound = false, awayFound = false;
				for (ListNode * tn = teams->first; tn != NULL; tn = tn->next) {
					if (strcmp((char *) tn->element, r->home) == 0) homeFound = true;
					if (strcmp((char *) tn->element, r->away) == 0) awayFound = true;
				}
				/* [20] result references unknown team */
				if (!homeFound) {
					logError(_logger, "Semantic error [20]: result references unknown team \"%s\" in tournament \"%s\".", r->home, t->name);
					state->semanticError = true;
				}
				if (!awayFound) {
					logError(_logger, "Semantic error [20]: result references unknown team \"%s\" in tournament \"%s\".", r->away, t->name);
					state->semanticError = true;
				}
			}
		}
	}

	if (zones != NULL) destroyList(zones, NULL);
}

/* -------------------------------------------------------------------------- */
/* Template validation                                                         */
/* -------------------------------------------------------------------------- */

static void _analyzeTemplate(Template * tmpl, ScopeStack * stack, CompilerState * state) {
	SymbolKind kind = (tmpl->type == TEMPLATE_POINTS) ? SYM_TEMPLATE_POINTS : SYM_TEMPLATE_ZONES;
	Symbol * sym = _makeSymbol(tmpl->name, kind, 0, 0);
	if (!_insertInCurrentScope(stack, sym)) {
		logError(_logger, "Semantic error [02]: duplicate template name \"%s\".", tmpl->name);
		state->semanticError = true;
		free(sym);
	}
}

/* -------------------------------------------------------------------------- */
/* Federation validation (two-pass)                                            */
/* -------------------------------------------------------------------------- */

static void _analyzeFederation(Federation * fed, ScopeStack * stack, CompilerState * state) {
	/* [02] duplicate federation name at global scope */
	Symbol * fedSym = _makeSymbol(fed->name, SYM_TOURNAMENT, 0, 0);
	if (!_insertInCurrentScope(stack, fedSym)) {
		logError(_logger, "Semantic error [02]: duplicate federation name \"%s\".", fed->name);
		state->semanticError = true;
		free(fedSym);
	}

	_pushScope(stack); /* federation scope */

	/* --- 1st pass: register all names into the federation scope --- */
	int maxTier = 0;
	for (ListNode * n = fed->items->first; n != NULL; n = n->next) {
		FederationItem * item = n->element;
		switch (item->type) {
			case FEDERATION_ITEM_TOURNAMENT: {
				Tournament * t = item->tournament;
				SymbolKind k = (t->type == KIND_LEAGUE) ? SYM_LEAGUE : SYM_TOURNAMENT;
				Symbol * sym = _makeSymbol(t->name, k, t->tier, 0);
				if (!_insertInCurrentScope(stack, sym)) {
					logError(_logger, "Semantic error [02]: duplicate name \"%s\" in federation \"%s\".", t->name, fed->name);
					state->semanticError = true;
					free(sym);
				}
				if (t->type == KIND_LEAGUE && t->tier > maxTier) maxTier = t->tier;
				break;
			}
			case FEDERATION_ITEM_TEMPLATE: {
				Template * tmpl = item->templateDefinition;
				SymbolKind k = (tmpl->type == TEMPLATE_POINTS) ? SYM_TEMPLATE_POINTS : SYM_TEMPLATE_ZONES;
				Symbol * sym = _makeSymbol(tmpl->name, k, 0, 0);
				if (!_insertInCurrentScope(stack, sym)) {
					logError(_logger, "Semantic error [02]: duplicate template name \"%s\" in federation \"%s\".", tmpl->name, fed->name);
					state->semanticError = true;
					free(sym);
				}
				break;
			}
			case FEDERATION_ITEM_CYCLE: {
				Cycle * cy = item->cycle;
				int arity = (int) cy->parameters->size;
				Symbol * sym = _makeSymbol(cy->name, SYM_CYCLE, 0, arity);
				if (!_insertInCurrentScope(stack, sym)) {
					logError(_logger, "Semantic error [02]: duplicate cycle name \"%s\" in federation \"%s\".", cy->name, fed->name);
					state->semanticError = true;
					free(sym);
				}
				break;
			}
			case FEDERATION_ITEM_CALENDAR:
			case FEDERATION_ITEM_MOVEMENT:
				break;
		}
	}

	/* --- 2nd pass: deep validation with full scope --- */
	for (ListNode * n = fed->items->first; n != NULL; n = n->next) {
		FederationItem * item = n->element;
		switch (item->type) {
			case FEDERATION_ITEM_TOURNAMENT:
				_analyzeTournament(item->tournament, stack, state, true, false);
				break;
			case FEDERATION_ITEM_TEMPLATE:
				/* already registered; no further rules */
				break;
			case FEDERATION_ITEM_CYCLE: {
				Cycle * cy = item->cycle;
				_pushScope(stack); /* cycle scope */
				/* Register parameters as year variables */
				for (ListNode * p = cy->parameters->first; p != NULL; p = p->next) {
					Symbol * vsym = _makeSymbol((char *) p->element, SYM_YEAR_VARIABLE, 0, 0);
					_insertInCurrentScope(stack, vsym);
				}
				_analyzeStatements(cy->body, stack, state, true);
				_popScope(stack);
				break;
			}
			case FEDERATION_ITEM_MOVEMENT:
				_analyzeMovement(item->movement, stack, state, maxTier);
				break;
			case FEDERATION_ITEM_CALENDAR:
				_analyzeCalendar(item->calendar, stack, state);
				break;
		}
	}

	_popScope(stack); /* federation scope */
}

/* -------------------------------------------------------------------------- */
/* Movement validation                                                         */
/* -------------------------------------------------------------------------- */

static void _analyzeMovement(Movement * mov, ScopeStack * stack, CompilerState * state, int maxTier) {
	for (ListNode * n = mov->rules->first; n != NULL; n = n->next) {
		MovementRule * rule = n->element;

		/* [19] referenced leagues must exist in scope */
		Symbol * fromSym = _lookupInStack(stack, rule->fromLeague);
		if (fromSym == NULL) {
			logError(_logger, "Semantic error [19]: movement references unknown league \"%s\".", rule->fromLeague);
			state->semanticError = true;
		}
		Symbol * toSym = _lookupInStack(stack, rule->toLeague);
		if (toSym == NULL) {
			logError(_logger, "Semantic error [19]: movement references unknown league \"%s\".", rule->toLeague);
			state->semanticError = true;
		}

		/* [17] promote from tier 1 (minimum tier) is invalid */
		if (rule->direction == MOVEMENT_PROMOTE && fromSym != NULL && fromSym->kind == SYM_LEAGUE && fromSym->tier == 1) {
			logError(_logger, "Semantic error [17]: cannot promote from tier-1 league \"%s\".", rule->fromLeague);
			state->semanticError = true;
		}

		/* [18] relegate from the highest tier (maxTier) is invalid */
		if (rule->direction == MOVEMENT_RELEGATE && fromSym != NULL && fromSym->kind == SYM_LEAGUE && fromSym->tier == maxTier) {
			logError(_logger, "Semantic error [18]: cannot relegate from the highest-tier league \"%s\" (tier %d).", rule->fromLeague, maxTier);
			state->semanticError = true;
		}
	}
}

/* -------------------------------------------------------------------------- */
/* Calendar validation                                                         */
/* -------------------------------------------------------------------------- */

static void _analyzeCalendar(Calendar * cal, ScopeStack * stack, CompilerState * state) {
	/* [13] fromYear > toYear */
	if (cal->fromYear > cal->toYear) {
		logError(_logger, "Semantic error [13]: calendar fromYear (%d) > toYear (%d).", cal->fromYear, cal->toYear);
		state->semanticError = true;
	}
	_analyzeStatements(cal->body, stack, state, false);
}

/* -------------------------------------------------------------------------- */
/* Statement and expression analysis                                           */
/* -------------------------------------------------------------------------- */

static void _analyzeStatements(List * stmts, ScopeStack * stack, CompilerState * state, bool insideLoop) {
	for (ListNode * n = stmts->first; n != NULL; n = n->next) {
		_analyzeStatement(n->element, stack, state, insideLoop);
	}
}

static void _analyzeStatement(Statement * stmt, ScopeStack * stack, CompilerState * state, bool insideLoop) {
	switch (stmt->type) {
		case STATEMENT_HOST: {
			/* [12] referenced tournament must exist */
			if (_lookupInStack(stack, stmt->host->tournament) == NULL) {
				logError(_logger, "Semantic error [12]: host references unknown tournament \"%s\".", stmt->host->tournament);
				state->semanticError = true;
			}
			/* [28] HOST_CURRENT_YEAR outside a loop */
			if (stmt->host->timing == HOST_CURRENT_YEAR && !insideLoop) {
				logError(_logger, "Semantic error [28]: 'host ... in current year' used outside an 'every' loop.");
				state->semanticError = true;
			}
			/* validate year expression if explicit */
			if (stmt->host->timing == HOST_EXPLICIT_YEAR && stmt->host->year != NULL) {
				_analyzeYearExpression(stmt->host->year, stack, state);
			}
			break;
		}
		case STATEMENT_CALL: {
			Call * call = stmt->call;
			Symbol * sym = _lookupInStack(stack, call->cycle);
			/* [15] unknown cycle */
			if (sym == NULL) {
				logError(_logger, "Semantic error [15]: call to undeclared cycle \"%s\".", call->cycle);
				state->semanticError = true;
			}
			else if (sym->kind == SYM_CYCLE) {
				/* [14] arity mismatch */
				int provided = (int) call->arguments->size;
				if (provided != sym->arity) {
					logError(_logger, "Semantic error [14]: cycle \"%s\" expects %d argument(s) but got %d.", call->cycle, sym->arity, provided);
					state->semanticError = true;
				}
			}
			/* validate argument expressions */
			for (ListNode * a = call->arguments->first; a != NULL; a = a->next) {
				_analyzeYearExpression(a->element, stack, state);
			}
			break;
		}
		case STATEMENT_EVERY: {
			Every * ev = stmt->every;
			if (ev->start != NULL) _analyzeYearExpression(ev->start, stack, state);
			_pushScope(stack);
			/* register loop variable */
			if (ev->variable != NULL) {
				Symbol * vsym = _makeSymbol(ev->variable, SYM_YEAR_VARIABLE, 0, 0);
				_insertInCurrentScope(stack, vsym);
			}
			_analyzeStatements(ev->body, stack, state, true);
			_popScope(stack);
			if (ev->otherwise != NULL) {
				_analyzeStatements(ev->otherwise, stack, state, true);
			}
			break;
		}
		case STATEMENT_IF: {
			If * cond = stmt->conditional;
			_analyzeCondition(cond->condition, stack, state);
			_analyzeStatements(cond->thenBody, stack, state, insideLoop);
			if (cond->elseBody != NULL) {
				_analyzeStatements(cond->elseBody, stack, state, insideLoop);
			}
			break;
		}
	}
}

static void _analyzeYearExpression(YearExpression * expr, ScopeStack * stack, CompilerState * state) {
	switch (expr->type) {
		case YEAR_ADDITION:
		case YEAR_SUBTRACTION:
			_analyzeYearExpression(expr->leftExpression, stack, state);
			_analyzeYearExpression(expr->rightExpression, stack, state);
			break;
		case YEAR_LITERAL:
			break;
		case YEAR_VARIABLE:
			/* [24] variable used without declaration */
			if (_lookupInStack(stack, expr->variable) == NULL) {
				logError(_logger, "Semantic error [24]: year variable \"%s\" used without declaration.", expr->variable);
				state->semanticError = true;
			}
			break;
	}
}

static void _analyzeCondition(Condition * cond, ScopeStack * stack, CompilerState * state) {
	switch (cond->type) {
		case CONDITION_AND:
		case CONDITION_OR:
			_analyzeCondition(cond->leftCondition, stack, state);
			_analyzeCondition(cond->rightCondition, stack, state);
			break;
		case CONDITION_NOT:
			_analyzeCondition(cond->operand, stack, state);
			break;
		case CONDITION_COMPARISON:
			_analyzeYearExpression(cond->leftYear, stack, state);
			_analyzeYearExpression(cond->rightYear, stack, state);
			break;
	}
}

/* ========================================================================== */
/* PUBLIC ENTRY POINT                                                          */
/* ========================================================================== */

CompilationStatus executeSemanticAnalysis(CompilerState * compilerState) {
	logDebugging(_logger, "Running semantic analysis...");
	Program * program = compilerState->abstractSyntaxtTree;
	if (program == NULL) {
		logError(_logger, "Semantic analysis received NULL program.");
		return FAILED;
	}

	ScopeStack stack;
	stack.top = -1;
	for (int i = 0; i < MAX_SCOPE_DEPTH; ++i) stack.scopes[i] = NULL;

	_analyzeProgram(program, &stack, compilerState);

	if (compilerState->semanticError) {
		logDebugging(_logger, "Semantic analysis finished with errors.");
		return FAILED;
	}
	logDebugging(_logger, "Semantic analysis finished successfully.");
	return SUCCEEDED;
}
