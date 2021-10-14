#include "libpq-fe.h"
#include <cstdlib>
static void
exit_nicely(PGconn* conn)
{
    PQfinish(conn);
    std::exit(1);
}
