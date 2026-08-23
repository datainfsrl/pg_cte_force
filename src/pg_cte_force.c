#include "postgres.h"

#include "fmgr.h"
#include "utils/guc.h"

#include "nodes/parsenodes.h"
#include "nodes/nodeFuncs.h"
#include "nodes/queryjumble.h"

#include "parser/analyze.h"
#include "parser/parse_node.h"

PG_MODULE_MAGIC;


/*
 * Modalità di funzionamento.
 */
typedef enum
{
    CTE_FORCE_DEFAULT = 0,
    CTE_FORCE_MATERIALIZED,
    CTE_FORCE_NOT_MATERIALIZED
} CteForceMode;


/*
 * GUC corrente.
 */
static int cte_force_mode = CTE_FORCE_DEFAULT;


/*
 * Valori accettati da:
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
 * Hook precedentemente installato da un'altra estensione.
 */
static post_parse_analyze_hook_type prev_post_parse_analyze_hook = NULL;


/*
 * Walker dell'albero Query.
 *
 * IMPORTANTE:
 *
 * Non utilizziamo QTW_EXAMINE_RTES_BEFORE.
 *
 * Con flags = 0 query_tree_walker() continua comunque a visitare:
 *
 *  - subquery presenti nelle RTE
 *  - CTE contenute nelle cteList
 *  - SubLink
 *  - espressioni
 *
 * ma NON passa RangeTblEntry direttamente al callback.
 */
static bool
force_cte_materialization_walker(Node *node, void *context)
{
    if (node == NULL)
        return false;

    /*
     * Query è un caso speciale.
     *
     * Non deve essere passata direttamente a
     * expression_tree_walker().
     */


    if (IsA(node, Query))
    {
        Query *query = (Query *) node;
        ListCell *lc;

        /*
         * Analizza tutte le CTE definite a questo livello.
         */
        foreach(lc, query->cteList)
        {
            CommonTableExpr *cte;

            cte = lfirst_node(CommonTableExpr, lc);

            /*
             * Se l'utente ha scritto esplicitamente:
             *
             *   AS MATERIALIZED
             *
             * oppure:
             *
             *   AS NOT MATERIALIZED
             *
             * non modifichiamo nulla.
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
                     * Nessuna modifica.
                     */
                    break;
            }
        }

        /*
         * flags = 0 è intenzionale.
         *
         * query_tree_walker() attraversa comunque le subquery
         * delle RTE, salvo specificare esplicitamente
         * QTW_IGNORE_RT_SUBQUERIES.
         *
         * Attraversa inoltre le query contenute nelle CTE salvo
         * QTW_IGNORE_CTE_SUBQUERIES.
         */
        return query_tree_walker(
            query,
            force_cte_materialization_walker,
            context,
            0
        );
    }

    /*
     * Gli altri nodi possono essere attraversati normalmente.
     */
    return expression_tree_walker(
        node,
        force_cte_materialization_walker,
        context
    );
}


/*
 * Entry point interno.
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
 * PostgreSQL 19 ha reso const il JumbleState.
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
     * Prima applichiamo la nostra modifica.
     */
    force_cte_materialization(query);

    /*
     * Poi richiamiamo l'eventuale hook precedentemente
     * installato.
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
 * Inizializzazione estensione.
 */
void
_PG_init(void)
{
    DefineCustomEnumVariable(
        "pg_cte_force.mode",

        "Imposta globalmente il comportamento delle CTE "
        "senza annotazione MATERIALIZED/NOT MATERIALIZED.",

        "Valori ammessi: default, materialized, not_materialized.",

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
     * Salva l'eventuale hook esistente.
     */
    prev_post_parse_analyze_hook =
        post_parse_analyze_hook;

    /*
     * Installa il nostro hook.
     */
    post_parse_analyze_hook =
        pg_cte_force_post_parse_analyze;
}


/*
 * Rimozione estensione.
 */
void
_PG_fini(void)
{
    /*
     * Ripristiniamo l'hook precedente SOLO se siamo
     * ancora noi ad essere installati.
     *
     * È leggermente più difensivo rispetto a:
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