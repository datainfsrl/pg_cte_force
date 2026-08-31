#include "postgres.h"

#include "fmgr.h"
#include "utils/guc.h"

#include "nodes/parsenodes.h"
#include "nodes/nodeFuncs.h"
#include "nodes/queryjumble.h"

#include "parser/analyze.h"
#include "parser/parse_node.h"

PG_MODULE_MAGIC;

//Protoypes for _PG_init and _PG_fini.
void _PG_init(void);
void _PG_fini(void);


/*
 * Operating modes.
 */
typedef enum
{
    CTE_FORCE_DEFAULT = 0,
    CTE_FORCE_MATERIALIZED,
    CTE_FORCE_NOT_MATERIALIZED
} CteForceMode;


/*
 * Current GUC value.
 */
static int cte_force_mode = CTE_FORCE_DEFAULT;


/*
 * Values accepted by:
 *
 * SET pg_cte_force.mode = '...';
 */
static const struct config_enum_entry cte_mode_options[] =
{
    {"default",          CTE_FORCE_DEFAULT,          false},
    {"materialized",     CTE_FORCE_MATERIALIZED,     false},
    {"not_materialized", CTE_FORCE_NOT_MATERIALIZED, false},
    {NULL, 0, false}
};


/*
 * Hook previously installed by another extension.
 */
static post_parse_analyze_hook_type prev_post_parse_analyze_hook = NULL;


/*
 * Query tree walker.
 *
 *  - subqueries present in RTEs
 *  - CTEs contained in cteList
 *  - SubLinks
 *  - expressions
 *
 * but does NOT pass RangeTblEntry directly to the callback.
 */
static bool
force_cte_materialization_walker(Node *node, void *context)
{
    if (node == NULL)
        return false;

    /*
     * Query is a special case.
     *
     * It must not be passed directly to
     * expression_tree_walker().
     */


    if (IsA(node, Query))
    {
        Query *query = (Query *) node;
        ListCell *lc;

        /*
         * Process all CTEs defined at this level.
         */
        foreach(lc, query->cteList)
        {
            CommonTableExpr *cte;

            cte = lfirst_node(CommonTableExpr, lc);

            /*
             * If the user explicitly wrote:
             *
             *   AS MATERIALIZED
             *
             * or:
             *
             *   AS NOT MATERIALIZED
             *
             * we don't change anything.
             */
            if (cte->ctematerialized != CTEMaterializeDefault)
                continue;

            switch (cte_force_mode)
            {
                case CTE_FORCE_MATERIALIZED:

                    cte->ctematerialized = CTEMaterializeAlways;

                    break;

                case CTE_FORCE_NOT_MATERIALIZED:

                    cte->ctematerialized = CTEMaterializeNever;

                    break;

                case CTE_FORCE_DEFAULT:
                default:

                    /*
                     * No change.
                     */
                    break;
            }
        }


        return query_tree_walker(
            query,
            force_cte_materialization_walker,
            context,
            0
        );
    }

    /*
     * Other nodes can be traversed normally.
     */
    return expression_tree_walker(
        node,
        force_cte_materialization_walker,
        context
    );
}


/*
 * Internal entry point.
 */
static void
force_cte_materialization(Query *query)
{
    if (query == NULL)
        return;

    if (cte_force_mode == CTE_FORCE_DEFAULT)
        return;

    force_cte_materialization_walker(
        (Node *) query,
        NULL
    );
}


/*
 * post_parse_analyze_hook
 *
 * PostgreSQL 19 made JumbleState const.
 */
#if PG_VERSION_NUM >= 190000

static void
pg_cte_force_post_parse_analyze(
    ParseState *pstate,
    Query *query,
    const JumbleState *jstate
)

#elif PG_VERSION_NUM >= 140000

static void
pg_cte_force_post_parse_analyze(
    ParseState *pstate,
    Query *query,
    JumbleState *jstate
)

#else

static void
pg_cte_force_post_parse_analyze(
    ParseState *pstate,
    Query *query
)

#endif

{
    /*
     * First, apply our modification.
     */
    force_cte_materialization(query);

    /*
     * Then call any previously installed hook.
     */
    if (prev_post_parse_analyze_hook)
    {
#if PG_VERSION_NUM >= 140000

        prev_post_parse_analyze_hook(
            pstate,
            query,
            jstate
        );

#else

        prev_post_parse_analyze_hook(
            pstate,
            query
        );

#endif
    }
}


/*
 * Extension initialization.
 */
void
_PG_init(void)
{
    DefineCustomEnumVariable(
        "pg_cte_force.mode",

        "Globally sets the behavior of CTEs "
        "without a MATERIALIZED/NOT MATERIALIZED annotation.",

        "Allowed values: default, materialized, not_materialized.",

        &cte_force_mode,

        CTE_FORCE_DEFAULT,

        cte_mode_options,

        PGC_USERSET,

        0,

        NULL,
        NULL,
        NULL
    );


    /*
     * Save any existing hook.
     */
    prev_post_parse_analyze_hook =
        post_parse_analyze_hook;

    /*
     * Install our hook.
     */
    post_parse_analyze_hook =
        pg_cte_force_post_parse_analyze;
}


/*
 * Extension teardown.
 */
void
_PG_fini(void)
{
    /*
     * Only restore the previous hook if we are still
     * the ones installed.
     *
     * This is slightly more defensive than simply doing:
     *
     * post_parse_analyze_hook =
     *     prev_post_parse_analyze_hook;
     */
    if (post_parse_analyze_hook ==
        pg_cte_force_post_parse_analyze)
    {
        post_parse_analyze_hook =
            prev_post_parse_analyze_hook;
    }
}
