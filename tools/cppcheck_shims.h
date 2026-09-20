#pragma once

// Lint-only shim, injected by `make linter` with --include=. cppcheck runs without the
// firmware SDK, so with_view_model() is an unknown function-like macro whose block argument
// breaks the parse — and cppcheck then abandons the WHOLE translation unit in silence, which
// reads like coverage the file does not have. Expanding it here keeps src/views/ genuinely
// analysed. The two helpers stand in for view_get_model/view_commit_model so the model is
// neither null nor uninitialized to the analyser, and the view is still seen as written to.
void *wc_lint_view_model(void *view);
void wc_lint_view_commit(void *view, int update);

#undef with_view_model
#define with_view_model(view, type, code, update)                                                  \
    do {                                                                                           \
        type = wc_lint_view_model(view);                                                           \
        code wc_lint_view_commit(view, (update));                                                  \
    } while (0)
