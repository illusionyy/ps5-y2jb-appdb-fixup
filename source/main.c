#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#define DB_PATH "/system_data/priv/mms/appinfo.db"
#define TITLE_ID "PPSA01650"
#define NEW_URI "http://127.0.0.4"
#define NEW_CONTENT_VERSION "99.999.999"

#if !defined(dstr)
#define dstr(s) #s
#endif
#if !defined(xstr)
#define xstr(s) dstr(s)
#endif

#if !defined(FILE_FUNC_LINE)
#define FILE_FUNC_LINE __FILE__ \
    ":"__FUNCSIG__              \
    ":" xstr(__LINE__)
#endif

#define close_and_exit(db, c)                                   \
    if (db)                                                     \
    {                                                           \
        sqlite3_close(db);                                      \
    }                                                           \
    notify("close_and_exit called from\n" FILE_FUNC_LINE "\n"); \
    return c

#if !defined(_countof)
#define _countof(a) (sizeof(a) / sizeof(*a))
#endif

#if !defined(_countof_1)
#define _countof_1(a) (_countof(a) - 1)
#endif

static void notify(const char* fmt, ...)
{
    struct notify_request
    {
        char useless1[45];
        char message[1024];
        char useless2[2051];
    } buf = {};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf.message, _countof_1(buf.message), fmt, args);
    va_end(args);
    size_t len = strlen(buf.message);
    // trim newline
    while (len > 0 && buf.message[len - 1] == '\n')
    {
        buf.message[--len] = '\0';
    }
    extern int sceKernelSendNotificationRequest(const size_t, const struct notify_request*, const size_t, const int);
    puts(buf.message);
    sceKernelSendNotificationRequest(0, &buf, sizeof(buf), 0);
}

int main(void)
{
    sqlite3* db = NULL;
    sqlite3_stmt* stmt = NULL;
    int rc = 0;

    FILE* f = fopen(DB_PATH, "r");
    if (!f)
    {
        fprintf(stderr, "Error: %s file not found!\n", DB_PATH);
        return 1;
    }
    fclose(f);

    rc = sqlite3_open(DB_PATH, &db);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Error opening DB: %s\n", sqlite3_errmsg(db));
        close_and_exit(db, 1);
    }

    const char* sel_sql =
        "SELECT key, val FROM tbl_appinfo "
        "WHERE titleId = ? "
        "AND key IN ('CONTENT_VERSION', 'VERSION_FILE_URI')";

    rc = sqlite3_prepare_v2(db, sel_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(db));
        close_and_exit(db, 1);
    }
    sqlite3_bind_text(stmt, 1, TITLE_ID, -1, SQLITE_STATIC);

    char found_keys[2][512 + 1] = {0};
    size_t count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < 2)
    {
        strncpy(found_keys[count], (const char*)sqlite3_column_text(stmt, 0), _countof_1(found_keys[count]));
        count++;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (count != 2)
    {
        fprintf(stderr, "Error: Expected 2 keys but found %ld\n", count);
        fprintf(stderr, "Required keys: CONTENT_VERSION, VERSION_FILE_URI\n");
        fprintf(stderr, "Found keys:");
        for (size_t i = 0; i < count; i++)
        {
            fprintf(stderr, " %s", found_keys[i]);
        }
        fprintf(stderr, "\n");
        close_and_exit(db, 1);
    }

    notify("All required keys found. Proceeding with updates...\n\n");

    /* Show before values */
    notify("Before update:\n");
    rc = sqlite3_prepare_v2(db, sel_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(db));
        close_and_exit(db, 1);
    }
    sqlite3_bind_text(stmt, 1, TITLE_ID, -1, SQLITE_STATIC);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        notify("%s: %s\n",
               sqlite3_column_text(stmt, 0),
               sqlite3_column_text(stmt, 1));
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    /* Update CONTENT_VERSION */
    const char* upd_cv_sql =
        "UPDATE tbl_appinfo SET val = ? "
        "WHERE titleId = ? AND key = 'CONTENT_VERSION'";

    rc = sqlite3_prepare_v2(db, upd_cv_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(db));
        close_and_exit(db, 1);
    }
    sqlite3_bind_text(stmt, 1, NEW_CONTENT_VERSION, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, TITLE_ID, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "Update error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        close_and_exit(db, 1);
    }
    printf("Updated CONTENT_VERSION (rows affected: %d)\n", sqlite3_changes(db));
    sqlite3_finalize(stmt);
    stmt = NULL;

    /* Update VERSION_FILE_URI */
    const char* upd_uri_sql =
        "UPDATE tbl_appinfo SET val = ? "
        "WHERE titleId = ? AND key = 'VERSION_FILE_URI'";

    rc = sqlite3_prepare_v2(db, upd_uri_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(db));
        close_and_exit(db, 1);
    }
    sqlite3_bind_text(stmt, 1, NEW_URI, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, TITLE_ID, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "Update error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        close_and_exit(db, 1);
    }
    printf("Updated VERSION_FILE_URI (rows affected: %d)\n", sqlite3_changes(db));
    sqlite3_finalize(stmt);
    stmt = NULL;

    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);

    notify("Verifying changes...\n");

    rc = sqlite3_prepare_v2(db, sel_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(db));
        close_and_exit(db, 1);
    }
    sqlite3_bind_text(stmt, 1, TITLE_ID, -1, SQLITE_STATIC);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        notify("%s: %s\n",
               sqlite3_column_text(stmt, 0),
               sqlite3_column_text(stmt, 1));
    }
    sqlite3_finalize(stmt);

    sqlite3_close(db);

    notify("Changes saved to %s\n", DB_PATH);
    return 0;
}
