#ifndef MOON_MOON_HITL_H
#define MOON_MOON_HITL_H

/* DOS-owned integration boundary for the portable TEST COCKPIT. */

typedef enum MoonHitlResult {
    MOON_HITL_OK = 0,
    MOON_HITL_ERR_ARGUMENT,
    MOON_HITL_ERR_PATH,
    MOON_HITL_ERR_PLAN,
    MOON_HITL_ERR_AUTO,
    MOON_HITL_ERR_JOURNAL,
    MOON_HITL_ERR_RUNTIME,
    MOON_HITL_ERR_INPUT,
    MOON_HITL_ERR_CGUI,
    MOON_HITL_ERR_PRESENT,
    MOON_HITL_ERR_COMMIT,
    MOON_HITL_ERR_SHUTDOWN,
    MOON_HITL_ERR_EVIDENCE
} MoonHitlResult;

/*
 * These entry points establish immutable authority for the complete session.
 * The ordinary path accepts only live DOS input.  The bounded smoke path may
 * drive deterministic synthetic input but can never append MANUAL authority.
 */
MoonHitlResult moon_hitl_run_live_dos(const char *plan_path);
MoonHitlResult moon_hitl_run_synthetic_smoke(const char *plan_path);

const char *moon_hitl_result_name(MoonHitlResult result);

#endif
