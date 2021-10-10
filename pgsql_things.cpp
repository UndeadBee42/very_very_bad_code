#include "libpq-fe.h"
static void
exit_nicely(PGconn* conn)
{
    PQfinish(conn);
    exit(1);
}
