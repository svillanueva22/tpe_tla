#ifndef SEMANTIC_ANALYZER_HEADER
#define SEMANTIC_ANALYZER_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/language/List.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdbool.h>

/** Initialize module's internal state. */
ModuleDestructor initializeSemanticAnalyzerModule();

/**
 * Runs all semantic validations over the AST stored in compilerState.
 * Sets compilerState->semanticError = true for every violation found.
 * Returns FAILED if any error was found, SUCCEEDED otherwise.
 */
CompilationStatus executeSemanticAnalysis(CompilerState * compilerState);

#endif
